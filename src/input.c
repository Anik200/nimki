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

    bool current_key_is_selection_or_cursor_move = (c == CTRL('k') || c == KEY_UP || c == KEY_DOWN ||
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
            editor_set_status_message("Use terminal copy/paste (Ctrl+Shift+C/V or right-click)");
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
                if (E.file_tree_visible && event.x < FILE_TREE_WIDTH - 1 && (event.bstate & BUTTON1_PRESSED || event.bstate & BUTTON1_CLICKED)) {
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
                } else if (event.bstate & BUTTON4_PRESSED) {
                    for (int i = 0; i < 3; ++i) {
                        // Allow scrolling up as long as there are more lines above
                        if (E.row_offset > 0) {
                            E.row_offset--;
                        }
                    }
                    cursor_moved = true;
                } else if (event.bstate & BUTTON5_PRESSED) {
                    for (int i = 0; i < 3; ++i) {
                        // Allow scrolling down as long as there are more lines to show below current view
                        // The max row offset should allow showing the end of the file in the bottom of the screen
                        int max_offset = E.num_lines > E.screen_rows ? E.num_lines - E.screen_rows : 0;
                        if (E.row_offset < max_offset) {
                            E.row_offset++;
                        }
                    }
                    cursor_moved = true;
                } else if (event.bstate & BUTTON1_PRESSED) {
                    // Calculate clicked position - this should just move the cursor
                    int clicked_cy = event.y + E.row_offset;
                    int target_display_cx = event.x + E.col_offset;
                    int clicked_cx = 0;

                    if (clicked_cy < E.num_lines) {
                        EditorLine *line = &E.lines[clicked_cy];
                        int current_display_cx = 0;
                        for (int char_idx = 0; char_idx < (int)line->len; char_idx++) {
                            int char_display_width = 1;
                            if (line->text[char_idx] == '\t') {
                                char_display_width = TAB_STOP - (current_display_cx % TAB_STOP);
                            }
                            if (current_display_cx + char_display_width > target_display_cx) {
                                break;
                            }
                            current_display_cx += char_display_width;
                            clicked_cx = char_idx + 1;
                        }
                    }

                    if (clicked_cy >= E.num_lines) {
                        clicked_cy = E.num_lines > 0 ? E.num_lines - 1 : 0;
                    }
                    EditorLine *line = (clicked_cy < E.num_lines) ? &E.lines[clicked_cy] : NULL;
                    int line_len = line ? (int)line->len : 0;
                    if (clicked_cx > line_len) {
                        clicked_cx = line_len;
                    }

                    // Store the click location for potential dragging
                    E.cy = clicked_cy;
                    E.cx = clicked_cx;

                    // For single click, immediately disable selection
                    E.selection_active = false;
                    // Don't set selection coordinates for single click

                    cursor_moved = true;
                } else if ((event.bstate & REPORT_MOUSE_POSITION) && (event.bstate & BUTTON1_PRESSED)) {
                    // Handle mouse dragging for text selection when mouse moves with left button held down
                    int drag_cy = event.y + E.row_offset;
                    int target_display_cx = event.x + E.col_offset;
                    int drag_cx = 0;

                    if (drag_cy < E.num_lines) {
                        EditorLine *line = &E.lines[drag_cy];
                        int current_display_cx = 0;
                        for (int char_idx = 0; char_idx < (int)line->len; char_idx++) {
                            int char_display_width = 1;
                            if (line->text[char_idx] == '\t') {
                                char_display_width = TAB_STOP - (current_display_cx % TAB_STOP);
                            }
                            if (current_display_cx + char_display_width > target_display_cx) {
                                break;
                            }
                            current_display_cx += char_display_width;
                            drag_cx = char_idx + 1;
                        }
                    }

                    if (drag_cy >= E.num_lines) {
                        drag_cy = E.num_lines > 0 ? E.num_lines - 1 : 0;
                    }
                    EditorLine *line = (drag_cy < E.num_lines) ? &E.lines[drag_cy] : NULL;
                    int line_len = line ? (int)line->len : 0;
                    if (drag_cx > line_len) {
                        drag_cx = line_len;
                    }

                    // Enable selection and update the end position
                    E.selection_active = true;
                    // If we just started dragging, set the initial position as the start
                    if (E.selection_start_cy == 0 && E.selection_start_cx == 0 &&
                        E.selection_end_cy == 0 && E.selection_end_cx == 0) {
                        // In practice, we should store the original click position, but for now
                        // assume the start of selection is the position where dragging started
                        E.selection_start_cy = E.cy;  // Position when drag started
                        E.selection_start_cx = E.cx;
                    }

                    // Update the end position of the selection
                    E.selection_end_cy = drag_cy;
                    E.selection_end_cx = drag_cx;
                    E.cy = drag_cy;
                    E.cx = drag_cx;
                    cursor_moved = true;
                } else if (event.bstate & BUTTON3_PRESSED) {
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
