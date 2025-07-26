#define _GNU_SOURCE

#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <stdarg.h>
#include <ctype.h>
#include <unistd.h> 
#include <sys/wait.h>
#include <signal.h>   

#define EDITOR_VERSION "0.1.0"
#define TAB_STOP 4

#define CTRL(k) ((k) & 0x1f)

enum EditorHighlight {
    HL_NORMAL = 0,
    HL_COMMENT,
    HL_KEYWORD1,
    HL_KEYWORD2,
    HL_STRING,
    HL_NUMBER,
    HL_MATCH,
    HL_PREPROC
};

typedef struct {
    char **filetype_extensions;
    char **keywords1;
    char **keywords2;
    char *singleline_comment_start;
    char *multiline_comment_start;
    char *multiline_comment_end;
} EditorSyntax;

typedef struct {
    char *text;
    size_t len;
    char *hl;
    int hl_open_comment;
} EditorLine;

typedef struct {
    EditorLine *lines;
    int num_lines;
    int cx, cy;
    int row_offset;
    int col_offset;
    int screen_rows, screen_cols;
    char *filename;
    int dirty;
    int select_all_active;
} EditorConfig;

EditorConfig E;

EditorSyntax *E_syntax = NULL;

char *C_HL_extensions[] = { ".c", ".h", ".cpp", ".hpp", ".cc", NULL };
char *C_HL_keywords[] = {
    "switch", "if", "while", "for", "break", "continue", "return", "else",
    "goto", "auto", "register", "extern", "const", "unsigned", "signed",
    "volatile", "do", "typeof", "_Bool", "_Complex", "_Imaginary",
    "case", "default", "sizeof", "enum", "union", "struct", "typedef", NULL
};
char *C_HL_types[] = {
    "int", "char", "float", "double", "void", "long", "short", NULL
};
EditorSyntax C_syntax = {
    C_HL_extensions,
    C_HL_keywords,
    C_HL_types,
    "//",
    "/*",
    "*/",
};

char *SH_HL_extensions[] = { ".sh", NULL };
char *SH_HL_keywords[] = {
    "if", "then", "else", "fi", "for", "in", "do", "done", "while", "until",
    "case", "esac", "function", "return", "export", "local", "read", "echo",
    "printf", "test", "exit", "break", "continue", "set", "unset", "trap",
    "eval", "exec", "source", ".", NULL
};
char *SH_HL_types[] = { NULL };
EditorSyntax SH_syntax = {
    SH_HL_extensions,
    SH_HL_keywords,
    SH_HL_types,
    "#",
    NULL,
    NULL,
};

char *JS_HL_extensions[] = { ".js", NULL };
char *JS_HL_keywords[] = {
    "function", "var", "let", "const", "if", "else", "for", "while", "do",
    "return", "break", "continue", "switch", "case", "default", "try",
    "catch", "finally", "throw", "new", "this", "super", "class", "extends",
    "import", "export", "await", "async", "yield", "typeof", "instanceof",
    "delete", "in", "void", "debugger", "with", NULL
};
char *JS_HL_types[] = { NULL };
EditorSyntax JS_syntax = {
    JS_HL_extensions,
    JS_HL_keywords,
    JS_HL_types,
    "//",
    "/*",
    "*/",
};

char *HTML_HL_extensions[] = { ".html", ".htm", NULL };
char *HTML_HL_keywords[] = {
    "html", "head", "body", "title", "meta", "link", "script", "style", "div",
    "p", "a", "img", "ul", "ol", "li", "table", "tr", "td", "th", "form",
    "input", "button", "span", "h1", "h2", "h3", "h4", "h5", "h6", "br", "hr",
    "em", "strong", "b", "i", "code", "pre", NULL
};
char *HTML_HL_types[] = { NULL };
EditorSyntax HTML_syntax = {
    HTML_HL_extensions,
    HTML_HL_keywords,
    HTML_HL_types,
    NULL,
    "<!--",
    "-->",
};

char *CSS_HL_extensions[] = { ".css", NULL };
char *CSS_HL_keywords[] = {
    "color", "background-color", "font-size", "margin", "padding", "border",
    "display", "position", "width", "height", "top", "right", "bottom", "left",
    "text-align", "line-height", "font-family", "font-weight", "float", "clear",
    "overflow", "z-index", "opacity", "transform", "transition", "animation",
    "selector", "class", "id", "media", "keyframes", "from", "to", "important", NULL
};
char *CSS_HL_types[] = { NULL };
EditorSyntax CSS_syntax = {
    CSS_HL_extensions,
    CSS_HL_keywords,
    CSS_HL_types,
    NULL,
    "/*",
    "*/",
};

