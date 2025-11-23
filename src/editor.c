#include "common.h"

EditorConfig E;
FileTreeState FT = {NULL, NULL, 0, 0};
EditorSyntax *E_syntax = NULL;

extern char status_message[80];
extern time_t status_message_time;

void init_editor();
void cleanup_editor();
void editor_free_snapshot(EditorStateSnapshot *snapshot);
void editor_save_state();
void editor_undo();
int get_cx_display();
void editor_scroll();
void editor_move_cursor(int key);
int is_separator(int c);
char *editor_prompt(const char *prompt_fmt, ...);

void init_editor() {
    E.cx = 0;
    E.cy = 0;
    E.num_lines = 0;
    E.lines = NULL;
    E.row_offset = 0;
    E.col_offset = 0;
    E.filename = NULL;
    E.dirty = 0;
    E.select_all_active = 0;

    E.undo_history_len = 0;
    E.undo_history_idx = 0;
    for (int i = 0; i < MAX_UNDO_STATES; ++i) {
        E.undo_history[i].lines = NULL;
        E.undo_history[i].num_lines = 0;
        E.undo_history[i].cx = 0;
        E.undo_history[i].cy = 0;
        E.undo_history[i].dirty = 0;
    }

    E.search_query = NULL;
    E.search_direction = 1;
    E.last_match_row = -1;
    E.last_match_col = -1;
    E.find_active = false;

    E.selection_active = false;
    E.selection_start_cy = 0;
    E.selection_start_cx = 0;
    E.selection_end_cy = 0;
    E.selection_end_cx = 0;

    E.context_menu_active = false;
    E.context_menu_x = 0;
    E.context_menu_y = 0;
    E.context_menu_selected_option = 0;
    E.show_line_numbers = false;

    E.file_tree_visible = false;
    E.file_tree_cursor = 0;
    E.file_tree_offset = 0;
    E.file_tree_width = 30;

    initscr();
    raw();
    noecho();
    keypad(stdscr, TRUE);

    mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);

    signal(SIGWINCH, handle_winch);

    getmaxyx(stdscr, E.screen_rows, E.screen_cols);
    E.screen_rows -= 2;

    if (has_colors()) {
        initialize_syntax_colors();
    }
}

void editor_free_snapshot(EditorStateSnapshot *snapshot) {
    if (snapshot->lines) {
        for (int i = 0; i < snapshot->num_lines; ++i) {
            free(snapshot->lines[i].text);
            snapshot->lines[i].text = NULL;
            free(snapshot->lines[i].hl);
            snapshot->lines[i].hl = NULL;
        }
        free(snapshot->lines);
        snapshot->lines = NULL;
    }
    snapshot->num_lines = 0;
    snapshot->cx = 0;
    snapshot->cy = 0;
    snapshot->dirty = 0;
}

void cleanup_editor() {
    endwin();

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
    if (E.filename) {
        free(E.filename);
        E.filename = NULL;
    }
    if (E.search_query) {
        free(E.search_query);
        E.search_query = NULL;
    }

    for (int i = 0; i < MAX_UNDO_STATES; ++i) {
        editor_free_snapshot(&E.undo_history[i]);
    }
    E.undo_history_len = 0;
    E.undo_history_idx = 0;

    if (FT.root) {
        free_file_tree(FT.root);
        FT.root = NULL;
    }
    if (FT.flat_nodes) {
        free(FT.flat_nodes);
        FT.flat_nodes = NULL;
    }
    FT.flat_node_count = 0;
    FT.max_nodes = 0;
}

