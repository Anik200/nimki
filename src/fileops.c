#include "common.h"

extern EditorConfig E;
extern EditorSyntax *E_syntax;

#ifdef _WIN32
static ssize_t getline(char **lineptr, size_t *n, FILE *stream) {
    if (!lineptr || !n || !stream) return -1;
    if (!*lineptr || *n == 0) {
        *n = 128;
        *lineptr = malloc(*n);
        if (!*lineptr) return -1;
    }
    size_t pos = 0;
    int c;
    while ((c = fgetc(stream)) != EOF) {
        if (pos + 2 >= *n) {
            size_t new_size = *n * 2;
            char *new_ptr = realloc(*lineptr, new_size);
            if (!new_ptr) return -1;
            *lineptr = new_ptr;
            *n = new_size;
        }
        (*lineptr)[pos++] = c;
        if (c == '\n') break;
    }
    if (pos == 0 && c == EOF) return -1;
    (*lineptr)[pos] = '\0';
    return (ssize_t)pos;
}
#endif

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
            if (E.lines[i].text) free(E.lines[i].text);
            if (E.lines[i].hl) free(E.lines[i].hl);
        }
        free(E.lines);
        E.lines = NULL;
        E.num_lines = 0;
    }

    int capacity = 0;

    while ((linelen = getline(&line_buffer, &linecap, fp)) != -1) {
        while (linelen > 0 && (line_buffer[linelen - 1] == '\n' || line_buffer[linelen - 1] == '\r')) {
            linelen--;
        }

        if (E.num_lines >= capacity) {
            int new_cap = capacity == 0 ? 64 : capacity * 2;
            EditorLine *new_lines = realloc(E.lines, new_cap * sizeof(EditorLine));
            if (!new_lines) {
                free(line_buffer);
                fclose(fp);
                cleanup_editor();
                fprintf(stderr, "Fatal error: out of memory reading file lines.\n");
                exit(1);
            }
            E.lines = new_lines;
            capacity = new_cap;
        }

        E.lines[E.num_lines].text = malloc(linelen + 1);
        if (E.lines[E.num_lines].text == NULL) {
            free(line_buffer);
            fclose(fp);
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
    E.cx = 0;
    E.cy = 0;
    E.row_offset = 0;
    E.col_offset = 0;
    editor_set_status_message("\"%s\" %dL", filename, E.num_lines);
    editor_save_state();
}

void editor_save_file() {
    if (!E.filename) {
        char *new_filename = editor_prompt("Save as: %s", "");
        if (new_filename == NULL) {
            editor_set_status_message("Save cancelled");
            return;
        }
        if (E.filename) free(E.filename);
        E.filename = new_filename;
        editor_select_syntax_highlight();
    }

    FILE *fp = fopen(E.filename, "w");
    if (!fp) {
        editor_set_status_message("Error saving file: %s", strerror(errno));
        return;
    }

    bool write_err = false;
    for (int i = 0; i < E.num_lines; ++i) {
        if (fprintf(fp, "%s\n", E.lines[i].text ? E.lines[i].text : "") < 0) {
            write_err = true;
            break;
        }
    }
    if (fclose(fp) != 0 || write_err) {
        editor_set_status_message("Error: Failed to write all data to \"%s\"", E.filename);
        return;
    }

    E.dirty = 0;
    editor_set_status_message("\"%s\" %dL written", E.filename, E.num_lines);
    editor_save_state();
}

int editor_insert_newline() {
    editor_save_state();
    if (E.num_lines == 0 || !E.lines) {
        E.lines = malloc(sizeof(EditorLine));
        if (E.lines == NULL) {
            editor_set_status_message("Error: Out of memory for lines array");
            return -1;
        }
        E.lines[0].text = strdup("");
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

    if (E.cy < 0) E.cy = 0;
    if (E.cy >= E.num_lines) E.cy = E.num_lines - 1;

    EditorLine *current = &E.lines[E.cy];
    if (E.cx < 0) E.cx = 0;
    if (E.cx > (int)current->len) E.cx = (int)current->len;

    char *left_text = malloc(E.cx + 1);
    char *right_text = strdup(&current->text[E.cx]);
    if (!left_text || !right_text) {
        if (left_text) free(left_text);
        if (right_text) free(right_text);
        editor_set_status_message("Error: Out of memory splitting line");
        return -1;
    }
    memcpy(left_text, current->text, E.cx);
    left_text[E.cx] = '\0';

    EditorLine *new_lines = realloc(E.lines, (E.num_lines + 1) * sizeof(EditorLine));
    if (new_lines == NULL) {
        free(left_text);
        free(right_text);
        editor_set_status_message("Error: Out of memory expanding lines array");
        return -1;
    }
    E.lines = new_lines;

    memmove(&E.lines[E.cy + 2], &E.lines[E.cy + 1], (E.num_lines - (E.cy + 1)) * sizeof(EditorLine));

    free(E.lines[E.cy].text);
    if (E.lines[E.cy].hl) free(E.lines[E.cy].hl);

    E.lines[E.cy].text = left_text;
    E.lines[E.cy].len = E.cx;
    E.lines[E.cy].hl = NULL;
    E.lines[E.cy].hl_open_comment = 0;

    E.lines[E.cy + 1].text = right_text;
    E.lines[E.cy + 1].len = strlen(right_text);
    E.lines[E.cy + 1].hl = NULL;
    E.lines[E.cy + 1].hl_open_comment = 0;

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
    if (E.cy >= E.num_lines || !E.lines) {
        if (editor_insert_newline() == -1) return;
    }

    EditorLine *line = &E.lines[E.cy];
    if (E.cx < 0) E.cx = 0;
    if (E.cx > (int)line->len) E.cx = (int)line->len;

    char *new_text = realloc(line->text, line->len + 2);
    if (new_text == NULL) {
        editor_set_status_message("Error: Out of memory inserting char");
        return;
    }
    line->text = new_text;
    memmove(&line->text[E.cx + 1], &line->text[E.cx], line->len - E.cx + 1);
    line->text[E.cx] = (char)c;
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
                if (E.lines[i].text) free(E.lines[i].text);
                if (E.lines[i].hl) free(E.lines[i].hl);
            }
            free(E.lines);
            E.lines = NULL;
        }
        E.lines = malloc(sizeof(EditorLine));
        if (E.lines == NULL) {
            cleanup_editor();
            exit(1);
        }
        E.lines[0].text = strdup("");
        E.lines[0].len = 0;
        E.lines[0].hl = NULL;
        E.lines[0].hl_open_comment = 0;
        E.num_lines = 1;
        E.cx = 0;
        E.cy = 0;
        E.dirty = 1;
        E.select_all_active = 0;
        editor_update_syntax(0);
        return;
    }

    if (E.selection_active) {
        int sel_min_cy = E.selection_start_cy;
        int sel_min_cx = E.selection_start_cx;
        int sel_max_cy = E.selection_end_cy;
        int sel_max_cx = E.selection_end_cx;

        if (sel_min_cy > sel_max_cy || (sel_min_cy == sel_max_cy && sel_min_cx > sel_max_cx)) {
            int t_y = sel_min_cy; int t_x = sel_min_cx;
            sel_min_cy = sel_max_cy; sel_min_cx = sel_max_cx;
            sel_max_cy = t_y; sel_max_cx = t_x;
        }

        if (sel_min_cy == sel_max_cy && sel_min_cx == sel_max_cx) {
            E.selection_active = false;
            return;
        }

        int target_cy = sel_min_cy;
        int target_cx = sel_min_cx;
        int deleted_lines = sel_max_cy - sel_min_cy;
        int new_num_lines = E.num_lines - deleted_lines;

        if (new_num_lines <= 0) {
            for (int i = 0; i < E.num_lines; ++i) {
                if (E.lines[i].text) free(E.lines[i].text);
                if (E.lines[i].hl) free(E.lines[i].hl);
            }
            free(E.lines);
            E.lines = malloc(sizeof(EditorLine));
            E.lines[0].text = strdup("");
            E.lines[0].len = 0;
            E.lines[0].hl = NULL;
            E.lines[0].hl_open_comment = 0;
            E.num_lines = 1;
            E.cx = 0;
            E.cy = 0;
        } else {
            EditorLine *start_line = &E.lines[sel_min_cy];
            EditorLine *end_line = &E.lines[sel_max_cy];
            size_t merged_len = sel_min_cx + (end_line->len - sel_max_cx);
            char *merged_text = malloc(merged_len + 1);
            if (merged_text) {
                memcpy(merged_text, start_line->text, sel_min_cx);
                memcpy(merged_text + sel_min_cx, end_line->text + sel_max_cx, end_line->len - sel_max_cx);
                merged_text[merged_len] = '\0';

                for (int r = sel_min_cy; r <= sel_max_cy; r++) {
                    if (E.lines[r].text) free(E.lines[r].text);
                    if (E.lines[r].hl) free(E.lines[r].hl);
                }

                E.lines[sel_min_cy].text = merged_text;
                E.lines[sel_min_cy].len = merged_len;
                E.lines[sel_min_cy].hl = NULL;
                E.lines[sel_min_cy].hl_open_comment = 0;

                if (deleted_lines > 0) {
                    memmove(&E.lines[sel_min_cy + 1], &E.lines[sel_max_cy + 1],
                            (E.num_lines - sel_max_cy - 1) * sizeof(EditorLine));
                }
                E.num_lines = new_num_lines;
                E.cx = target_cx;
                E.cy = target_cy;
            }
        }

        E.selection_active = false;
        E.dirty = 1;
        for (int i = target_cy; i < E.num_lines; i++) editor_update_syntax(i);
        return;
    }

    if (E.cy >= E.num_lines || E.num_lines == 0 || !E.lines) return;
    if (E.cx == 0 && E.cy == 0 && E.lines[0].len == 0) return;

    EditorLine *line = &E.lines[E.cy];
    if (E.cx > 0) {
        memmove(&line->text[E.cx - 1], &line->text[E.cx], line->len - E.cx + 1);
        line->len--;
        char *shrunk = realloc(line->text, line->len + 1);
        if (shrunk) line->text = shrunk;
        E.cx--;
        E.dirty = 1;
        editor_update_syntax(E.cy);
    } else {
        if (E.cy > 0) {
            EditorLine *prev = &E.lines[E.cy - 1];
            size_t prev_len = prev->len;
            size_t merged_len = prev_len + line->len;

            char *new_prev_text = realloc(prev->text, merged_len + 1);
            if (!new_prev_text) {
                editor_set_status_message("Error: Out of memory merging lines");
                return;
            }
            prev->text = new_prev_text;
            memcpy(&prev->text[prev_len], line->text, line->len);
            prev->text[merged_len] = '\0';
            prev->len = merged_len;

            free(line->text);
            if (line->hl) free(line->hl);

            memmove(&E.lines[E.cy], &E.lines[E.cy + 1], (E.num_lines - E.cy - 1) * sizeof(EditorLine));
            E.num_lines--;

            E.cx = (int)prev_len;
            E.cy--;
            E.dirty = 1;
            editor_update_syntax(E.cy);
        }
    }
}

