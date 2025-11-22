
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
#include <stdbool.h>
#include <math.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>

#define EDITOR_VERSION "0.1.3"
#define TAB_STOP 4

#define CTRL(k) ((k) & 0x1f)

#define MAX_UNDO_STATES 20

enum EditorHighlight {
    HL_NORMAL = 0,
    HL_COMMENT,
    HL_KEYWORD1,
    HL_KEYWORD2,
    HL_STRING,
    HL_NUMBER,
    HL_MATCH,
    HL_PREPROC,
    HL_SELECTION
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
    int dirty;
} EditorStateSnapshot;


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

    EditorStateSnapshot undo_history[MAX_UNDO_STATES];
    int undo_history_len;
    int undo_history_idx;

    char *search_query;
    int search_direction;
    int last_match_row;
    int last_match_col;
    bool find_active;

    bool selection_active;
    int selection_start_cy, selection_start_cx;
    int selection_end_cy, selection_end_cx;

    bool context_menu_active;
    int context_menu_x, context_menu_y;
    int context_menu_selected_option;
    bool show_line_numbers;

    bool file_tree_visible;
    int file_tree_cursor;
    int file_tree_offset;
    int file_tree_width;
} EditorConfig;

EditorConfig E;

#define FILE_TREE_WIDTH 30

typedef struct FileTreeNode {
    char *name;
    char *path;
    bool is_dir;
    bool expanded;
    struct FileTreeNode **children;
    int num_children;
    int parent_index;
} FileTreeNode;

typedef struct {
    FileTreeNode *root;
    FileTreeNode **flat_nodes;
    int flat_node_count;
    int max_nodes;
} FileTreeState;

FileTreeState FT = {NULL, NULL, 0, 0};

EditorSyntax *E_syntax = NULL;

FileTreeNode *create_file_tree_node(const char *path, bool is_dir);
void free_file_tree(FileTreeNode *node);
FileTreeNode *load_directory_tree(const char *path);
void flatten_file_tree(FileTreeNode *node, FileTreeNode ***array, int *count, int *capacity);
void refresh_flat_file_tree();
int get_node_depth(FileTreeNode *node);
void draw_file_tree();
void toggle_file_tree();
void file_tree_move_cursor(int direction);
void file_tree_toggle_expand();
void file_tree_open_file();

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
int editor_insert_newline();
void editor_del_char();
void editor_set_status_message(const char *fmt, ...);
int get_cx_display();
void editor_select_syntax_highlight();
void editor_update_syntax(int filerow);
int is_separator(int c);
char *editor_prompt(const char *prompt_fmt, ...);
void paste_from_clipboard();
void handle_winch(int sig);
void editor_draw_clock();

void editor_free_snapshot(EditorStateSnapshot *snapshot);
void editor_save_state();
void editor_undo();
void editor_find();
void editor_find_next(int direction);
void editor_copy_selection_to_clipboard();
void editor_select_all();
void editor_draw_context_menu();


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
        init_pair(HL_SELECTION, COLOR_WHITE, COLOR_BLUE);
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
            editor_set_status_message("Undo error: Out of memory restoring line text.");
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
            editor_set_status_message("Undo error: Out of memory restoring line hl.");
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
                if (E.selection_active) {
                    if (filerow >= sel_min_cy && filerow <= sel_max_cy) {
                        if (filerow == sel_min_cy && filerow == sel_max_cy) {
                            if (i >= sel_min_cx && i < sel_max_cx) {
                                is_selected = true;
                            }
                        } else if (filerow == sel_min_cy) {
                            if (i >= sel_min_cx) {
                                is_selected = true;
                            }
                        } else if (filerow == sel_max_cy) {
                            if (i < sel_max_cx) {
                                is_selected = true;
                            }
                        } else {
                            is_selected = true;
                        }
                    }
                }

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


