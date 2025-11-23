#include "common.h"

extern EditorConfig E;
extern FileTreeState FT;

char status_message[80];
time_t status_message_time;

void editor_draw_rows();
void editor_draw_status_bar();
void editor_draw_message_bar();
void editor_draw_clock();
void editor_draw_context_menu();
void editor_refresh_screen();
void editor_scroll();

void editor_draw_rows() {
    if (E.file_tree_visible) {
        draw_file_tree();
    }

    int x_offset = E.file_tree_visible ? FILE_TREE_WIDTH : 0;
    int y;
    int line_num_width = 0;
    if (E.show_line_numbers) {
        int num_digits = 1;
        if (E.num_lines > 0) {
            num_digits = (int)log10(E.num_lines) + 1;
        }
        line_num_width = num_digits + 1;
    }

    for (y = 0; y < E.screen_rows; y++) {
        int filerow = y + E.row_offset;

        move(y, x_offset);
        clrtoeol();

        if (filerow >= E.num_lines) {
        } else {
            EditorLine *line = &E.lines[filerow];
            int current_color_pair = HL_NORMAL;
            int display_col = 0;

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

            if (E.show_line_numbers) {
                attron(COLOR_PAIR(HL_COMMENT));
                mvprintw(y, x_offset, "%*d ", line_num_width - 1, filerow + 1);
                attroff(COLOR_PAIR(HL_COMMENT));
            }

            int text_cols = E.screen_cols - x_offset - line_num_width;

            for (int i = 0; i < (int)line->len; i++) {
                int char_display_width = 1;
                if (line->text[i] == '\t') {
                    char_display_width = TAB_STOP - (display_col % TAB_STOP);
                }

                if (display_col < E.col_offset) {
                    display_col += char_display_width;
                    continue;
                }

                if ((display_col - E.col_offset) >= text_cols) break;

                bool is_selected = false;

                if (is_selected && has_colors()) {
                    if (HL_SELECTION != current_color_pair) {
                        attroff(COLOR_PAIR(current_color_pair));
                        current_color_pair = HL_SELECTION;
                        attron(COLOR_PAIR(current_color_pair));
                    }
                } else {
                    if (E_syntax && has_colors()) {
                        int hl_type = line->hl[i];
                        if (hl_type != current_color_pair) {
                            attroff(COLOR_PAIR(current_color_pair));
                            current_color_pair = hl_type;
                            attron(COLOR_PAIR(current_color_pair));
                        }
                    }
                }

                if (line->text[i] == '\t') {
                    for (int k = 0; k < char_display_width; k++) {
                        mvaddch(y, x_offset + (display_col - E.col_offset) + line_num_width + k, ' ');
                    }
                } else {
                    mvaddch(y, x_offset + (display_col - E.col_offset) + line_num_width, line->text[i]);
                }
                display_col += char_display_width;
            }
            if (E_syntax && has_colors()) {
                attroff(COLOR_PAIR(current_color_pair));
            }
        }
    }
}

void editor_draw_status_bar() {
    attron(A_REVERSE);

    int x_offset = E.file_tree_visible ? FILE_TREE_WIDTH : 0;
    int max_width = E.screen_cols - x_offset;

    mvprintw(E.screen_rows, x_offset, "%.*s - %d lines %s",
             max_width - 15,
             E.filename ? E.filename : "[No Name]", E.num_lines,
             E.dirty ? "(modified)" : "");

    char rstatus[80];
    snprintf(rstatus, sizeof(rstatus), "%d/%d", E.cy + 1, E.num_lines);
    mvprintw(E.screen_rows, x_offset + max_width - strlen(rstatus), "%s", rstatus);

    attroff(A_REVERSE);
}

void editor_draw_message_bar() {
    int x_offset = E.file_tree_visible ? FILE_TREE_WIDTH : 0;
    move(E.screen_rows + 1, x_offset);
    clrtoeol();

    int msglen = strlen(status_message);
    int max_width = E.screen_cols - x_offset;
    if (msglen > max_width) msglen = max_width;
    if (time(NULL) - status_message_time < 5) {
        mvprintw(E.screen_rows + 1, x_offset, "%.*s", msglen, status_message);
    }
}

void editor_draw_clock() {
    time_t rawtime;
    struct tm *info;
    char time_str[6];

    time(&rawtime);
    info = localtime(&rawtime);
    strftime(time_str, sizeof(time_str), "%H:%M", info);

    int clock_len = strlen(time_str);
    if (E.screen_cols >= clock_len) {
        mvprintw(0, E.screen_cols - clock_len, "%s", time_str);
    }
}

void editor_refresh_screen() {
    editor_scroll();

    editor_draw_rows();
    editor_draw_status_bar();
    editor_draw_message_bar();
    editor_draw_clock();
    editor_draw_context_menu();

    move(E.cy - E.row_offset, get_cx_display() - E.col_offset);
    doupdate();
}

void editor_set_status_message(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(status_message, sizeof(status_message), fmt, ap);
    va_end(ap);
    status_message_time = time(NULL);
}

void editor_draw_context_menu() {
    if (!E.context_menu_active) return;

    const char *options[] = {"Copy", "Select All", NULL};
    int num_options = 0;
    for (int i = 0; options[i] != NULL; i++) {
        num_options++;
    }

    size_t menu_width = 0;
    for (int i = 0; options[i] != NULL; i++) {
        if (strlen(options[i]) > menu_width) {
            menu_width = strlen(options[i]);
        }
    }
    menu_width += 4;

    int menu_height = num_options + 2;

    int start_x = E.context_menu_x;
    int start_y = E.context_menu_y;

    if (start_x + (int)menu_width >= E.screen_cols) {
        start_x = E.screen_cols - (int)menu_width - 1;
    }
    if (start_y + menu_height >= E.screen_rows + 2) {
        start_y = E.screen_rows + 2 - menu_height -1;
    }
    if (start_x < 0) start_x = 0;
    if (start_y < 0) start_y = 0;

    attron(A_REVERSE);
    for (int y = 0; y < menu_height; y++) {
        mvhline(start_y + y, start_x, ' ', (int)menu_width);
    }
    attroff(A_REVERSE);

    for (int i = 0; i < num_options; i++) {
        if (i == E.context_menu_selected_option) {
            attron(A_REVERSE | A_BOLD);
        } else {
            attron(A_REVERSE);
        }
        mvprintw(start_y + 1 + i, start_x + 2, "%s", options[i]);
        attroff(A_REVERSE | A_BOLD);
    }
}