char *XML_HL_extensions[] = { ".xml", NULL };
char *XML_HL_keywords[] = {
    "xml", "version", "encoding", "root", "element", "attribute", "value",
    "item", "data", "note", "to", "from", "heading", "body", NULL
};
char *XML_HL_types[] = { NULL };
EditorSyntax XML_syntax = {
    XML_HL_extensions,
    XML_HL_keywords,
    XML_HL_types,
    NULL,
    "<!--",
    "-->",
};


EditorSyntax *EditorSyntaxes[] = {
    &C_syntax,
    &SH_syntax,
    &JS_syntax,
    &HTML_syntax,
    &CSS_syntax,
    &XML_syntax,
    NULL
};


void init_editor();
void cleanup_editor();
void editor_read_file(const char *filename);
void editor_save_file();
void editor_draw_rows();
void editor_refresh_screen();
void editor_move_cursor(int key);
void editor_process_keypress();
void editor_insert_char(int c);
int editor_insert_newline(); // Modified to return int for success/failure
void editor_del_char();
void editor_set_status_message(const char *fmt, ...);
int get_cx_display();
void editor_select_syntax_highlight();
void editor_update_syntax(int filerow);
int is_separator(int c);
char *editor_prompt(const char *prompt_fmt, ...);
void paste_from_clipboard();
void handle_winch(int sig); // Prototype for SIGWINCH handler
void editor_draw_clock(); // Prototype for the new clock function

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

    initscr();
    raw();
    noecho();
    keypad(stdscr, TRUE);
    // Removed nodelay(stdscr, TRUE); to make getch() blocking again

    // Register SIGWINCH handler
    signal(SIGWINCH, handle_winch);

    getmaxyx(stdscr, E.screen_rows, E.screen_cols);
    E.screen_rows -= 2; // Reserve space for status and message bars

    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(HL_NORMAL, COLOR_WHITE, COLOR_BLACK);
        init_pair(HL_COMMENT, COLOR_CYAN, COLOR_BLACK);
        init_pair(HL_KEYWORD1, COLOR_YELLOW, COLOR_BLACK);
        init_pair(HL_KEYWORD2, COLOR_GREEN, COLOR_BLACK);
        init_pair(HL_STRING, COLOR_MAGENTA, COLOR_BLACK);
        init_pair(HL_NUMBER, COLOR_RED, COLOR_BLACK);
        init_pair(HL_MATCH, COLOR_BLACK, COLOR_YELLOW);
        init_pair(HL_PREPROC, COLOR_BLUE, COLOR_BLACK);
    }

    // Enable mouse events for clicks and wheel
    mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
}

void cleanup_editor() {
    endwin();

    if (E.lines) {
        for (int i = 0; i < E.num_lines; ++i) {
            free(E.lines[i].text);
            free(E.lines[i].hl);
        }
        free(E.lines);
    }
    if (E.filename) {
        free(E.filename);
    }
}

void editor_read_file(const char *filename) {
    if (E.filename) free(E.filename);
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
        return;
    }

    char *line_buffer = NULL;
    size_t linecap = 0;
    ssize_t linelen;

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

    for (int i = 0; i < E.num_lines; i++) {
        editor_update_syntax(i);
    }

    E.dirty = 0;
    editor_set_status_message("Opened file: %s (%d lines)", filename, E.num_lines);
}

