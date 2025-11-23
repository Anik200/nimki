#include"common.h"

extern EditorConfig E;
extern EditorSyntax *E_syntax;

void editor_read_file(const char *filename);
void editor_save_file();
void editor_select_syntax_highlight();
int editor_insert_newline();
void editor_insert_char(int c);
void editor_del_char();

void editor_read_file(const char *filename) {
    if (E.filename) {
        free(E.filename);
        E.filename = NULL;
    }
    E.filename = strdup(filename);
    if (E.filename == NULL) {
        cleanup_editor();
        fprintf(stderr, "Fatal error: out of memory (filename).\n");
        exit(1);
    }

    editor_select_syntax_highlight();

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        if (errno == ENOENT) {
            E.lines = malloc(sizeof(EditorLine));
            if (E.lines == NULL) {
                cleanup_editor();
                fprintf(stderr, "Fatal error: out of memory (initial line).\n");
                exit(1);
            }
            E.lines[0].text = strdup("");
            if (E.lines[0].text == NULL) {
                free(E.lines);
                E.lines = NULL;
                cleanup_editor();
                fprintf(stderr, "Fatal error: out of memory (initial line text).\n");
                exit(1);
            }
            E.lines[0].len = 0;
            E.lines[0].hl = NULL;
            E.lines[0].hl_open_comment = 0;
            E.num_lines = 1;
            editor_set_status_message("New file: %s", filename);
        } else {
            cleanup_editor();
            fprintf(stderr, "Error opening file '%s': %s\n", filename, strerror(errno));
            exit(1);
        }
        editor_save_state();
        return;
    }

    char *line_buffer = NULL;
    size_t linecap = 0;
    ssize_t linelen;

    if (E.lines) {
        for (int i = 0; i < E.num_lines; ++i) {
            free(E.lines[i].text);
            free(E.lines[i].hl);
        }
        free(E.lines);
        E.lines = NULL;
        E.num_lines = 0;
    }

    while ((linelen = getline(&line_buffer, &linecap, fp)) != -1) {
        while (linelen > 0 && (line_buffer[linelen - 1] == '\n' || line_buffer[linelen - 1] == '\r')) {
            linelen--;
        }

        E.lines = realloc(E.lines, (E.num_lines + 1) * sizeof(EditorLine));
        if (E.lines == NULL) {
            free(line_buffer);
            cleanup_editor();
            fprintf(stderr, "Fatal error: out of memory (realloc lines).\n");
            exit(1);
        }

        E.lines[E.num_lines].text = malloc(linelen + 1);
        if (E.lines[E.num_lines].text == NULL) {
            free(line_buffer);
            cleanup_editor();
            fprintf(stderr, "Fatal error: out of memory (line text).\n");
            exit(1);
        }
        memcpy(E.lines[E.num_lines].text, line_buffer, linelen);
        E.lines[E.num_lines].text[linelen] = '\0';
        E.lines[E.num_lines].len = linelen;
        E.lines[E.num_lines].hl = NULL;
        E.lines[E.num_lines].hl_open_comment = 0;
        E.num_lines++;
    }
    free(line_buffer);
    fclose(fp);

    if (E.num_lines == 0) {
        E.lines = malloc(sizeof(EditorLine));
        if (E.lines == NULL) {
            cleanup_editor();
            fprintf(stderr, "Fatal error: out of memory (empty file init after read).\n");
            exit(1);
        }
        E.lines[0].text = strdup("");
        if (E.lines[0].text == NULL) {
            free(E.lines);
            E.lines = NULL;
            cleanup_editor();
            fprintf(stderr, "Fatal error: out of memory (empty file text after read).\n");
            exit(1);
        }
        E.lines[0].len = 0;
        E.lines[0].hl = NULL;
        E.lines[0].hl_open_comment = 0;
        E.num_lines = 1;
    }

    for (int i = 0; i < E.num_lines; i++) {
        editor_update_syntax(i);
    }

    E.dirty = 0;
    editor_set_status_message("Opened file: %s (%d lines)", filename, E.num_lines);
    editor_save_state();
}