void editor_delete_current_line() {
    if (E.num_lines == 0 || !E.lines) return;
    editor_save_state();

    if (E.cy < 0) E.cy = 0;
    if (E.cy >= E.num_lines) E.cy = E.num_lines - 1;

    if (E.yank_buffer) free(E.yank_buffer);
    E.yank_buffer = strdup(E.lines[E.cy].text ? E.lines[E.cy].text : "");
    E.yank_is_line = true;

    if (E.lines[E.cy].text) free(E.lines[E.cy].text);
    if (E.lines[E.cy].hl) free(E.lines[E.cy].hl);

    memmove(&E.lines[E.cy], &E.lines[E.cy + 1], (E.num_lines - E.cy - 1) * sizeof(EditorLine));
    E.num_lines--;

    if (E.num_lines == 0) {
        E.lines[0].text = strdup("");
        E.lines[0].len = 0;
        E.lines[0].hl = NULL;
        E.lines[0].hl_open_comment = 0;
        E.num_lines = 1;
        E.cy = 0;
        E.cx = 0;
    } else {
        if (E.cy >= E.num_lines) E.cy = E.num_lines - 1;
        int max_cx = (int)E.lines[E.cy].len;
        if (E.cx >= max_cx) E.cx = max_cx > 0 ? max_cx - 1 : 0;
    }

    E.dirty = 1;
    for (int i = E.cy; i < E.num_lines; i++) editor_update_syntax(i);
    editor_set_status_message("1 line deleted");
    editor_refresh_screen();
}