void editor_scroll() {
    if (E.cy < E.row_offset) {
        E.row_offset = E.cy;
    }
    if (E.cy >= E.row_offset + E.screen_rows) {
        E.row_offset = E.cy - E.screen_rows + 1;
    }

    int current_line_len = (E.cy < E.num_lines) ? (int)E.lines[E.cy].len : 0;
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

    editor_draw_rows();
    editor_draw_status_bar();
    editor_draw_message_bar();
    editor_draw_clock();
    editor_draw_context_menu();

    move(E.cy - E.row_offset, get_cx_display() - E.col_offset);
    doupdate();
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

    editor_update_syntax(E.cy - 1);
    editor_update_syntax(E.cy);

    return 0;
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
            editor_set_status_message("Fatal error: Out of memory (clear all text).");
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
                editor_set_status_message("Fatal error: Out of memory (empty file init after sel del).");
                exit(1);
            }
            E.lines[0].text = strdup("");
            if (E.lines[0].text == NULL) {
                free(E.lines);
                E.lines = NULL;
                editor_set_status_message("Fatal error: Out of memory (empty file text after sel del).");
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
                    editor_set_status_message("Fatal error: Out of memory (empty file init after single del).");
                    exit(1);
                }
                E.lines[0].text = strdup("");
                if (E.lines[0].text == NULL) {
                    free(E.lines);
                    E.lines = NULL;
                    editor_set_status_message("Fatal error: Out of memory (empty file text after single del).");
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

void paste_from_clipboard() {
    editor_save_state();
    int pipefd[2];
    pid_t pid;
    char buffer[1024];
    ssize_t bytes_read;
    char *clipboard_tool = NULL;
    char *argv[3];
    int tool_found = 0;

    if (getenv("XDG_SESSION_TYPE") != NULL && strcmp(getenv("XDG_SESSION_TYPE"), "wayland") == 0) {
        if (access("/usr/bin/wl-paste", X_OK) == 0 || access("/bin/wl-paste", X_OK) == 0) {
            clipboard_tool = "wl-paste";
            argv[0] = "wl-paste";
            argv[1] = NULL;
            tool_found = 1;
        }
    }
    
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

    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);

        execvp(clipboard_tool, argv);
        
        _exit(1);
    } else {
        close(pipefd[1]);

        while ((bytes_read = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytes_read] = '\0';
            for (int i = 0; i < bytes_read; ++i) {
                if (buffer[i] == '\n' || buffer[i] == '\r') {
                    editor_insert_newline();
                } else if (buffer[i] >= 32 && buffer[i] <= 126) {
                    editor_insert_char(buffer[i]);
                }
            }
        }
        close(pipefd[0]);

        int status;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            editor_set_status_message("Pasted from clipboard using %s.", clipboard_tool);
        } else {
            editor_set_status_message("Paste error: %s failed or returned an error.", clipboard_tool);
        }
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

    int pipefd[2];
    pid_t pid;
    char *clipboard_tool = NULL;
    char *argv[4];

    if (getenv("XDG_SESSION_TYPE") != NULL && strcmp(getenv("XDG_SESSION_TYPE"), "wayland") == 0) {
        if (access("/usr/bin/wl-copy", X_OK) == 0 || access("/bin/wl-copy", X_OK) == 0) {
            clipboard_tool = "wl-copy";
            argv[0] = "wl-copy";
            argv[1] = NULL;
        }
    }
    
    if (clipboard_tool == NULL) {
        if (access("/usr/bin/xclip", X_OK) == 0 || access("/bin/xclip", X_OK) == 0) {
            clipboard_tool = "xclip";
            argv[0] = "xclip";
            argv[1] = "-selection";
            argv[2] = "clipboard";
            argv[3] = NULL;
        }
    }

    if (clipboard_tool == NULL) {
        editor_set_status_message("Copy error: Neither wl-copy nor xclip found. Please install one.");
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


void handle_winch(int sig) {
    (void)sig;
    endwin();
    refresh();
    getmaxyx(stdscr, E.screen_rows, E.screen_cols);
    E.screen_rows -= 2;
    editor_refresh_screen();
}

void editor_find_next(int direction) {
    if (E.search_query == NULL) return;

    int current_row = E.last_match_row;
    int current_col = E.last_match_col;

    if (current_row == -1) {
        current_row = E.cy;
        current_col = E.cx;
        E.search_direction = direction;
    } else {
        current_col += direction;
    }

    size_t query_len = strlen(E.search_query);
    int original_row = current_row;
    int original_col = current_col;

    while (1) {
        if (current_row < 0 || current_row >= E.num_lines) break;

        EditorLine *line = &E.lines[current_row];
        char *match = NULL;

        if (direction == 1) {
            if (current_col >= (int)line->len) {
                current_row++;
                current_col = 0;
                continue;
            }
            match = strstr(line->text + current_col, E.search_query);
        } else {
            if (current_col < 0) {
                current_row--;
                if (current_row < 0) break;
                current_col = (int)E.lines[current_row].len - 1;
                continue;
            }
            for (int i = current_col; i >= 0; i--) {
                if ((size_t)i + query_len <= line->len && strncmp(line->text + i, E.search_query, query_len) == 0) {
                    match = line->text + i;
                    break;
                }
            }
        }

        if (match) {
            E.cy = current_row;
            E.cx = match - line->text;
            E.last_match_row = E.cy;
            E.last_match_col = E.cx;
            editor_set_status_message("Found '%s' at %d:%d", E.search_query, E.cy + 1, E.cx + 1);
            editor_refresh_screen();
            return;
        }

        if (direction == 1) {
            current_row++;
            current_col = 0;
        } else {
            current_row--;
            current_col = (int)E.lines[current_row].len - 1;
        }

        if (current_row >= E.num_lines) {
            current_row = 0;
            current_col = 0;
        } else if (current_row < 0) {
            current_row = E.num_lines - 1;
            current_col = (int)E.lines[E.num_lines - 1].len - 1;
        }

        if (current_row == original_row && current_col == original_col) {
            break;
        }
    }
    editor_set_status_message("No more matches for '%s'", E.search_query);
    E.last_match_row = -1;
    E.last_match_col = -1;
    editor_refresh_screen();
}

void editor_find() {
    char *query = editor_prompt("Search (Use arrows to navigate, ESC to cancel): %s",
                                 E.search_query ? E.search_query : "");

    if (query == NULL) {
        editor_set_status_message("");
        E.find_active = false;
        for (int i = 0; i < E.num_lines; i++) {
            editor_update_syntax(i);
        }
        editor_refresh_screen();
        return;
    }

    if (E.search_query) {
        if (strcmp(E.search_query, query) != 0) {
            free(E.search_query);
            E.search_query = query;
            E.last_match_row = -1;
            E.last_match_col = -1;
        } else {
            free(query);
        }
    } else {
        E.search_query = query;
        E.last_match_row = -1;
        E.last_match_col = -1;
    }

    E.find_active = true;
    editor_find_next(1);
}

void editor_select_all() {
    E.selection_active = true;
    E.selection_start_cy = 0;
    E.selection_start_cx = 0;
    E.selection_end_cy = E.num_lines > 0 ? E.num_lines - 1 : 0;
    E.selection_end_cx = E.num_lines > 0 ? (int)E.lines[E.selection_end_cy].len : 0;
    editor_set_status_message("All text selected.");
    for (int i = 0; i < E.num_lines; i++) {
        editor_update_syntax(i);
    }
    editor_refresh_screen();
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


FileTreeNode *create_file_tree_node(const char *path, bool is_dir) {
    FileTreeNode *node = malloc(sizeof(FileTreeNode));
    if (!node) return NULL;

    node->path = strdup(path);
    if (!node->path) {
        free(node);
        return NULL;
    }

    const char *slash = strrchr(path, '/');
    node->name = strdup(slash ? slash + 1 : path);
    if (!node->name) {
        free(node->path);
        free(node);
        return NULL;
    }

    node->is_dir = is_dir;
    node->expanded = false;
    node->children = NULL;
    node->num_children = 0;
    node->parent_index = -1;

    return node;
}

void free_file_tree(FileTreeNode *node) {
    if (!node) return;

    for (int i = 0; i < node->num_children; i++) {
        free_file_tree(node->children[i]);
    }
    free(node->children);
    free(node->name);
    free(node->path);
    free(node);
}

FileTreeNode *load_directory_tree(const char *path) {
    FileTreeNode *node = create_file_tree_node(path, true);
    if (!node) return NULL;

    struct stat st;
    if (stat(path, &st) == -1) {
        node->is_dir = false;
        return node;
    }
    node->is_dir = S_ISDIR(st.st_mode);

    if (!node->is_dir) return node;

    DIR *dir = opendir(path);
    if (!dir) return node;

    struct dirent *entry;
    node->children = NULL;
    node->num_children = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char child_path[PATH_MAX];
        snprintf(child_path, sizeof(child_path), "%s/%s", path, entry->d_name);

        FileTreeNode *child = load_directory_tree(child_path);
        if (child) {
            node->num_children++;
            node->children = realloc(node->children, node->num_children * sizeof(FileTreeNode *));
            if (!node->children) {
                free_file_tree(child);
                closedir(dir);
                return node;
            }
            node->children[node->num_children - 1] = child;
        }
    }
    closedir(dir);
    return node;
}

void flatten_file_tree(FileTreeNode *node, FileTreeNode ***array, int *count, int *capacity) {
    if (*count >= *capacity) {
        *capacity = (*capacity == 0) ? 64 : *capacity * 2;
        FileTreeNode **new_array = realloc(*array, (*capacity) * sizeof(FileTreeNode *));
        if (!new_array) return;
        *array = new_array;
    }
    (*array)[(*count)++] = node;
    if (node->is_dir && node->expanded) {
        for (int i = 0; i < node->num_children; i++) {
            flatten_file_tree(node->children[i], array, count, capacity);
        }
    }
}

void refresh_flat_file_tree() {
    if (FT.flat_nodes) {
        free(FT.flat_nodes);
    }

    FT.flat_nodes = NULL;
    FT.flat_node_count = 0;
    int capacity = 0;

    if (FT.root) {
        flatten_file_tree(FT.root, &FT.flat_nodes, &FT.flat_node_count, &capacity);
    }
}

int get_node_depth(FileTreeNode *node) {
    if (!node || !node->path) return 0;

    int depth = 0;
    const char *path = node->path;
    for (const char *c = path; *c; c++) {
        if (*c == '/') depth++;
    }
    if (FT.root && FT.root->path) {
        const char *root_path = FT.root->path;
        int root_depth = 0;
        for (const char *c = root_path; *c; c++) {
            if (*c == '/') root_depth++;
        }
        depth -= root_depth;
        if (depth < 0) depth = 0;
    }
    return depth;
}

void draw_file_tree() {
    if (!E.file_tree_visible || !FT.flat_nodes) return;

    int max_rows = E.screen_rows;
    int start = E.file_tree_offset;
    int end = start + max_rows;
    if (end > FT.flat_node_count) end = FT.flat_node_count;

    for (int i = start; i < end; i++) {
        FileTreeNode *node = FT.flat_nodes[i];
        int y = i - start;
        move(y, 0);
        clrtoeol();

        int indent = get_node_depth(node) * 2;
        if (indent > 20) indent = 20;

        if (node->is_dir) {
            if (node->expanded)
                mvprintw(y, indent, "[-] %s", node->name);
            else
                mvprintw(y, indent, "[+] %s", node->name);
        } else {
            mvprintw(y, indent, " %s", node->name);
        }

        if (i == E.file_tree_cursor) {
            attron(A_REVERSE);
            mvchgat(y, 0, -1, A_REVERSE, 0, NULL);
            attroff(A_REVERSE);
        }
    }

    for (int i = end; i < max_rows; i++) {
        move(i, 0);
        clrtoeol();
    }

    for (int y = 0; y < E.screen_rows; y++) {
        mvaddch(y, FILE_TREE_WIDTH - 1, ACS_VLINE);
    }
}

void toggle_file_tree() {
    E.file_tree_visible = !E.file_tree_visible;
    if (E.file_tree_visible) {
        if (!FT.root) {
            char cwd[PATH_MAX];
            if (!getcwd(cwd, sizeof(cwd))) strcpy(cwd, ".");
            FT.root = load_directory_tree(cwd);
            if (FT.root) {
                FT.root->expanded = true;
                refresh_flat_file_tree();
                E.file_tree_cursor = 0;
                E.file_tree_offset = 0;
            }
        }
    } else {
    }
}

void file_tree_move_cursor(int direction) {
    if (!E.file_tree_visible || !FT.flat_nodes) return;

    E.file_tree_cursor += direction;
    if (E.file_tree_cursor < 0) E.file_tree_cursor = 0;
    if (E.file_tree_cursor >= FT.flat_node_count)
        E.file_tree_cursor = FT.flat_node_count - 1;

    if (E.file_tree_cursor < E.file_tree_offset) {
        E.file_tree_offset = E.file_tree_cursor;
    } else if (E.file_tree_cursor >= E.file_tree_offset + E.screen_rows) {
        E.file_tree_offset = E.file_tree_cursor - E.screen_rows + 1;
    }
}

void file_tree_toggle_expand() {
    if (!E.file_tree_visible || !FT.flat_nodes || E.file_tree_cursor >= FT.flat_node_count) return;

    FileTreeNode *node = FT.flat_nodes[E.file_tree_cursor];
    if (node->is_dir) {
        node->expanded = !node->expanded;
        refresh_flat_file_tree();
        if (E.file_tree_cursor >= FT.flat_node_count)
            E.file_tree_cursor = FT.flat_node_count - 1;
    }
}

void file_tree_open_file() {
    if (!E.file_tree_visible || !FT.flat_nodes || E.file_tree_cursor >= FT.flat_node_count) return;

    FileTreeNode *node = FT.flat_nodes[E.file_tree_cursor];
    if (!node->is_dir) {
        editor_read_file(node->path);
        toggle_file_tree();
    }
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
        editor_refresh_screen();
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
            paste_from_clipboard();
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
                editor_set_status_message("Selection mode active. Move cursor to select. Press Ctrl+K again to copy.");
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
                        if (E.row_offset > 0) {
                            E.row_offset--;
                        }
                    }
                    cursor_moved = true;
                } else if (event.bstate & BUTTON5_PRESSED) {
                    for (int i = 0; i < 3; ++i) {
                        if (E.row_offset < E.num_lines - 1) {
                            E.row_offset++;
                        }
                    }
                    cursor_moved = true;
                } else if (event.bstate & BUTTON1_PRESSED || event.bstate & BUTTON1_CLICKED) {
                    E.cy = event.y + E.row_offset;

                    int target_display_cx = event.x + E.col_offset;
                    int actual_cx = 0;
                    if (E.cy < E.num_lines) {
                        EditorLine *line = &E.lines[E.cy];
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
                            actual_cx = char_idx + 1;
                        }
                    }
                    E.cx = actual_cx;

                    if (E.cy >= E.num_lines) {
                        E.cy = E.num_lines > 0 ? E.num_lines - 1 : 0;
                    }
                    EditorLine *line = (E.cy < E.num_lines) ? &E.lines[E.cy] : NULL;
                    int line_len = line ? (int)line->len : 0;
                    if (E.cx > line_len) {
                        E.cx = line_len;
                    }
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
    if (filerow < 0 || filerow >= E.num_lines) return;

    EditorLine *line = &E.lines[filerow];

    if (line->hl) {
        free(line->hl);
        line->hl = NULL;
    }
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

    size_t i = 0;
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

    if (E.find_active && E.search_query && filerow >= E.row_offset && filerow < E.row_offset + E.screen_rows) {
        char *match_ptr = line->text;
        while ((match_ptr = strstr(match_ptr, E.search_query)) != NULL) {
            int start_col = match_ptr - line->text;
            for (size_t k = 0; k < strlen(E.search_query); k++) {
                if ((size_t)start_col + k < line->len) {
                    line->hl[start_col + k] = HL_MATCH;
                }
            }
            match_ptr += strlen(E.search_query);
        }
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
            free(E.lines);
            E.lines = NULL;
            cleanup_editor();
            fprintf(stderr, "Fatal error: out of memory (main empty line text).\n");
            exit(1);
        }
        E.lines[0].len = 0;
        E.lines[0].hl = NULL;
        E.lines[0].hl_open_comment = 0;
        E.num_lines = 1;
        editor_update_syntax(0);
        editor_set_status_message("Welcome to Nimki! Press Ctrl+Q to quit. Ctrl+S to save. Ctrl+F to find. Ctrl+K to select/copy. Ctrl+T to toggle line numbers.");
    }
    
    editor_refresh_screen();

    while (1) {
        editor_process_keypress();
    }

    return 0;
}
