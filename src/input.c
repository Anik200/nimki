#include "common.h"

extern EditorConfig E;
extern FileTreeState FT;
extern char status_message[256];
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
        if (clicked_cy < E.num_lines && E.lines) {
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

    if (E.context_menu_active) {
        switch (c) {
            case KEY_UP:
                E.context_menu_selected_option--;
                if (E.context_menu_selected_option < 0) E.context_menu_selected_option = 1;
                break;
            case KEY_DOWN:
                E.context_menu_selected_option++;
                if (E.context_menu_selected_option > 1) E.context_menu_selected_option = 0;
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
        editor_refresh_screen();
        return;
    }

    if (c == KEY_MOUSE) {
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
#ifdef REPORT_MOUSE_POSITION
            } else if (event.bstate & REPORT_MOUSE_POSITION) {
#else
            } else if (0) {
#endif
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
                if (event.y >= 0 && event.y < E.screen_rows) {
                    screen_to_editor_coords(event.x, event.y, &E.cy, &E.cx);
                    E.selection_active = false;
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
        editor_refresh_screen();
        return;
    }

    if (E.file_tree_visible) {
        if (E.vim_enabled) {
            switch (c) {
                case 'j':
                    file_tree_move_cursor(1);
                    editor_refresh_screen();
                    return;
                case 'k':
                    file_tree_move_cursor(-1);
                    editor_refresh_screen();
                    return;
                case 'h':
                case 'l':
                case 'o':
                    file_tree_toggle_expand();
                    editor_refresh_screen();
                    return;
                case 'q':
                    toggle_file_tree();
                    return;
                case 'g':
                    E.file_tree_cursor = 0;
                    editor_refresh_screen();
                    return;
                case 'G':
                    if (FT.flat_node_count > 0) E.file_tree_cursor = FT.flat_node_count - 1;
                    editor_refresh_screen();
                    return;
            }
        }
        switch (c) {
            case KEY_UP:
                file_tree_move_cursor(-1);
                editor_refresh_screen();
                return;
            case KEY_DOWN:
                file_tree_move_cursor(1);
                editor_refresh_screen();
                return;
            case KEY_LEFT:
            case KEY_RIGHT:
            case ' ':
            case '\t':
                file_tree_toggle_expand();
                editor_refresh_screen();
                return;
            case '\r':
            case '\n':
            case KEY_ENTER:
                file_tree_open_file();
                editor_refresh_screen();
                return;
            case 27:
            case CTRL('n'):
                toggle_file_tree();
                return;
            case KEY_HOME:
                E.file_tree_cursor = 0;
                editor_refresh_screen();
                return;
            case KEY_END:
                if (FT.flat_node_count > 0) E.file_tree_cursor = FT.flat_node_count - 1;
                editor_refresh_screen();
                return;
            case KEY_PPAGE:
                file_tree_move_cursor(-E.screen_rows);
                editor_refresh_screen();
                return;
            case KEY_NPAGE:
                file_tree_move_cursor(E.screen_rows);
                editor_refresh_screen();
                return;
        }
        return;
    }

    if (E.find_active) {
        if (E.vim_enabled && (c == 'n' || c == 'N')) {
        } else if (c != KEY_UP && c != KEY_DOWN && c != CTRL('f')) {
            E.find_active = false;
            editor_set_status_message("");
            for (int i = 0; i < E.num_lines; i++) {
                editor_update_syntax(i);
            }
            editor_refresh_screen();
        }
    }

    if (E.find_active && (c == KEY_UP || c == KEY_DOWN)) {
        if (c == KEY_UP) {
            editor_find_next(-1);
        } else if (c == KEY_DOWN) {
            editor_find_next(1);
        }
        editor_refresh_screen();
        return;
    }

    if (E.selection_active && c != CTRL('k') && c != KEY_MOUSE &&
        c != KEY_UP && c != KEY_DOWN && c != KEY_LEFT && c != KEY_RIGHT &&
        (!E.vim_enabled || (c != 'h' && c != 'j' && c != 'k' && c != 'l'))) {
        if (c == KEY_BACKSPACE || c == KEY_DC || c == 127 || c == 8
#ifdef CTL_BKSP
            || c == CTL_BKSP
#endif
        ) {
            editor_del_char();
            editor_refresh_screen();
            return;
        }
        E.selection_active = false;
        editor_set_status_message("");
        for (int i = 0; i < E.num_lines; i++) {
            editor_update_syntax(i);
        }
        editor_refresh_screen();
    }

    if (c == CTRL('q') || c == CTRL('c')) {
        if (E.dirty) {
            editor_set_status_message("WARNING! File has unsaved changes. Press Ctrl+Q again to force quit.");
            editor_refresh_screen();
            int c2 = getch();
            if (c2 != CTRL('q') && c2 != CTRL('c')) return;
        }
        cleanup_editor();
        exit(0);
    }
    if (c == CTRL('s')) {
        editor_save_file();
        return;
    }
    if (c == CTRL('n')) {
        toggle_file_tree();
        return;
    }
    if (c == CTRL('t')) {
        E.show_line_numbers = !E.show_line_numbers;
        editor_set_status_message("Line numbers %s", E.show_line_numbers ? "ON" : "OFF");
        editor_refresh_screen();
        return;
    }
    if (c == CTRL('f') || c == CTRL('w')) {
        editor_find();
        return;
    }
    if (c == CTRL('z')) {
        editor_undo();
        return;
    }
    if (c == CTRL('k')) {
        if (!E.selection_active) {
            E.selection_active = true;
            E.selection_start_cy = E.cy;
            E.selection_start_cx = E.cx;
            E.selection_end_cy = E.cy;
            E.selection_end_cx = E.cx;
            editor_set_status_message("-- VISUAL -- Move cursor to select, press Ctrl+K to copy");
        } else {
            editor_copy_selection_to_clipboard();
        }
        editor_refresh_screen();
        return;
    }
    if (c == CTRL('a')) {
        editor_select_all();
        return;
    }

    if (E.vim_enabled) {
        if (E.vim_mode == VIM_MODE_NORMAL) {
            if (E.pending_cmd == 'd') {
                E.pending_cmd = '\0';
                if (c == 'd') {
                    editor_delete_current_line();
                } else {
                    editor_set_status_message("");
                    editor_refresh_screen();
                }
                return;
            }
            if (E.pending_cmd == 'y') {
                E.pending_cmd = '\0';
                if (c == 'y') {
                    editor_yank_current_line();
                } else {
                    editor_set_status_message("");
                    editor_refresh_screen();
                }
                return;
            }
            if (E.pending_cmd == 'g') {
                E.pending_cmd = '\0';
                if (c == 'g') {
                    E.cy = 0;
                    E.cx = 0;
                    if (E.selection_active) {
                        E.selection_end_cy = E.cy;
                        E.selection_end_cx = E.cx;
                    }
                    editor_refresh_screen();
                } else {
                    editor_set_status_message("");
                    editor_refresh_screen();
                }
                return;
            }

            switch (c) {
                case 'h':
                    editor_move_cursor(KEY_LEFT);
                    break;
                case 'j':
                    editor_move_cursor(KEY_DOWN);
                    break;
                case 'k':
                    editor_move_cursor(KEY_UP);
                    break;
                case 'l':
                    editor_move_cursor(KEY_RIGHT);
                    break;
                case 'w':
                    editor_move_word_forward();
                    break;
                case 'b':
                    editor_move_word_backward();
                    break;
                case 'e':
                    editor_move_word_end();
                    break;
                case '0':
                    E.cx = 0;
                    break;
                case '$':
                    if (E.cy < E.num_lines && E.lines) {
                        int len = (int)E.lines[E.cy].len;
                        E.cx = len > 0 ? len - 1 : 0;
                    }
                    break;
                case '^':
                    if (E.cy < E.num_lines && E.lines) {
                        EditorLine *line = &E.lines[E.cy];
                        E.cx = 0;
                        while (E.cx < (int)line->len && isspace((unsigned char)line->text[E.cx])) {
                            E.cx++;
                        }
                    }
                    break;
                case 'G':
                    E.cy = E.num_lines > 0 ? E.num_lines - 1 : 0;
                    E.cx = 0;
                    break;
                case 'g':
                    E.pending_cmd = 'g';
                    return;
                case CTRL('d'):
                    E.cy += E.screen_rows / 2;
                    if (E.cy >= E.num_lines) E.cy = E.num_lines > 0 ? E.num_lines - 1 : 0;
                    break;
                case CTRL('u'):
                    E.cy -= E.screen_rows / 2;
                    if (E.cy < 0) E.cy = 0;
                    break;
                case 'i':
                    E.vim_mode = VIM_MODE_INSERT;
                    editor_set_status_message("");
                    editor_refresh_screen();
                    return;
                case 'I':
                    if (E.cy < E.num_lines && E.lines) {
                        EditorLine *line = &E.lines[E.cy];
                        E.cx = 0;
                        while (E.cx < (int)line->len && isspace((unsigned char)line->text[E.cx])) {
                            E.cx++;
                        }
                    }
                    E.vim_mode = VIM_MODE_INSERT;
                    editor_set_status_message("");
                    editor_refresh_screen();
                    return;
                case 'a':
                    if (E.cy < E.num_lines && E.lines) {
                        int len = (int)E.lines[E.cy].len;
                        if (E.cx < len) E.cx++;
                    }
                    E.vim_mode = VIM_MODE_INSERT;
                    editor_set_status_message("");
                    editor_refresh_screen();
                    return;
                case 'A':
                    if (E.cy < E.num_lines && E.lines) {
                        E.cx = (int)E.lines[E.cy].len;
                    }
                    E.vim_mode = VIM_MODE_INSERT;
                    editor_set_status_message("");
                    editor_refresh_screen();
                    return;
                case 'o':
                    if (E.cy < E.num_lines && E.lines) {
                        E.cx = (int)E.lines[E.cy].len;
                    }
                    editor_insert_newline();
                    E.vim_mode = VIM_MODE_INSERT;
                    editor_set_status_message("");
                    editor_refresh_screen();
                    return;
                case 'O':
                    if (E.cy < E.num_lines && E.lines) {
                        E.cx = 0;
                        editor_insert_newline();
                        E.cy--;
                    }
                    E.vim_mode = VIM_MODE_INSERT;
                    editor_set_status_message("");
                    editor_refresh_screen();
                    return;
                case 'x':
                    if (E.cy < E.num_lines && E.lines) {
                        EditorLine *line = &E.lines[E.cy];
                        if (E.cx < (int)line->len) {
                            E.cx++;
                            editor_del_char();
                        }
                    }
                    break;
                case 'X':
                case KEY_BACKSPACE:
                case 127:
                case 8:
#ifdef CTL_BKSP
                case CTL_BKSP:
#endif
                    editor_del_char();
                    break;
                case KEY_DC:
                    if (E.cy < E.num_lines && E.lines) {
                        EditorLine *line = &E.lines[E.cy];
                        if (E.cx < (int)line->len) {
                            E.cx++;
                            editor_del_char();
                        }
                    }
                    break;
                case 'd':
                    E.pending_cmd = 'd';
                    return;
                case 'D':
                    if (E.cy < E.num_lines && E.lines) {
                        EditorLine *line = &E.lines[E.cy];
                        if (E.cx < (int)line->len) {
                            editor_save_state();
                            if (E.yank_buffer) free(E.yank_buffer);
                            E.yank_buffer = strdup(&line->text[E.cx]);
                            E.yank_is_line = false;
                            line->text[E.cx] = '\0';
                            line->len = E.cx;
                            E.dirty = 1;
                            editor_update_syntax(E.cy);
                        }
                    }
                    break;
                case 'y':
                    E.pending_cmd = 'y';
                    return;
                case 'p':
                    editor_paste_yank(true);
                    return;
                case 'P':
                    editor_paste_yank(false);
                    return;
                case 'u':
                    editor_undo();
                    return;
                case '/':
                    editor_find();
                    return;
                case 'n':
                    editor_find_next(1);
                    return;
                case 'N':
                    editor_find_next(-1);
                    return;
                case ':':
                    editor_handle_vim_command();
                    return;
                case 27:
                    if (E.selection_active) {
                        E.selection_active = false;
                        for (int i = 0; i < E.num_lines; i++) editor_update_syntax(i);
                    }
                    editor_set_status_message("");
                    break;
                default:
                    break;
            }

            if (E.selection_active) {
                E.selection_end_cy = E.cy;
                E.selection_end_cx = E.cx;
            }
            editor_refresh_screen();
            return;
        }

        if (c == 27) {
            E.vim_mode = VIM_MODE_NORMAL;
            if (E.cx > 0) E.cx--;
            editor_set_status_message("");
            editor_refresh_screen();
            return;
        }
    }

    switch (c) {
        case 27:
            if (E.selection_active) {
                E.selection_active = false;
                for (int i = 0; i < E.num_lines; i++) editor_update_syntax(i);
            }
            editor_set_status_message("");
            break;

        case KEY_BACKSPACE:
        case 127:
        case 8:
#ifdef CTL_BKSP
        case CTL_BKSP:
#endif
            editor_del_char();
            break;

        case KEY_DC:
            if (E.cy < E.num_lines && E.lines) {
                EditorLine *line = &E.lines[E.cy];
                if (E.cx < (int)line->len) {
                    E.cx++;
                    editor_del_char();
                } else if (E.cy < E.num_lines - 1) {
                    E.cy++;
                    E.cx = 0;
                    editor_del_char();
                }
            }
            break;

        case '\t':
            editor_insert_char('\t');
            break;

        case '\r':
        case '\n':
        case KEY_ENTER:
            editor_insert_newline();
            break;

        case KEY_HOME:
        case KEY_END:
        case KEY_PPAGE:
        case KEY_NPAGE:
        case KEY_UP:
        case KEY_DOWN:
        case KEY_LEFT:
        case KEY_RIGHT:
            editor_move_cursor(c);
            if (E.selection_active) {
                E.selection_end_cy = E.cy;
                E.selection_end_cx = E.cx;
            }
            break;

        default:
            if (c >= 32 && c <= 126) {
                editor_insert_char(c);
            } else if (c >= 128) {
                editor_insert_char(c);
            }
            break;
    }

    editor_refresh_screen();
}