void editor_yank_current_line() {
    if (E.cy < 0 || E.cy >= E.num_lines || !E.lines) return;
    if (E.yank_buffer) free(E.yank_buffer);
    E.yank_buffer = strdup(E.lines[E.cy].text ? E.lines[E.cy].text : "");
    E.yank_is_line = true;
    editor_set_status_message("1 line yanked");
}

void editor_paste_yank(bool below) {
    if (!E.yank_buffer) {
        editor_set_status_message("Yank buffer empty");
        return;
    }
    editor_save_state();

    if (E.yank_is_line) {
        int target = below ? E.cy + 1 : E.cy;
        if (target > E.num_lines) target = E.num_lines;

        EditorLine *new_lines = realloc(E.lines, (E.num_lines + 1) * sizeof(EditorLine));
        if (!new_lines) {
            editor_set_status_message("Out of memory pasting line");
            return;
        }
        E.lines = new_lines;

        memmove(&E.lines[target + 1], &E.lines[target], (E.num_lines - target) * sizeof(EditorLine));
        char *pasted = strdup(E.yank_buffer);
        if (!pasted) return;
        E.lines[target].text = pasted;
        E.lines[target].len = strlen(pasted);
        E.lines[target].hl = NULL;
        E.lines[target].hl_open_comment = 0;
        E.num_lines++;
        E.cy = target;
        E.cx = 0;
        E.dirty = 1;
        for (int i = target; i < E.num_lines; i++) editor_update_syntax(i);
        editor_set_status_message("1 line pasted");
    } else {
        size_t yank_len = strlen(E.yank_buffer);
        if (yank_len > 0) {
            if (E.cy >= E.num_lines || !E.lines) {
                if (editor_insert_newline() == -1) return;
            }
            EditorLine *line = &E.lines[E.cy];
            if (E.cx < 0) E.cx = 0;
            if (E.cx > (int)line->len) E.cx = (int)line->len;
            char *new_text = realloc(line->text, line->len + yank_len + 1);
            if (new_text) {
                line->text = new_text;
                memmove(&line->text[E.cx + yank_len], &line->text[E.cx], line->len - E.cx + 1);
                memcpy(&line->text[E.cx], E.yank_buffer, yank_len);
                line->len += yank_len;
                E.cx += (int)yank_len;
                E.dirty = 1;
                editor_update_syntax(E.cy);
            }
        }
    }
    editor_refresh_screen();
}

void editor_handle_vim_command() {
    char *cmd = editor_prompt(":%s", "");
    if (!cmd) return;

    char *p = cmd;
    while (*p && isspace((unsigned char)*p)) p++;

    if (strcmp(p, "w") == 0) {
        editor_save_file();
    } else if (strcmp(p, "q") == 0) {
        if (E.dirty) {
            editor_set_status_message("No write since last change (use :q! to override)");
        } else {
            cleanup_editor();
            exit(0);
        }
    } else if (strcmp(p, "q!") == 0) {
        cleanup_editor();
        exit(0);
    } else if (strcmp(p, "wq") == 0 || strcmp(p, "x") == 0) {
        editor_save_file();
        cleanup_editor();
        exit(0);
    } else if (isdigit((unsigned char)p[0])) {
        int line_no = atoi(p);
        if (line_no > 0) {
            E.cy = line_no - 1;
            if (E.cy >= E.num_lines) E.cy = E.num_lines - 1;
            E.cx = 0;
        }
    } else if (*p) {
        editor_set_status_message("Not an editor command: :%s", p);
    }
    free(cmd);
    editor_refresh_screen();
}