void editor_save_file() {
    if (!E.filename) {
        char *new_filename = editor_prompt("Save as: %s (ESC to cancel)", "");
        if (new_filename == NULL) {
            editor_set_status_message("Save cancelled.");
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

    for (int i = 0; i < E.num_lines; ++i) {
        fprintf(fp, "%s\n", E.lines[i].text);
    }
    fclose(fp);
    E.dirty = 0;
    editor_set_status_message("File saved: %s", E.filename);
}

void editor_draw_rows() {
    int y;
    for (y = 0; y < E.screen_rows; y++) {
        int filerow = y + E.row_offset;

        if (filerow >= E.num_lines) {
        } else {
            EditorLine *line = &E.lines[filerow];
            int current_color_pair = HL_NORMAL;
            int display_col = 0;

            for (int i = 0; i < line->len; i++) {
                int char_display_width = 1;
                if (line->text[i] == '\t') {
                    char_display_width = TAB_STOP - (display_col % TAB_STOP);
                }

                if (display_col < E.col_offset) {
                    display_col += char_display_width;
                    continue;
                }

                if ((display_col - E.col_offset) >= E.screen_cols) break;

                if (E_syntax && has_colors()) {
                    int hl_type = line->hl[i];
                    if (hl_type != current_color_pair) {
                        attroff(COLOR_PAIR(current_color_pair));
                        current_color_pair = hl_type;
                        attron(COLOR_PAIR(current_color_pair));
                    }
                }

                if (line->text[i] == '\t') {
                    for (int k = 0; k < char_display_width; k++) {
                        mvaddch(y, (display_col - E.col_offset) + k, ' ');
                    }
                } else {
                    mvaddch(y, (display_col - E.col_offset), line->text[i]);
                }
                display_col += char_display_width;
            }
            if (E_syntax && has_colors()) {
                attroff(COLOR_PAIR(current_color_pair));
            }
        }
        clrtoeol();
    }
}

void editor_draw_status_bar() {
    attron(A_REVERSE);

    mvprintw(E.screen_rows, 0, "%.20s - %d lines %s",
             E.filename ? E.filename : "[No Name]", E.num_lines,
             E.dirty ? "(modified)" : "");

    char rstatus[80];
    snprintf(rstatus, sizeof(rstatus), "%d/%d", E.cy + 1, E.num_lines);
    mvprintw(E.screen_rows, E.screen_cols - strlen(rstatus), "%s", rstatus);

    attroff(A_REVERSE);
}

char status_message[80];
time_t status_message_time;

void editor_set_status_message(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(status_message, sizeof(status_message), fmt, ap);
    va_end(ap);
    status_message_time = time(NULL);
}

void editor_draw_message_bar() {
    move(E.screen_rows + 1, 0);
    clrtoeol();

    int msglen = strlen(status_message);
    if (msglen > E.screen_cols) msglen = E.screen_cols;
    if (time(NULL) - status_message_time < 5) {
        mvprintw(E.screen_rows + 1, 0, "%.*s", msglen, status_message);
    }
}

// New function to draw the clock
void editor_draw_clock() {
    time_t rawtime;
    struct tm *info;
    char time_str[6]; // HH:MM\0

    time(&rawtime);
    info = localtime(&rawtime);
    strftime(time_str, sizeof(time_str), "%H:%M", info);

    // Position the clock in the upper right corner
    // Assuming the main editing area starts at (0,0) and status/message bars are at the bottom
    // The clock should be drawn in the top-most line of the terminal, not within E.screen_rows
    // So, it's drawn at row 0.
    int clock_len = strlen(time_str);
    if (E.screen_cols >= clock_len) {
        mvprintw(0, E.screen_cols - clock_len, "%s", time_str);
    }
}


void editor_scroll() {
    if (E.cy < E.row_offset) {
        E.row_offset = E.cy;
    }
    if (E.cy >= E.row_offset + E.screen_rows) {
        E.row_offset = E.cy - E.screen_rows + 1;
    }

    int current_line_len = (E.cy < E.num_lines) ? E.lines[E.cy].len : 0;
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

void editor_refresh_screen() {
    editor_scroll();

    clear(); // Clear the entire screen

    editor_draw_rows();
    editor_draw_status_bar();
    editor_draw_message_bar();
    editor_draw_clock(); // Draw the clock

    move(E.cy - E.row_offset, get_cx_display() - E.col_offset);
    refresh(); // Update the physical screen
}

int get_cx_display() {
    int display_cx = 0;
    if (E.cy >= E.num_lines) return 0;

    EditorLine *line = &E.lines[E.cy];
    for (int i = 0; i < E.cx; i++) {
        if (i >= line->len) break;
        if (line->text[i] == '\t') {
            display_cx += (TAB_STOP - (display_cx % TAB_STOP));
        } else {
            display_cx++;
        }
    }
    return display_cx;
}

void editor_move_cursor(int key) {
    EditorLine *line = (E.cy >= E.num_lines) ? NULL : &E.lines[E.cy];

    switch (key) {
        case KEY_LEFT:
            if (E.cx > 0) {
                E.cx--;
            } else if (E.cy > 0) {
                E.cy--;
                E.cx = E.lines[E.cy].len;
            }
            break;
        case KEY_RIGHT:
            if (line && E.cx < line->len) {
                E.cx++;
            } else if (line && E.cx == line->len && E.cy < E.num_lines - 1) {
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
            if (line) E.cx = line->len;
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
    int line_len = line ? line->len : 0;
    if (E.cx > line_len) {
        E.cx = line_len;
    }
}

void editor_insert_char(int c) {
    // If at the end of the file, try to insert a new line first
    if (E.cy == E.num_lines) {
        if (editor_insert_newline() == -1) {
            // If inserting a new line failed, we cannot insert the character
            editor_set_status_message("Error: Failed to prepare new line for character insertion.");
            return;
        }
    }

    // Ensure E.lines is not NULL and E.cy is a valid index after potential newline insertion
    if (E.lines == NULL || E.cy >= E.num_lines) {
        editor_set_status_message("Internal error: Invalid line state for character insertion.");
        return;
    }

    EditorLine *line = &E.lines[E.cy];
    // Reallocate memory for the line text to accommodate the new character + null terminator
    line->text = realloc(line->text, line->len + 2);
    if (line->text == NULL) {
        editor_set_status_message("Error: Out of memory for line %d.", E.cy);
        return; // Do not exit, try to continue
    }
    // Shift existing characters to the right to make space for the new character
    memmove(&line->text[E.cx + 1], &line->text[E.cx], line->len - E.cx + 1);
    line->text[E.cx] = c; // Insert the new character
    line->len++;          // Increment line length
    E.cx++;               // Move cursor to the right
    E.dirty = 1;          // Mark file as modified

    editor_update_syntax(E.cy); // Update syntax highlighting for the current line
}

// Returns 0 on success, -1 on failure
int editor_insert_newline() {
    // Case 1: Inserting a new line into an empty file
    if (E.num_lines == 0) {
        E.lines = malloc(sizeof(EditorLine));
        if (E.lines == NULL) {
            editor_set_status_message("Error: Out of memory for initial lines array.");
            return -1;
        }
        E.lines[0].text = strdup("");
        if (E.lines[0].text == NULL) {
            free(E.lines); // Clean up partially allocated memory
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

    // Reallocate E.lines to make space for a new line
    E.lines = realloc(E.lines, (E.num_lines + 1) * sizeof(EditorLine));
    if (E.lines == NULL) {
        editor_set_status_message("Error: Out of memory for lines array (realloc).");
        return -1;
    }

    // Shift existing lines down to make space for the new line
    // If inserting at the end of the file (E.cy == E.num_lines), this memmove does nothing, which is correct.
    memmove(&E.lines[E.cy + 1], &E.lines[E.cy], (E.num_lines - E.cy) * sizeof(EditorLine));

    // Initialize the new line
    E.lines[E.cy].hl = NULL;
    E.lines[E.cy].hl_open_comment = 0;

    // If cursor is at the beginning of a line, insert an empty line
    if (E.cx == 0) {
        E.lines[E.cy].text = strdup("");
        if (E.lines[E.cy].text == NULL) {
            // Revert memmove if possible, or handle gracefully
            editor_set_status_message("Error: Out of memory for new empty line text.");
            // This is tricky: if strdup fails, we've already realloc'd and memmoved.
            // For simplicity in a minimalist editor, we'll just report error and leave
            // the lines array in a potentially inconsistent state (a line with NULL text).
            // A more robust editor would need to undo the realloc/memmove.
            return -1;
        }
        E.lines[E.cy].len = 0;
    } else { // Split the current line
        EditorLine *current_line = &E.lines[E.cy]; // This is the line *before* it was potentially overwritten by memmove
        // Need to copy the part of the line *after* the cursor to the new line
        E.lines[E.cy + 1].len = current_line->len - E.cx;
        E.lines[E.cy + 1].text = strdup(&current_line->text[E.cx]);
        if (E.lines[E.cy + 1].text == NULL) {
            editor_set_status_message("Error: Out of memory for split line text.");
            return -1;
        }
        E.lines[E.cy + 1].hl = NULL; // New line needs its own highlight array
        E.lines[E.cy + 1].hl_open_comment = 0;

        // Truncate the current line
        current_line->text = realloc(current_line->text, E.cx + 1);
        if (current_line->text == NULL) {
            editor_set_status_message("Error: Out of memory for truncated line.");
            return -1;
        }
        current_line->text[E.cx] = '\0';
        current_line->len = E.cx;
    }

    E.num_lines++;
    E.cy++;
    E.cx = 0;
    E.dirty = 1;

    editor_update_syntax(E.cy - 1); // Update syntax for the line that was potentially truncated
    editor_update_syntax(E.cy);     // Update syntax for the newly inserted line

    return 0; // Success
}


void editor_del_char() {
    if (E.select_all_active) {
        if (E.lines) {
            for (int i = 0; i < E.num_lines; ++i) {
                free(E.lines[i].text);
                free(E.lines[i].hl);
            }
            free(E.lines);
        }
        E.lines = malloc(sizeof(EditorLine));
        if (E.lines == NULL) {
            editor_set_status_message("Fatal error: Out of memory (clear all init).");
            exit(1); // Exit on critical failure
        }
        E.lines[0].text = strdup("");
        if (E.lines[0].text == NULL) {
            free(E.lines);
            E.lines = NULL;
            editor_set_status_message("Fatal error: Out of memory (clear all text).");
            exit(1); // Exit on critical failure
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

    if (E.cy == E.num_lines || E.num_lines == 0) return; // No lines to delete from
    if (E.cx == 0 && E.cy == 0 && E.lines[0].len == 0) return; // Only an empty first line

    EditorLine *line = &E.lines[E.cy];
    if (E.cx > 0) { // Delete character before cursor on current line
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
    } else { // Delete newline (merge current line with previous)
        if (E.cy > 0) {
            EditorLine *prev_line = &E.lines[E.cy - 1];
            // Reallocate previous line to hold its content + current line's content
            prev_line->text = realloc(prev_line->text, prev_line->len + line->len + 1);
            if (prev_line->text == NULL) {
                editor_set_status_message("Error: Out of memory (merge line realloc).");
                return;
            }
            // Copy current line's content to the end of the previous line
            memcpy(&prev_line->text[prev_line->len], line->text, line->len);
            prev_line->len += line->len;
            prev_line->text[prev_line->len] = '\0'; // Null-terminate the merged line

            free(line->text); // Free current line's text
            free(line->hl);   // Free current line's highlight data

            // Shift subsequent lines up
            memmove(&E.lines[E.cy], &E.lines[E.cy + 1], (E.num_lines - E.cy - 1) * sizeof(EditorLine));
            E.num_lines--;
            E.lines = realloc(E.lines, E.num_lines * sizeof(EditorLine));
            // If E.num_lines becomes 0, lines could be NULL, but that's handled at the top of this function
            if (E.num_lines > 0 && E.lines == NULL) {
                editor_set_status_message("Error: Out of memory shrinking lines realloc.");
                return;
            }

            // If all lines were deleted, ensure there's at least one empty line
            if (E.num_lines == 0) {
                E.lines = malloc(sizeof(EditorLine));
                if (E.lines == NULL) {
                    editor_set_status_message("Fatal error: Out of memory (empty file init).");
                    exit(1); // Exit on critical failure
                }
                E.lines[0].text = strdup("");
                if (E.lines[0].text == NULL) {
                    free(E.lines);
                    E.lines = NULL;
                    editor_set_status_message("Fatal error: Out of memory (empty file text).");
                    exit(1); // Exit on critical failure
                }
                E.lines[0].len = 0;
                E.lines[0].hl = NULL;
                E.lines[0].hl_open_comment = 0;
                E.num_lines = 1;
                E.cx = 0;
                E.cy = 0;
                editor_update_syntax(0);
            } else {
                // Adjust cursor position to the end of the merged line
                E.cx = prev_line->len; // Set cursor to the end of the previous line
                E.cy--; // Move cursor to the previous line
                editor_update_syntax(E.cy); // Update syntax for the merged line
            }
            E.dirty = 1;
        }
    }
}

char *editor_prompt(const char *prompt_fmt, ...) {
    char buffer[128];
    int buflen = 0;
    buffer[0] = '\0';

    while (1) {
        editor_set_status_message(prompt_fmt, buffer);
        editor_refresh_screen(); // Always refresh to show prompt

        int c = getch(); // This getch() should block for prompt input
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

void paste_from_clipboard() {
    int pipefd[2];
    pid_t pid;
    char buffer[1024];
    ssize_t bytes_read;
    char *clipboard_tool = NULL;
    char *argv[3];
    int tool_found = 0;

    // Try wl-paste first for Wayland
    // Check if XDG_SESSION_TYPE is "wayland" or if wl-paste is found in common paths
    if (getenv("XDG_SESSION_TYPE") != NULL && strcmp(getenv("XDG_SESSION_TYPE"), "wayland") == 0) {
        if (access("/usr/bin/wl-paste", X_OK) == 0 || access("/bin/wl-paste", X_OK) == 0) {
            clipboard_tool = "wl-paste";
            argv[0] = "wl-paste";
            argv[1] = NULL; // wl-paste doesn't need -o
            tool_found = 1;
        }
    }
    
    // Fallback to xclip for X11 if Wayland tools not found or not Wayland session
    if (!tool_found) {
        if (access("/usr/bin/xclip", X_OK) == 0 || access("/bin/xclip", X_OK) == 0) {
            clipboard_tool = "xclip";
            argv[0] = "xclip";
            argv[1] = "-o";
            argv[2] = NULL;
            tool_found = 1;
        }
    }

    if (!tool_found) {
        editor_set_status_message("Paste error: Neither wl-paste nor xclip found. Please install one.");
        return;
    }

    editor_set_status_message("Attempting to paste using %s...", clipboard_tool);
    editor_refresh_screen();

    // Flush stdout before forking to avoid ncurses issues with subshells
    fflush(stdout);

    if (pipe(pipefd) == -1) {
        editor_set_status_message("Paste error: Failed to create pipe.");
        return;
    }

    pid = fork();
    if (pid == -1) {
        editor_set_status_message("Paste error: Failed to fork process.");
        close(pipefd[0]);
        close(pipefd[1]);
        return;
    }

    if (pid == 0) { // Child process
        close(pipefd[0]); // Close read end
        dup2(pipefd[1], STDOUT_FILENO); // Redirect stdout to pipe write end
        close(pipefd[1]); // Close original write end

        execvp(clipboard_tool, argv);
        
        // If execvp fails
        _exit(1); // Use _exit in child process after fork
    } else { // Parent process
        close(pipefd[1]); // Close write end

        // Read from pipe
        while ((bytes_read = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytes_read] = '\0'; // Null-terminate the buffer
            for (int i = 0; i < bytes_read; ++i) {
                // Insert character by character, handling newlines
                if (buffer[i] == '\n' || buffer[i] == '\r') {
                    editor_insert_newline();
                } else if (buffer[i] >= 32 && buffer[i] <= 126) { // Only insert printable ASCII
                    editor_insert_char(buffer[i]);
                }
                // Ignore other characters (e.g., non-ASCII, other control characters)
            }
        }
        close(pipefd[0]); // Close read end

        int status;
        waitpid(pid, &status, 0); // Wait for child process to finish

        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            editor_set_status_message("Pasted from clipboard using %s.", clipboard_tool);
        } else {
            editor_set_status_message("Paste error: %s failed or returned an error.", clipboard_tool);
        }
    }
}

// Signal handler for window resize events
void handle_winch(int sig) {
    (void)sig; // Suppress unused parameter warning
    endwin(); // End ncurses mode temporarily
    refresh(); // Re-initialize ncurses with new window size
    getmaxyx(stdscr, E.screen_rows, E.screen_cols); // Get new dimensions
    E.screen_rows -= 2; // Adjust for status and message bars
    editor_refresh_screen(); // Redraw the editor content
}

void editor_process_keypress() {
    int c = getch(); // This getch() is now blocking again
    bool cursor_moved = false; // Track if cursor moved to trigger refresh
    int original_cx = E.cx;
    int original_cy = E.cy;

    if (E.select_all_active && c != KEY_BACKSPACE && c != 127 && c != KEY_DC) {
        E.select_all_active = 0;
        editor_set_status_message("");
    }

    switch (c) {
        case CTRL('q'):
        case CTRL('c'):
            if (E.dirty) {
                editor_set_status_message("WARNING! File has unsaved changes. Press Ctrl+Q/C again to force quit.");
                editor_refresh_screen(); // Refresh to show warning
                int c2 = getch(); // This getch() should block for confirmation
                if (c2 != CTRL('q') && c2 != CTRL('c')) return;
            }
            cleanup_editor();
            exit(0);
            break;

        case CTRL('s'):
            editor_save_file();
            break;

        case CTRL('a'):
            E.select_all_active = 1;
            E.cx = 0;
            E.cy = 0;
            editor_set_status_message("All text selected. Press Backspace to delete.");
            cursor_moved = true; // Cursor moved, needs refresh
            break;

        case CTRL('v'):
            paste_from_clipboard();
            // Pasting modifies content and cursor, so refresh is needed
            break;

        case KEY_BACKSPACE:
        case KEY_DC:
        case 127:
            editor_del_char();
            break;

        case '\r':
        case '\n':
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
            cursor_moved = true; // Cursor moved, needs refresh
            break;

        case KEY_MOUSE:
            {
                MEVENT event;
                if (getmouse(&event) == OK) {
                    if (event.bstate & BUTTON1_CLICKED) {
                        E.cy = event.y + E.row_offset;
                        
                        int target_display_cx = event.x + E.col_offset;
                        int actual_cx = 0;
                        if (E.cy < E.num_lines) {
                            EditorLine *line = &E.lines[E.cy];
                            int current_display_cx = 0;
                            for (int char_idx = 0; char_idx < line->len; char_idx++) {
                                int char_display_width = 1;
                                if (line->text[char_idx] == '\t') {
                                    char_display_width = TAB_STOP - (current_display_cx % TAB_STOP);
                                }
                                if (current_display_cx + char_display_width > target_display_cx) {
                                    break;
                                }
                                current_display_cx += char_display_width;
                                actual_cx = char_idx + 1;
                            }
                        }
                        E.cx = actual_cx;

                        if (E.cy >= E.num_lines) {
                            E.cy = E.num_lines > 0 ? E.num_lines - 1 : 0;
                        }
                        EditorLine *line = (E.cy < E.num_lines) ? &E.lines[E.cy] : NULL;
                        int line_len = line ? line->len : 0;
                        if (E.cx > line_len) {
                            E.cx = line_len;
                        }
                        cursor_moved = true; // Cursor moved by click
                    } else if (event.bstate & BUTTON4_PRESSED) { // Mouse wheel up
                        for (int i = 0; i < 3; ++i) { // Scroll 3 lines up
                            editor_move_cursor(KEY_UP);
                        }
                        cursor_moved = true;
                    } else if (event.bstate & BUTTON5_PRESSED) { // Mouse wheel down
                        for (int i = 0; i < 3; ++i) { // Scroll 3 lines down
                            editor_move_cursor(KEY_DOWN);
                        }
                        cursor_moved = true;
                    }
                }
            }
            break; // Break from KEY_MOUSE case
        default:
            if (c >= 32 && c <= 126) {
                 editor_insert_char(c);
            }
            break;
    }

    // Only refresh if something changed (content, cursor, or status message)
    // The dirty flag handles content changes. Cursor changes are tracked by original_cx/cy.
    // Status message changes are handled by editor_set_status_message.
    if (E.dirty || cursor_moved || original_cx != E.cx || original_cy != E.cy || time(NULL) - status_message_time < 5) {
        editor_refresh_screen();
    }
}

int is_separator(int c) {
    return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[];", c) != NULL;
}

void editor_select_syntax_highlight() {
    E_syntax = NULL;

    if (E.filename) {
        char *ext = strrchr(E.filename, '.');

        if (ext) {
            for (int i = 0; EditorSyntaxes[i]; i++) {
                EditorSyntax *syntax = EditorSyntaxes[i];
                for (int j = 0; syntax->filetype_extensions[j]; j++) {
                    if (strcmp(ext, syntax->filetype_extensions[j]) == 0) {
                        E_syntax = syntax;
                        return;
                    }
                }
            }
        }
    }
}

void editor_update_syntax(int filerow) {
    EditorLine *line = &E.lines[filerow];

    if (line->hl) free(line->hl);
    line->hl = malloc(line->len);
    if (line->hl == NULL) { return; }
    memset(line->hl, HL_NORMAL, line->len);

    if (E_syntax == NULL) return;

    char **keywords1 = E_syntax->keywords1;
    char **keywords2 = E_syntax->keywords2;
    char *sc_start = E_syntax->singleline_comment_start;
    char *mc_start = E_syntax->multiline_comment_start;
    char *mc_end = E_syntax->multiline_comment_end;

    int prev_sep = 1;
    int in_string = 0;
    int in_multiline_comment = (filerow > 0 && E.lines[filerow - 1].hl_open_comment);

    int i = 0;
    while (i < line->len) {
        char c = line->text[i];
        unsigned char prev_hl = (i > 0) ? line->hl[i-1] : HL_NORMAL;

        if (mc_start && mc_end) {
            if (in_multiline_comment) {
                line->hl[i] = HL_COMMENT;
                if (strncmp(&line->text[i], mc_end, strlen(mc_end)) == 0) {
                    for (size_t j = 0; j < strlen(mc_end); j++) line->hl[i+j] = HL_COMMENT;
                    i += strlen(mc_end);
                    in_multiline_comment = 0;
                    prev_sep = 1;
                    continue;
                }
                i++;
                continue;
            } else if (strncmp(&line->text[i], mc_start, strlen(mc_start)) == 0) {
                for (size_t j = 0; j < strlen(mc_start); j++) line->hl[i+j] = HL_COMMENT;
                i += strlen(mc_start);
                in_multiline_comment = 1;
                continue;
            }
        }

        if (sc_start && strncmp(&line->text[i], sc_start, strlen(sc_start)) == 0) {
            for (size_t j = i; j < line->len; j++) {
                line->hl[j] = HL_COMMENT;
            }
            break;
        }

        if (in_string) {
            line->hl[i] = HL_STRING;
            if (c == '\\' && i + 1 < line->len) {
                line->hl[i+1] = HL_STRING;
                i += 2;
                continue;
            }
            if (c == in_string) {
                in_string = 0;
            }
            i++;
            prev_sep = 0;
            continue;
        } else {
            if (c == '"' || c == '\'') {
                in_string = c;
                line->hl[i] = HL_STRING;
                i++;
                prev_sep = 0;
                continue;
            }
        }

        if (isdigit(c) && (prev_sep || prev_hl == HL_NUMBER)) {
            line->hl[i] = HL_NUMBER;
            i++;
            prev_sep = 0;
            continue;
        }

        if (i == 0 && c == '#') {
            for (size_t j = 0; j < line->len; j++) {
                line->hl[j] = HL_PREPROC;
            }
            break;
        }

        if (prev_sep) {
            for (size_t k = 0; keywords1[k]; k++) {
                size_t kwlen = strlen(keywords1[k]);
                if (strncmp(&line->text[i], keywords1[k], kwlen) == 0 &&
                    is_separator(line->text[i + kwlen])) {
                    for (size_t j = 0; j < kwlen; j++) line->hl[i+j] = HL_KEYWORD1;
                    i += kwlen;
                    prev_sep = 0;
                    goto next_char_in_loop;
                }
            }
            for (size_t k = 0; keywords2[k]; k++) {
                size_t kwlen = strlen(keywords2[k]);
                if (strncmp(&line->text[i], keywords2[k], kwlen) == 0 &&
                    is_separator(line->text[i + kwlen])) {
                    for (size_t j = 0; j < kwlen; j++) line->hl[i+j] = HL_KEYWORD2;
                    i += kwlen;
                    prev_sep = 0;
                    goto next_char_in_loop;
                }
            }
        }

        prev_sep = is_separator(c);
        i++;
        next_char_in_loop:;
    }

    int changed_comment_state = (line->hl_open_comment != in_multiline_comment);
    line->hl_open_comment = in_multiline_comment;

    if (changed_comment_state && filerow + 1 < E.num_lines) {
        editor_update_syntax(filerow + 1);
    }
}

int main(int argc, char *argv[]) {
    init_editor();

    if (argc >= 2) {
        editor_read_file(argv[1]);
    } else {
        E.lines = malloc(sizeof(EditorLine));
        if (E.lines == NULL) {
            cleanup_editor();
            fprintf(stderr, "Fatal error: out of memory (main empty line).\n");
            exit(1);
        }
        E.lines[0].text = strdup("");
        if (E.lines[0].text == NULL) {
            cleanup_editor();
            fprintf(stderr, "Fatal error: out of memory (main empty line text).\n");
            exit(1);
        }
        E.lines[0].len = 0;
        E.lines[0].hl = NULL;
        E.lines[0].hl_open_comment = 0;
        E.num_lines = 1;
        editor_update_syntax(0);
        editor_set_status_message("Welcome to Nimki! Press Ctrl+Q to quit. Ctrl+S to save.");
    }

    while (1) {
        editor_process_keypress();
    }

    return 0;
}