void editor_save_file() {
    if (!E.filename) {
        char *new_filename = editor_prompt("Save as: %s (ESC to cancel)", "");
        if (new_filename == NULL) {
            editor_set_status_message("Save cancelled.");
            return;
        }
        if (E.filename) {
            free(E.filename);
            E.filename = NULL;
        }
        E.filename = new_filename;
        editor_select_syntax_highlight();
    }

    FILE *fp = fopen(E.filename, "w");
    if (!fp) {
        editor_set_status_message("Error saving file: %s", strerror(errno));
        return;
    }

    for (int i = 0; i < E.num_lines; ++i) {
        fprintf(fp, "%s\n", E.lines[i].text);
    }
    fclose(fp);
    E.dirty = 0;
    editor_set_status_message("File saved: %s", E.filename);
    editor_save_state();
}

int editor_insert_newline() {
    editor_save_state();
    if (E.num_lines == 0) {
        E.lines = malloc(sizeof(EditorLine));
        if (E.lines == NULL) {
            editor_set_status_message("Error: Out of memory for initial lines array.");
            return -1;
        }
        E.lines[0].text = strdup("");
        if (E.lines[0].text == NULL) {
            free(E.lines);
            E.lines = NULL;
            editor_set_status_message("Error: Out of memory for initial line text.");
            return -1;
        }
        E.lines[0].len = 0;
        E.lines[0].hl = NULL;
        E.lines[0].hl_open_comment = 0;
        E.num_lines = 1;
        E.cy = 0;
        E.cx = 0;
        E.dirty = 1;
        editor_update_syntax(0);
        return 0;
    }

    E.lines = realloc(E.lines, (E.num_lines + 1) * sizeof(EditorLine));
    if (E.lines == NULL) {
        editor_set_status_message("Error: Out of memory for lines array (realloc).");
        return -1;
    }

    memmove(&E.lines[E.cy + 1], &E.lines[E.cy], (E.num_lines - E.cy) * sizeof(EditorLine));

    E.lines[E.cy].hl = NULL;
    E.lines[E.cy].hl_open_comment = 0;

    if (E.cx == 0) {
        E.lines[E.cy].text = strdup("");
        if (E.lines[E.cy].text == NULL) {
            editor_set_status_message("Error: Out of memory for new empty line text.");
            return -1;
        }
        E.lines[E.cy].len = 0;
    } else {
        EditorLine *current_line = &E.lines[E.cy];
        E.lines[E.cy + 1].len = current_line->len - E.cx;
        E.lines[E.cy + 1].text = strdup(&current_line->text[E.cx]);
        if (E.lines[E.cy + 1].text == NULL) {
            editor_set_status_message("Error: Out of memory for split line text.");
            return -1;
        }
        E.lines[E.cy + 1].hl = NULL;
        E.lines[E.cy + 1].hl_open_comment = 0;

        current_line->text = realloc(current_line->text, E.cx + 1);
        if (current_line->text == NULL) {
            editor_set_status_message("Error: out of memory for truncated line.");
            return -1;
        }
        current_line->text[E.cx] = '\0';
        current_line->len = E.cx;
    }

    E.num_lines++;
    E.cy++;
    E.cx = 0;
    E.dirty = 1;

    editor_update_syntax(E.cy - 1);
    editor_update_syntax(E.cy);

    return 0;
}

void editor_insert_char(int c) {
    editor_save_state();
    if (E.cy == E.num_lines) {
        if (editor_insert_newline() == -1) {
            editor_set_status_message("Error: Failed to prepare new line for character insertion.");
            return;
        }
    }

    if (E.lines == NULL || E.cy >= E.num_lines) {
        editor_set_status_message("Internal error: Invalid line state for character insertion.");
        return;
    }

    EditorLine *line = &E.lines[E.cy];
    line->text = realloc(line->text, line->len + 2);
    if (line->text == NULL) {
        editor_set_status_message("Error: Out of memory for line %d.", E.cy);
        return;
    }
    memmove(&line->text[E.cx + 1], &line->text[E.cx], line->len - E.cx + 1);
    line->text[E.cx] = c;
    line->len++;
    E.cx++;
    E.dirty = 1;

    editor_update_syntax(E.cy);
}