void editor_save_state() {
    if (E.undo_history_idx < E.undo_history_len) {
        for (int i = E.undo_history_idx; i < E.undo_history_len; ++i) {
            editor_free_snapshot(&E.undo_history[i]);
        }
        E.undo_history_len = E.undo_history_idx;
    }

    if (E.undo_history_len == MAX_UNDO_STATES) {
        editor_free_snapshot(&E.undo_history[0]);
        memmove(&E.undo_history[0], &E.undo_history[1], (MAX_UNDO_STATES - 1) * sizeof(EditorStateSnapshot));
        E.undo_history_len--;
        E.undo_history_idx--;
    }

    EditorStateSnapshot *new_snapshot = &E.undo_history[E.undo_history_idx];
    new_snapshot->num_lines = E.num_lines;
    new_snapshot->cx = E.cx;
    new_snapshot->cy = E.cy;
    new_snapshot->dirty = E.dirty;

    new_snapshot->lines = malloc(E.num_lines * sizeof(EditorLine));
    if (new_snapshot->lines == NULL) {
        editor_set_status_message("Undo error: Out of memory for snapshot lines.");
        return;
    }

    for (int i = 0; i < E.num_lines; ++i) {
        new_snapshot->lines[i].len = E.lines[i].len;
        new_snapshot->lines[i].hl_open_comment = E.lines[i].hl_open_comment;

        new_snapshot->lines[i].text = strdup(E.lines[i].text);
        if (new_snapshot->lines[i].text == NULL) {
            editor_set_status_message("Undo error: Out of memory for snapshot line text.");
            editor_free_snapshot(new_snapshot);
            return;
        }

        new_snapshot->lines[i].hl = malloc(E.lines[i].len);
        if (new_snapshot->lines[i].hl == NULL) {
            editor_set_status_message("Undo error: Out of memory for snapshot line hl.");
            free(new_snapshot->lines[i].text);
            editor_free_snapshot(new_snapshot);
            return;
        }
        memcpy(new_snapshot->lines[i].hl, E.lines[i].hl, E.lines[i].len);
    }

    E.undo_history_len++;
    E.undo_history_idx++;
}

void editor_undo() {
    if (E.undo_history_idx <= 0) {
        editor_set_status_message("Nothing to undo.");
        return;
    }

    E.undo_history_idx--;

    EditorStateSnapshot *prev_snapshot = &E.undo_history[E.undo_history_idx];

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

    E.num_lines = prev_snapshot->num_lines;
    E.cx = prev_snapshot->cx;
    E.cy = prev_snapshot->cy;
    E.dirty = prev_snapshot->dirty;

    E.lines = malloc(E.num_lines * sizeof(EditorLine));
    if (E.lines == NULL) {
        editor_set_status_message("Undo error: Out of memory restoring lines.");
        return;
    }

    for (int i = 0; i < E.num_lines; ++i) {
        E.lines[i].len = prev_snapshot->lines[i].len;
        E.lines[i].hl_open_comment = prev_snapshot->lines[i].hl_open_comment;

        E.lines[i].text = strdup(prev_snapshot->lines[i].text);
        if (E.lines[i].text == NULL) {
            editor_set_status_message("Undo error: out of memory restoring line text.");
            for (int j = 0; j < i; ++j) {
                free(E.lines[j].text);
                free(E.lines[j].hl);
            }
            free(E.lines);
            E.lines = NULL;
            E.num_lines = 0;
            return;
        }

        E.lines[i].hl = malloc(prev_snapshot->lines[i].len);
        if (E.lines[i].hl == NULL) {
            editor_set_status_message("Undo error: out of memory restoring line hl.");
            for (int j = 0; j < i; ++j) {
                free(E.lines[j].text);
                free(E.lines[j].hl);
            }
            free(E.lines);
            E.lines = NULL;
            E.num_lines = 0;
            return;
        }
        memcpy(E.lines[i].hl, prev_snapshot->lines[i].hl, prev_snapshot->lines[i].len);
    }

    for (int i = 0; i < E.num_lines; i++) {
        editor_update_syntax(i);
    }

    editor_set_status_message("Undo successful.");
    editor_refresh_screen();
}

