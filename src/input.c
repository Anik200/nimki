#include"common.h"

extern EditorConfig E;
extern FileTreeState FT;
extern char status_message[80];
extern time_t status_message_time;

void editor_process_keypress();
void handle_winch(int sig);
void editor_find();
void editor_find_next(int direction);
void editor_select_all();
void editor_copy_selection_to_clipboard();
void paste_from_clipboard();
void editor_undo();
void editor_save_file();
void editor_read_file(const char *filename);
void editor_del_char();
void editor_insert_char(int c);
void editor_move_cursor(int key);
void editor_refresh_screen();
void file_tree_move_cursor(int direction);
void file_tree_toggle_expand();
void file_tree_open_file();
void toggle_file_tree();
void draw_file_tree();

static void screen_to_editor_coords(int screen_x, int screen_y, int *out_cy, int *out_cx) {
    int x_offset = E.file_tree_visible ? FILE_TREE_WIDTH : 0;
    int line_num_width = 0;
    if (E.show_line_numbers) {
        int num_digits = 1;
        if (E.num_lines > 0) {
            num_digits = (int)log10(E.num_lines) + 1;
        }
        line_num_width = num_digits + 1;
    }

    int clicked_cy = screen_y + E.row_offset;
    if (clicked_cy < 0) clicked_cy = 0;
    if (clicked_cy >= E.num_lines) {
        clicked_cy = E.num_lines > 0 ? E.num_lines - 1 : 0;
    }

    int clicked_cx = 0;
    int text_screen_x = screen_x - (x_offset + line_num_width);
    if (text_screen_x <= 0) {
        clicked_cx = 0;
    } else {
        int target_display_cx = text_screen_x + E.col_offset;
        if (clicked_cy < E.num_lines) {
            EditorLine *line = &E.lines[clicked_cy];
            int current_display_cx = 0;
            for (int char_idx = 0; char_idx < (int)line->len; char_idx++) {
                int char_display_width = 1;
                if (line->text[char_idx] == '\t') {
                    char_display_width = TAB_STOP - (current_display_cx % TAB_STOP);
                }
                if (target_display_cx < current_display_cx + char_display_width) {
                    clicked_cx = char_idx;
                    break;
                }
                current_display_cx += char_display_width;
                clicked_cx = char_idx + 1;
            }
            if (clicked_cx > (int)line->len) {
                clicked_cx = (int)line->len;
            }
        }
    }

    *out_cy = clicked_cy;
    *out_cx = clicked_cx;
}

