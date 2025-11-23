#include"common.h"
extern EditorConfig E;
void paste_from_clipboard() {
    editor_set_status_message("Use terminal paste (Ctrl+Shift+V or right-click)");
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

    int pipefd[2];
    pid_t pid;
    char *clipboard_tool = NULL;
    char *argv[4];

    // kinda almost lost it here with the copying, carefully mess with it T-T
    bool tool_found = false;
    if (getenv("XDG_SESSION_TYPE") != NULL && strcmp(getenv("XDG_SESSION_TYPE"), "wayland") == 0) {
        if (access("/usr/bin/wl-copy", X_OK) == 0) {
            clipboard_tool = "wl-copy";
            argv[0] = "wl-copy";
            argv[1] = NULL;
            argv[2] = NULL;
            argv[3] = NULL;
            tool_found = true;
        } else if (access("/bin/wl-copy", X_OK) == 0) {
            clipboard_tool = "wl-copy";
            argv[0] = "wl-copy";
            argv[1] = NULL;
            argv[2] = NULL;
            argv[3] = NULL;
            tool_found = true;
        }
    }
    if (!tool_found) {
        if (access("/usr/bin/pbcopy", X_OK) == 0) {
            clipboard_tool = "pbcopy";
            argv[0] = "pbcopy";
            argv[1] = NULL;
            argv[2] = NULL;
            argv[3] = NULL;
            tool_found = true;
        }
        else if (access("/usr/bin/xclip", X_OK) == 0) {
            clipboard_tool = "xclip";
            argv[0] = "xclip";
            argv[1] = "-selection";
            argv[2] = "clipboard";
            argv[3] = NULL;
            tool_found = true;
        }
        else if (access("/bin/xclip", X_OK) == 0) {
            clipboard_tool = "xclip";
            argv[0] = "xclip";
            argv[1] = "-selection";
            argv[2] = "clipboard";
            argv[3] = NULL;
            tool_found = true;
        }
    }

    if (!tool_found) {
        if (access("/opt/homebrew/bin/pbcopy", X_OK) == 0) {
            clipboard_tool = "pbcopy";
            argv[0] = "/opt/homebrew/bin/pbcopy";
            argv[1] = NULL;
            argv[2] = NULL;
            argv[3] = NULL;
            tool_found = true;
        }
        else if (access("/usr/local/bin/pbcopy", X_OK) == 0) {
            clipboard_tool = "pbcopy";
            argv[0] = "/usr/local/bin/pbcopy";
            argv[1] = NULL;
            argv[2] = NULL;
            argv[3] = NULL;
            tool_found = true;
        }
        else if (access("/bin/wl-copy", X_OK) == 0) {
            clipboard_tool = "wl-copy";
            argv[0] = "wl-copy";
            argv[1] = NULL;
            argv[2] = NULL;
            argv[3] = NULL;
            tool_found = true;
        }
    }

    if (!tool_found) {
        editor_set_status_message("Copy error: No clipboard tool found (pbcopy, wl-copy, xclip).");
        free(selected_text);
        return;
    }

    editor_set_status_message("Attempting to copy using %s...", clipboard_tool);
    editor_refresh_screen();

    fflush(stdout);

    if (pipe(pipefd) == -1) {
        editor_set_status_message("Copy error: Failed to create pipe.");
        free(selected_text);
        return;
    }

    pid = fork();
    if (pid == -1) {
        editor_set_status_message("Copy error: Failed to fork process.");
        close(pipefd[0]);
        close(pipefd[1]);
        free(selected_text);
        return;
    }

    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDIN_FILENO);
        close(pipefd[1]);

        execvp(clipboard_tool, argv);
        _exit(1);
    } else {
        close(pipefd[0]);

        write(pipefd[1], selected_text, current_offset);
        close(pipefd[1]);

        int status;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            editor_set_status_message("Copied %zu bytes to clipboard using %s.", current_offset, clipboard_tool);
        } else {
            editor_set_status_message("Copy error: %s failed or returned an error.", clipboard_tool);
        }
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
    editor_set_status_message("Use terminal selection/copy (drag to select, Ctrl+Shift+C to copy)");
}