void editor_move_cursor(int key) {
    EditorLine *line = (E.cy >= E.num_lines) ? NULL : &E.lines[E.cy];

    switch (key) {
        case KEY_LEFT:
            if (E.cx > 0) {
                E.cx--;
            } else if (E.cy > 0) {
                E.cy--;
                E.cx = (int)E.lines[E.cy].len;
            }
            break;
        case KEY_RIGHT:
            if (line && E.cx < (int)line->len) {
                E.cx++;
            } else if (line && E.cx == (int)line->len && E.cy < E.num_lines - 1) {
                E.cy++;
                E.cx = 0;
            }
            break;
        case KEY_UP:
            if (E.cy > 0) {
                E.cy--;
            }
            break;
        case KEY_DOWN:
            if (E.cy < E.num_lines - 1) {
                E.cy++;
            }
            break;
        case KEY_HOME:
            E.cx = 0;
            break;
        case KEY_END:
            if (line) E.cx = (int)line->len;
            break;
        case KEY_PPAGE:
        case KEY_NPAGE:
            {
                int times = E.screen_rows;
                while (times--) {
                    if (key == KEY_PPAGE) {
                        if (E.cy > 0) E.cy--;
                    } else {
                        if (E.cy < E.num_lines - 1) E.cy++;
                    }
                }
            }
            break;
    }
    line = (E.cy >= E.num_lines) ? NULL : &E.lines[E.cy];
    int line_len = line ? (int)line->len : 0;
    if (E.cx > line_len) {
        E.cx = line_len;
    }
}

int get_cx_display() {
    int display_cx = 0;
    if (E.cy >= E.num_lines) return 0;

    EditorLine *line = &E.lines[E.cy];
    for (int i = 0; i < (int)E.cx; i++) {
        if (i >= (int)line->len) break;
        if (line->text[i] == '\t') {
            display_cx += (TAB_STOP - (display_cx % TAB_STOP));
        } else {
            display_cx++;
        }
    }
    int line_num_width = 0;
    if (E.show_line_numbers) {
        int num_digits = 1;
        if (E.num_lines > 0) {
            num_digits = (int)log10(E.num_lines) + 1;
        }
        line_num_width = num_digits + 1;
    }
    return display_cx + line_num_width;
}

void editor_scroll() {
    // Only auto-scroll if cursor is completely out of view
    // Don't auto-scroll if cursor is visible anywhere in the current view
    int top_of_view = E.row_offset;
    int bottom_of_view = E.row_offset + E.screen_rows - 1;

    // Only adjust scrolling if cursor is outside the visible range
    if (E.cy < top_of_view) {
        // Cursor is above visible area, scroll view up to show it
        E.row_offset = E.cy;
    } else if (E.cy > bottom_of_view) {
        // Cursor is below visible area, scroll view down to show it
        E.row_offset = E.cy - E.screen_rows + 1;
    }

    // Ensure row_offset is valid
    if (E.row_offset < 0) E.row_offset = 0;
    int max_row_offset = (E.num_lines > E.screen_rows) ? E.num_lines - E.screen_rows : 0;
    if (max_row_offset < 0) max_row_offset = 0;
    if (E.row_offset > max_row_offset) E.row_offset = max_row_offset;

    int current_line_len = (E.cy >= E.num_lines) ? 0 : (int)E.lines[E.cy].len;
    if (E.cx > current_line_len) {
        E.cx = current_line_len;
    }

    int current_cx_display = get_cx_display();

    if (current_cx_display < E.col_offset) {
        E.col_offset = current_cx_display;
    }
    if (current_cx_display >= E.col_offset + E.screen_cols) {
        E.col_offset = current_cx_display - E.screen_cols + 1;
    }
}

int is_separator(int c) {
    return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[];", c) != NULL;
}

char *editor_prompt(const char *prompt_fmt, ...) {
    char buffer[128];
    size_t buflen = 0;
    buffer[0] = '\0';

    while (1) {
        editor_set_status_message(prompt_fmt, buffer);
        editor_refresh_screen();

        int c = getch();
        if (c == '\r' || c == '\n') {
            if (buflen > 0) {
                return strdup(buffer);
            }
            editor_set_status_message("");
            return NULL;
        } else if (c == CTRL('c') || c == CTRL('q') || c == 27) {
            editor_set_status_message("");
            return NULL;
        } else if (c == KEY_BACKSPACE || c == 127 || c == KEY_DC) {
            if (buflen > 0) {
                buflen--;
                buffer[buflen] = '\0';
            }
        } else if (c >= 32 && c <= 126) {
            if (buflen < sizeof(buffer) - 1) {
                buffer[buflen++] = c;
                buffer[buflen] = '\0';
            }
        }
    }
}