void editor_del_char() {
    editor_save_state();

    if (E.select_all_active) {
        if (E.lines) {
            for (int i = 0; i < E.num_lines; ++i) {
                free(E.lines[i].text);
                E.lines[i].text = NULL;
                free(E.lines[i].hl);
                E.lines[i].hl = NULL;
            }
            free(E.lines);
            E.lines = NULL;
        }
        E.lines = malloc(sizeof(EditorLine));
        if (E.lines == NULL) {
            editor_set_status_message("Fatal error: Out of memory (clear all init).");
            exit(1);
        }
        E.lines[0].text = strdup("");
        if (E.lines[0].text == NULL) {
            free(E.lines);
            E.lines = NULL;
            editor_set_status_message("Fatal error: out of memory (clear all text).");
            exit(1);
        }
        E.lines[0].len = 0;
        E.lines[0].hl = NULL;
        E.lines[0].hl_open_comment = 0;
        E.num_lines = 1;
        E.cx = 0;
        E.cy = 0;
        E.dirty = 1;
        E.select_all_active = 0;
        editor_update_syntax(0);
        editor_set_status_message("All text deleted.");
        return;
    }

    if (E.selection_active) {
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

        if (sel_min_cy == sel_max_cy && sel_min_cx == sel_max_cx) {
            E.selection_active = false;
            editor_set_status_message("No text selected for deletion.");
            return;
        }

        int target_cy = sel_min_cy;
        int target_cx = sel_min_cx;

        int new_num_lines = E.num_lines - (sel_max_cy - sel_min_cy);
        if (new_num_lines == 0) {
            if (E.lines) {
                for (int i = 0; i < E.num_lines; ++i) {
                    free(E.lines[i].text);
                    E.lines[i].text = NULL;
                    free(E.lines[i].hl);
                    E.lines[i].hl = NULL;
                }
                free(E.lines);
                E.lines = NULL;
            }
            E.lines = malloc(sizeof(EditorLine));
            if (E.lines == NULL) {
                editor_set_status_message("Fatal error: out of memory (empty file init after sel del).");
                exit(1);
            }
            E.lines[0].text = strdup("");
            if (E.lines[0].text == NULL) {
                free(E.lines);
                E.lines = NULL;
                editor_set_status_message("Fatal error: out of memory (empty file text after sel del).");
                exit(1);
            }
            E.lines[0].len = 0;
            E.lines[0].hl = NULL;
            E.lines[0].hl_open_comment = 0;
            E.num_lines = 1;
            E.cx = 0;
            E.cy = 0;
        } else {
            EditorLine *new_lines = malloc(new_num_lines * sizeof(EditorLine));
            if (new_lines == NULL) {
                editor_set_status_message("Error: Out of memory for new lines array during deletion.");
                return;
            }

            int current_new_line_idx = 0;

            for (int r = 0; r < sel_min_cy; r++) {
                new_lines[current_new_line_idx++] = E.lines[r];
            }

            EditorLine *start_line_orig = &E.lines[sel_min_cy];
            EditorLine *end_line_orig = &E.lines[sel_max_cy];

            size_t merged_len = sel_min_cx + (end_line_orig->len - sel_max_cx);
            char *merged_text = malloc(merged_len + 1);
            if (merged_text == NULL) {
                editor_set_status_message("Error: Out of memory for merged text.");
                free(new_lines);
                return;
            }
            memcpy(merged_text, start_line_orig->text, sel_min_cx);
            memcpy(merged_text + sel_min_cx, end_line_orig->text + sel_max_cx, end_line_orig->len - sel_max_cx);
            merged_text[merged_len] = '\0';

            new_lines[current_new_line_idx].text = merged_text;
            new_lines[current_new_line_idx].len = merged_len;
            new_lines[current_new_line_idx].hl = NULL;
            new_lines[current_new_line_idx].hl_open_comment = 0;
            current_new_line_idx++;

            for (int r = sel_max_cy + 1; r < E.num_lines; r++) {
                new_lines[current_new_line_idx++] = E.lines[r];
            }

            for (int r = sel_min_cy; r <= sel_max_cy; r++) {
                if (r != sel_min_cy || (r == sel_min_cy && sel_min_cy != sel_max_cy)) {
                    free(E.lines[r].text);
                    E.lines[r].text = NULL;
                    free(E.lines[r].hl);
                    E.lines[r].hl = NULL;
                }
            }
            if (sel_min_cy != sel_max_cy) {
                free(start_line_orig->text);
                start_line_orig->text = NULL;
                free(start_line_orig->hl);
                start_line_orig->hl = NULL;
            }

            free(E.lines);
            E.lines = NULL;

            E.lines = new_lines;
            E.num_lines = new_num_lines;
            E.cx = target_cx;
            E.cy = target_cy;
        }

        E.selection_active = false;
        E.dirty = 1;
        editor_set_status_message("Selected text deleted.");
        for (int i = target_cy; i < E.num_lines; i++) {
            editor_update_syntax(i);
        }
        return;
    }

    if (E.cy == E.num_lines || E.num_lines == 0) return;
    if (E.cx == 0 && E.cy == 0 && E.lines[0].len == 0) return;

    EditorLine *line = &E.lines[E.cy];
    if (E.cx > 0) {
        memmove(&line->text[E.cx - 1], &line->text[E.cx], line->len - E.cx + 1);
        line->len--;
        line->text = realloc(line->text, line->len + 1);
        if (line->text == NULL) {
            editor_set_status_message("Error: Out of memory (del char realloc).");
            return;
        }
        E.cx--;
        E.dirty = 1;
        editor_update_syntax(E.cy);
    } else {
        if (E.cy > 0) {
            EditorLine *prev_line = &E.lines[E.cy - 1];
            prev_line->text = realloc(prev_line->text, prev_line->len + line->len + 1);
            if (prev_line->text == NULL) {
                editor_set_status_message("Error: Out of memory (merge line realloc).");
                return;
            }
            memcpy(&prev_line->text[prev_line->len], line->text, line->len);
            prev_line->len += line->len;
            prev_line->text[prev_line->len] = '\0';

            free(line->text);
            line->text = NULL;
            free(line->hl);
            line->hl = NULL;

            memmove(&E.lines[E.cy], &E.lines[E.cy + 1], (E.num_lines - E.cy - 1) * sizeof(EditorLine));
            E.num_lines--;

            if (E.num_lines > 0) {
                EditorLine *new_lines_ptr = realloc(E.lines, E.num_lines * sizeof(EditorLine));
                if (new_lines_ptr == NULL) {
                    editor_set_status_message("Error: Out of memory shrinking lines realloc. Data might be inconsistent.");
                } else {
                    E.lines = new_lines_ptr;
                }
            } else {
                if (E.lines) {
                    free(E.lines);
                    E.lines = NULL;
                }
                E.lines = malloc(sizeof(EditorLine));
                if (E.lines == NULL) {
                    editor_set_status_message("Fatal error: out of memory (empty file init after single del).");
                    exit(1);
                }
                E.lines[0].text = strdup("");
                if (E.lines[0].text == NULL) {
                    free(E.lines);
                    E.lines = NULL;
                    editor_set_status_message("Fatal error: out of memory (empty file text after single del).");
                    exit(1);
                }
                E.lines[0].len = 0;
                E.lines[0].hl = NULL;
                E.lines[0].hl_open_comment = 0;
                E.num_lines = 1;
                E.cx = 0;
                E.cy = 0;
                editor_update_syntax(0);
                E.dirty = 1;
                return;
            }

            E.cx = (int)prev_line->len;
            E.cy--;
            E.dirty = 1;
            editor_update_syntax(E.cy);
        }
    }
}
