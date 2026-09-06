#include "common.h"
#include <stdint.h>

extern EditorConfig E;

void paste_from_clipboard() {
    editor_set_status_message("Use terminal paste (Ctrl+Shift+V or right-click)");
}

static char *base64_encode(const unsigned char *data, size_t input_length) {
    static const char encoding_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t output_length = 4 * ((input_length + 2) / 3);
    char *encoded_data = malloc(output_length + 1);
    if (!encoded_data) return NULL;

    size_t i = 0, j = 0;
    while (i < input_length) {
        uint32_t octet_a = i < input_length ? data[i++] : 0;
        uint32_t octet_b = i < input_length ? data[i++] : 0;
        uint32_t octet_c = i < input_length ? data[i++] : 0;

        uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;

        encoded_data[j++] = encoding_table[(triple >> 18) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 12) & 0x3F];
        encoded_data[j++] = (i > input_length + 1) ? '=' : encoding_table[(triple >> 6) & 0x3F];
        encoded_data[j++] = (i > input_length) ? '=' : encoding_table[triple & 0x3F];
    }
    encoded_data[output_length] = '\0';
    return encoded_data;
}

static void copy_via_osc52(const char *text, size_t len) {
    char *b64 = base64_encode((const unsigned char *)text, len);
    if (!b64) return;

    FILE *tty = fopen("/dev/tty", "w");
    if (tty) {
        fprintf(tty, "\033]52;c;%s\007", b64);
        fflush(tty);
        fclose(tty);
    } else {
        printf("\033]52;c;%s\007", b64);
        fflush(stdout);
    }
    free(b64);
}

static bool copy_via_command(const char *cmd, char *const argv[], const char *text, size_t len) {
    int pipefd[2];
    if (pipe(pipefd) == -1) return false;

    pid_t pid = fork();
    if (pid == -1) {
        close(pipefd[0]);
        close(pipefd[1]);
        return false;
    }

    if (pid == 0) {
        close(pipefd[1]);
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);

        execvp(cmd, argv);
        _exit(1);
    } else {
        close(pipefd[0]);
        ssize_t written = 0;
        while ((size_t)written < len) {
            ssize_t n = write(pipefd[1], text + written, len - written);
            if (n <= 0) break;
            written += n;
        }
        close(pipefd[1]);

        int status;
        waitpid(pid, &status, 0);
        return (WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }
}