void editor_process_keypress() {
    MEVENT event;
    int c = getch();
    bool cursor_moved = false;
    int original_cx = E.cx;
    int original_cy = E.cy;

    if (E.context_menu_active) {
        switch (c) {
            case KEY_UP:
                E.context_menu_selected_option--;
                if (E.context_menu_selected_option < 0) {
                    E.context_menu_selected_option = 1;
                }
                break;
            case KEY_DOWN:
                E.context_menu_selected_option++;
                if (E.context_menu_selected_option > 1) {
                    E.context_menu_selected_option = 0;
                }
                break;
            case '\n':
            case '\r':
            case KEY_ENTER:
                if (E.context_menu_selected_option == 0) {
                    editor_copy_selection_to_clipboard();
                } else if (E.context_menu_selected_option == 1) {
                    editor_select_all();
                }
                editor_set_status_message("");
                break;
            case 27:
                E.context_menu_active = false;
                editor_set_status_message("");
                break;
        }
    }

    bool current_key_is_selection_or_cursor_move = (c == CTRL('k') || c == CTRL('a') || c == KEY_MOUSE ||
                                                     c == KEY_UP || c == KEY_DOWN ||
                                                     c == KEY_LEFT || c == KEY_RIGHT || c == KEY_HOME ||
                                                     c == KEY_END || c == KEY_PPAGE || c == KEY_NPAGE);

    if (E.selection_active && !current_key_is_selection_or_cursor_move &&
        !(c == KEY_BACKSPACE || c == KEY_DC || c == 127)) {
        E.selection_active = false;
        editor_set_status_message("");
        for (int i = 0; i < E.num_lines; i++) {
            editor_update_syntax(i);
        }
        editor_refresh_screen(); // Refresh immediately when selection is cleared
    }

    if (E.find_active && c != KEY_UP && c != KEY_DOWN && c != CTRL('f')) {
        E.find_active = false;
        editor_set_status_message("");
        for (int i = 0; i < E.num_lines; i++) {
            editor_update_syntax(i);
        }
        editor_refresh_screen();
    }

    if (E.select_all_active && c != KEY_BACKSPACE && c != 127 && c != KEY_DC) {
        E.select_all_active = 0;
        editor_set_status_message("");
    }

    switch (c) {
        case CTRL('q'):
        case CTRL('c'):
            if (E.dirty) {
                editor_set_status_message("WARNING! File has unsaved changes. Press Ctrl+Q/C again to force quit.");
                editor_refresh_screen();
                int c2 = getch();
                if (c2 != CTRL('q') && c2 != CTRL('c')) return;
            }
            cleanup_editor();
            exit(0);
            break;

        case CTRL('s'):
            editor_save_file();
            break;

        case CTRL('a'):
            editor_select_all();
            cursor_moved = true;
            break;

        case CTRL('v'):
            editor_set_status_message("Use terminal paste (Ctrl+Shift+V or right-click)");
            break;

        case CTRL('w'):
            editor_find();
            break;

        case CTRL('z'):
            editor_undo();
            break;

        case CTRL('f'):
            editor_find();
            break;

        case CTRL('k'):
            if (!E.selection_active) {
                E.selection_active = true;
                E.selection_start_cy = E.cy;
                E.selection_start_cx = E.cx;
                E.selection_end_cy = E.cy;
                E.selection_end_cx = E.cx;
                editor_set_status_message("Selection mode active. Move cursor to select, press Ctrl+K to copy.");
            } else {
                editor_copy_selection_to_clipboard();
            }
            cursor_moved = true;
            break;

        case KEY_BACKSPACE:
        case KEY_DC:
        case 127:
            editor_del_char();
            break;

        case '\t':
            if (E.file_tree_visible) {
                file_tree_toggle_expand();
                editor_refresh_screen();
                return;
            } else {
                editor_insert_char('\t');
            }
            break;

        case '\r':
        case '\n':
            if (E.file_tree_visible) {
                file_tree_open_file();
                return;
            } else {
                editor_insert_newline();
            }
            break;

        case KEY_HOME:
        case KEY_END:
            if (E.file_tree_visible) {
                editor_move_cursor(c);
                cursor_moved = true;
                if (E.selection_active) {
                    E.selection_end_cy = E.cy;
                    E.selection_end_cx = E.cx;
                }
            } else {
                editor_move_cursor(c);
                cursor_moved = true;
                if (E.selection_active) {
                    E.selection_end_cy = E.cy;
                    E.selection_end_cx = E.cx;
                }
            }
            break;
        case KEY_PPAGE:
        case KEY_NPAGE:
            if (E.file_tree_visible) {
                if (c == KEY_PPAGE) {
                    file_tree_move_cursor(-E.screen_rows);
                } else if (c == KEY_NPAGE) {
                    file_tree_move_cursor(E.screen_rows);
                }
                editor_refresh_screen();
                return;
            } else {
                editor_move_cursor(c);
                cursor_moved = true;
                if (E.selection_active) {
                    E.selection_end_cy = E.cy;
                    E.selection_end_cx = E.cx;
                }
            }
            break;
        case KEY_UP:
        case KEY_DOWN:
        case KEY_LEFT:
        case KEY_RIGHT:
            if (E.file_tree_visible) {
                if (c == KEY_UP) {
                    file_tree_move_cursor(-1);
                } else if (c == KEY_DOWN) {
                    file_tree_move_cursor(1);
                } else if (c == KEY_LEFT || c == KEY_RIGHT) {
                    file_tree_toggle_expand();
                }
                editor_refresh_screen();
                return;
            } else {
                editor_move_cursor(c);
                cursor_moved = true;
                if (E.selection_active) {
                    E.selection_end_cy = E.cy;
                    E.selection_end_cx = E.cx;
                }
            }
            break;
        case CTRL('t'):
            E.show_line_numbers = !E.show_line_numbers;
            editor_set_status_message("Line numbers %s", E.show_line_numbers ? "ON" : "OFF");
            cursor_moved = true;
            break;

        case CTRL('n'):
            toggle_file_tree();
            editor_refresh_screen();
            return;

        case KEY_MOUSE:
            if (getmouse(&event) == OK) {
                if (E.file_tree_visible && event.x < FILE_TREE_WIDTH - 1 && (
#ifdef BUTTON1_PRESSED
                    (event.bstate & BUTTON1_PRESSED) ||
#endif
#ifdef BUTTON1_CLICKED
                    (event.bstate & BUTTON1_CLICKED) ||
#endif
                    (event.bstate & 1) || (event.bstate & 2)
                )) {
                    int tree_row = event.y + E.file_tree_offset;
                    if (tree_row < FT.flat_node_count) {
                        E.file_tree_cursor = tree_row;
                        FileTreeNode *node = FT.flat_nodes[E.file_tree_cursor];
                        if (node->is_dir) {
                            file_tree_toggle_expand();
                        } else {
                            file_tree_open_file();
                        }
                        editor_refresh_screen();
                        return;
                    }
                } else if (
#ifdef BUTTON4_PRESSED
                    (event.bstate & BUTTON4_PRESSED)
#else
                    0
#endif
#ifdef BUTTON4_CLICKED
                    || (event.bstate & BUTTON4_CLICKED)
#endif
                ) {
                    // Wheel up: scroll up by 3 lines and keep cursor visible
                    int scroll_amount = 3;
                    while (scroll_amount-- > 0 && E.row_offset > 0) {
                        E.row_offset--;
                    }
                    if (E.cy >= E.row_offset + E.screen_rows) {
                        E.cy = E.row_offset + E.screen_rows - 1;
                    }
                    if (E.cy < 0) E.cy = 0;
                    if (E.cy >= E.num_lines) E.cy = E.num_lines > 0 ? E.num_lines - 1 : 0;
                    int line_len = (E.cy < E.num_lines) ? (int)E.lines[E.cy].len : 0;
                    if (E.cx > line_len) E.cx = line_len;
                    cursor_moved = true;
                } else if (
#ifdef BUTTON5_PRESSED
                    (event.bstate & BUTTON5_PRESSED)
#else
                    0
#endif
#ifdef BUTTON5_CLICKED
                    || (event.bstate & BUTTON5_CLICKED)
#endif
                ) {
                    // Wheel down: scroll down by 3 lines and keep cursor visible
                    int max_offset = E.num_lines > E.screen_rows ? E.num_lines - E.screen_rows : 0;
                    int scroll_amount = 3;
                    while (scroll_amount-- > 0 && E.row_offset < max_offset) {
                        E.row_offset++;
                    }
                    if (E.cy < E.row_offset) {
                        E.cy = E.row_offset;
                    }
                    if (E.cy >= E.num_lines) E.cy = E.num_lines > 0 ? E.num_lines - 1 : 0;
                    int line_len = (E.cy < E.num_lines) ? (int)E.lines[E.cy].len : 0;
                    if (E.cx > line_len) E.cx = line_len;
                    cursor_moved = true;
                } else if (event.bstate & REPORT_MOUSE_POSITION) {
                    // Mouse dragging with button held down for selection
                    if (event.y >= 0 && event.y < E.screen_rows) {
                        int drag_cy, drag_cx;
                        screen_to_editor_coords(event.x, event.y, &drag_cy, &drag_cx);

                        if (!E.selection_active) {
                            E.selection_active = true;
                            E.selection_start_cy = E.cy;
                            E.selection_start_cx = E.cx;
                        }
                        E.selection_end_cy = drag_cy;
                        E.selection_end_cx = drag_cx;
                        E.cy = drag_cy;
                        E.cx = drag_cx;
                        cursor_moved = true;
                    }
                } else if (
#ifdef BUTTON1_PRESSED
                    (event.bstate & BUTTON1_PRESSED) ||
#endif
#ifdef BUTTON1_CLICKED
                    (event.bstate & BUTTON1_CLICKED) ||
#endif
#ifdef BUTTON1_DOUBLE_CLICKED
                    (event.bstate & BUTTON1_DOUBLE_CLICKED) ||
#endif
#ifdef BUTTON1_RELEASED
                    (event.bstate & BUTTON1_RELEASED) ||
#endif
                    (event.bstate & 1) || (event.bstate & 2)
                ) {
                    // Snap cursor directly to clicked position
                    if (event.y >= 0 && event.y < E.screen_rows) {
                        screen_to_editor_coords(event.x, event.y, &E.cy, &E.cx);
                        E.selection_active = false;
                        cursor_moved = true;
                    }
                } else if (
#ifdef BUTTON3_PRESSED
                    (event.bstate & BUTTON3_PRESSED) ||
#endif
#ifdef BUTTON3_CLICKED
                    (event.bstate & BUTTON3_CLICKED) ||
#endif
                    (event.bstate & 4)
                ) {
                    E.context_menu_active = true;
                    E.context_menu_x = event.x;
                    E.context_menu_y = event.y;
                    E.context_menu_selected_option = 0;
                    editor_set_status_message("");
                }
            }
            break;
        default:
            if (c >= 32 && c <= 126) {
                 editor_insert_char(c);
            }
            break;
    }

    if (E.dirty || cursor_moved || original_cx != E.cx || original_cy != E.cy || time(NULL) - status_message_time < 5 || E.context_menu_active) {
        editor_refresh_screen();
    }
}

void handle_winch(int sig) {
    (void)sig;
    endwin();
    refresh();
    getmaxyx(stdscr, E.screen_rows, E.screen_cols);
    E.screen_rows -= 2;
    editor_refresh_screen();
}
