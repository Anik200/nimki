#ifndef COMMON_H
#define COMMON_H

#define _GNU_SOURCE

#include<ncurses.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<errno.h>
#include<time.h>
#include<stdarg.h>
#include<ctype.h>
#include<unistd.h>
#include<sys/wait.h>
#include<signal.h>
#include<stdbool.h>
#include<math.h>
#include<dirent.h>
#include<sys/stat.h>
#include<limits.h>

#define EDITOR_VERSION "0.1.4"
#define TAB_STOP 4

#define CTRL(k) ((k) & 0x1f)

#define MAX_UNDO_STATES 20

#define FILE_TREE_WIDTH 30

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

extern EditorConfig E;

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

extern FileTreeState FT;
extern EditorSyntax *E_syntax;

// Function declarations
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
void free_file_tree(FileTreeNode *node);
void refresh_flat_file_tree();
void draw_file_tree();
void toggle_file_tree();
void file_tree_move_cursor(int direction);
void file_tree_toggle_expand();
void file_tree_open_file();
int get_cx_display();
void editor_scroll();
void initialize_syntax_colors();

#endif