void editor_copy_selection_to_clipboard() {
    if (!E.selection_active) {
        editor_set_status_message("No text selected to copy.");
        return;
    }

    int sel_min_cy = E.selection_start_cy;
    int sel_min_cx = E.selection_start_cx;
    int sel_max_cy = E.selection_end_cy;
    int sel_max_cx = E.selection_end_cx;

    if (sel_min_cy > sel_max_cy || (sel_min_cy == sel_max_cy && sel_min_cx > sel_max_cx)) {
        int temp_cy = sel_min_cy;
        int temp_cx = sel_min_cx;
        sel_min_cy = sel_max_cy;
        sel_min_cx = sel_max_cx;
        sel_max_cy = temp_cy;
        sel_max_cx = temp_cx;
    }

    size_t total_len = 0;
    for (int r = sel_min_cy; r <= sel_max_cy; r++) {
        if (r < 0 || r >= E.num_lines) continue;

        EditorLine *line = &E.lines[r];
        int start_col = (r == sel_min_cy) ? sel_min_cx : 0;
        int end_col = (r == sel_max_cy) ? sel_max_cx : (int)line->len;

        if (end_col > (int)line->len) end_col = (int)line->len;
        if (start_col < 0) start_col = 0;

        if (end_col > start_col) {
            total_len += (size_t)(end_col - start_col);
        }
        if (r < sel_max_cy) {
            total_len += 1;
        }
    }

    if (total_len == 0) {
        editor_set_status_message("No text selected to copy.");
        return;
    }

    char *selected_text = malloc(total_len + 1);
    if (selected_text == NULL) {
        editor_set_status_message("Copy error: Out of memory for selected text.");
        return;
    }
    selected_text[0] = '\0';
    size_t current_offset = 0;

    for (int r = sel_min_cy; r <= sel_max_cy; r++) {
        if (r < 0 || r >= E.num_lines) continue;

        EditorLine *line = &E.lines[r];
        int start_col = (r == sel_min_cy) ? sel_min_cx : 0;
        int end_col = (r == sel_max_cy) ? sel_max_cx : (int)line->len;

        if (end_col > (int)line->len) end_col = (int)line->len;
        if (start_col < 0) start_col = 0;

        if (end_col > start_col) {
            size_t segment_len = (size_t)(end_col - start_col);
            memcpy(selected_text + current_offset, line->text + start_col, segment_len);
            current_offset += segment_len;
        }
        if (r < sel_max_cy) {
            selected_text[current_offset++] = '\n';
        }
    }
    selected_text[current_offset] = '\0';

    // 1. Always emit OSC 52 escape sequence (works over SSH, Windows Terminal, iTerm2, Alacritty, Kitty, etc.)
    copy_via_osc52(selected_text, current_offset);

    // 2. Also copy to native OS clipboard tool if present (Wayland, X11, macOS, Windows)
    bool tool_copied = false;
    const char *tool_name = NULL;

    // Check Wayland
    if (getenv("WAYLAND_DISPLAY") != NULL || (getenv("XDG_SESSION_TYPE") && strcmp(getenv("XDG_SESSION_TYPE"), "wayland") == 0)) {
        if (access("/usr/bin/wl-copy", X_OK) == 0 || access("/bin/wl-copy", X_OK) == 0) {
            char *argv[] = {"wl-copy", NULL};
            tool_copied = copy_via_command("wl-copy", argv, selected_text, current_offset);
            if (tool_copied) tool_name = "wl-copy";
        }
    }

    // Check X11
    if (!tool_copied && getenv("DISPLAY") != NULL) {
        if (access("/usr/bin/xclip", X_OK) == 0 || access("/bin/xclip", X_OK) == 0) {
            char *argv[] = {"xclip", "-selection", "clipboard", NULL};
            tool_copied = copy_via_command("xclip", argv, selected_text, current_offset);
            if (tool_copied) tool_name = "xclip";
        } else if (access("/usr/bin/xsel", X_OK) == 0 || access("/bin/xsel", X_OK) == 0) {
            char *argv[] = {"xsel", "--clipboard", "--input", NULL};
            tool_copied = copy_via_command("xsel", argv, selected_text, current_offset);
            if (tool_copied) tool_name = "xsel";
        }
    }

    // Check macOS
    if (!tool_copied) {
        if (access("/usr/bin/pbcopy", X_OK) == 0 || access("/opt/homebrew/bin/pbcopy", X_OK) == 0 || access("/usr/local/bin/pbcopy", X_OK) == 0) {
            char *argv[] = {"pbcopy", NULL};
            tool_copied = copy_via_command("pbcopy", argv, selected_text, current_offset);
            if (tool_copied) tool_name = "pbcopy";
        }
    }

    // Check Windows (clip.exe)
    if (!tool_copied) {
        if (access("/c/Windows/System32/clip.exe", X_OK) == 0 || access("/mnt/c/Windows/System32/clip.exe", X_OK) == 0) {
            char *clip_path = (access("/mnt/c/Windows/System32/clip.exe", X_OK) == 0) ? "/mnt/c/Windows/System32/clip.exe" : "/c/Windows/System32/clip.exe";
            char *argv[] = {clip_path, NULL};
            tool_copied = copy_via_command(clip_path, argv, selected_text, current_offset);
            if (tool_copied) tool_name = "clip.exe";
        }
    }

    if (tool_copied) {
        editor_set_status_message("Copied %zu bytes (clipboard tool: %s + terminal).", current_offset, tool_name);
    } else {
        editor_set_status_message("Copied %zu bytes to clipboard.", current_offset);
    }

    free(selected_text);
    selected_text = NULL;
    E.selection_active = false;
    for (int i = 0; i < E.num_lines; i++) {
        editor_update_syntax(i);
    }
    editor_refresh_screen();
}

void editor_select_all() {
    if (E.num_lines == 0) return;
    E.selection_active = true;
    E.selection_start_cy = 0;
    E.selection_start_cx = 0;
    E.selection_end_cy = E.num_lines - 1;
    E.selection_end_cx = (int)E.lines[E.num_lines - 1].len;
    editor_set_status_message("All text selected. Press Ctrl+K to copy.");
    for (int i = 0; i < E.num_lines; i++) {
        editor_update_syntax(i);
    }
    editor_refresh_screen();
}
