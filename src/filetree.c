#include "common.h"

#ifdef _WIN32
#ifndef strcasecmp
#define strcasecmp _stricmp
#endif
#endif

extern EditorConfig E;
extern FileTreeState FT;

void draw_file_tree();
void refresh_flat_file_tree();
int get_node_depth(FileTreeNode *node);

FileTreeNode *create_file_tree_node(const char *path, bool is_dir) {
    FileTreeNode *node = malloc(sizeof(FileTreeNode));
    if (!node) return NULL;

    node->path = strdup(path);
    if (!node->path) {
        free(node);
        return NULL;
    }

    const char *slash = strrchr(path, '/');
#ifdef _WIN32
    const char *bslash = strrchr(path, '\\');
    if (!slash || (bslash && bslash > slash)) slash = bslash;
#endif

    if (slash && *(slash + 1) != '\0') {
        node->name = strdup(slash + 1);
    } else {
        node->name = strdup(path);
    }

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

static int compare_nodes(const void *a, const void *b) {
    FileTreeNode *node_a = *(FileTreeNode **)a;
    FileTreeNode *node_b = *(FileTreeNode **)b;
    if (node_a->is_dir != node_b->is_dir) {
        return node_b->is_dir - node_a->is_dir;
    }
    return strcasecmp(node_a->name, node_b->name);
}

void load_directory_children(FileTreeNode *node) {
    if (!node || !node->is_dir || node->children != NULL) return;

    DIR *dir = opendir(node->path);
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        if (strcmp(entry->d_name, ".git") == 0)
            continue;

        char child_path[PATH_MAX];
#ifdef _WIN32
        snprintf(child_path, sizeof(child_path), "%s\\%s", node->path, entry->d_name);
#else
        snprintf(child_path, sizeof(child_path), "%s/%s", node->path, entry->d_name);
#endif

        struct stat st;
        bool is_dir = false;
        if (stat(child_path, &st) == 0) {
            is_dir = S_ISDIR(st.st_mode);
        }

        FileTreeNode *child = create_file_tree_node(child_path, is_dir);
        if (child) {
            FileTreeNode **new_children = realloc(node->children, (node->num_children + 1) * sizeof(FileTreeNode *));
            if (!new_children) {
                free_file_tree(child);
                break;
            }
            node->children = new_children;
            node->children[node->num_children++] = child;
        }
    }
    closedir(dir);

    if (node->children && node->num_children > 1) {
        qsort(node->children, node->num_children, sizeof(FileTreeNode *), compare_nodes);
    }
}

FileTreeNode *load_directory_tree(const char *path) {
    FileTreeNode *node = create_file_tree_node(path, true);
    if (!node) return NULL;
    load_directory_children(node);
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
    for (const char *c = node->path; *c; c++) {
        if (*c == '/' || *c == '\\') depth++;
    }
    if (FT.root && FT.root->path) {
        int root_depth = 0;
        for (const char *c = FT.root->path; *c; c++) {
            if (*c == '/' || *c == '\\') root_depth++;
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

        for (int x = 0; x < FILE_TREE_WIDTH - 1; x++) {
            mvaddch(y, x, ' ');
        }

        int indent = get_node_depth(node) * 2;
        if (indent > 16) indent = 16;

        int prefix_len = node->is_dir ? 4 : 2;
        int max_name_len = (FILE_TREE_WIDTH - 1) - indent - prefix_len;
        if (max_name_len < 0) max_name_len = 0;

        if (node->is_dir) {
            mvprintw(y, indent, "[%c] %.*s", node->expanded ? '-' : '+', max_name_len, node->name);
        } else {
            mvprintw(y, indent, "  %.*s", max_name_len, node->name);
        }

        if (i == E.file_tree_cursor) {
            mvchgat(y, 0, FILE_TREE_WIDTH - 1, A_REVERSE, 0, NULL);
        }
    }

    for (int i = end; i < max_rows; i++) {
        int y = i - start;
        for (int x = 0; x < FILE_TREE_WIDTH - 1; x++) {
            mvaddch(y, x, ' ');
        }
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
            size_t len = strlen(cwd);
            while (len > 1 && (cwd[len - 1] == '/' || cwd[len - 1] == '\\')) {
#ifdef _WIN32
                if (len == 3 && cwd[1] == ':') break;
#endif
                cwd[len - 1] = '\0';
                len--;
            }
            FT.root = load_directory_tree(cwd);
            if (FT.root) {
                FT.root->expanded = true;
                refresh_flat_file_tree();
                E.file_tree_cursor = 0;
                E.file_tree_offset = 0;
            }
        }
    }
    editor_refresh_screen();
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
        if (!node->expanded) {
            if (!node->children) {
                load_directory_children(node);
            }
            node->expanded = true;
        } else {
            node->expanded = false;
        }
        refresh_flat_file_tree();
        if (E.file_tree_cursor >= FT.flat_node_count)
            E.file_tree_cursor = FT.flat_node_count - 1;
        editor_refresh_screen();
    }
}

void file_tree_open_file() {
    if (!E.file_tree_visible || !FT.flat_nodes || E.file_tree_cursor >= FT.flat_node_count) return;

    FileTreeNode *node = FT.flat_nodes[E.file_tree_cursor];
    if (node->is_dir) {
        file_tree_toggle_expand();
    } else {
        editor_read_file(node->path);
        toggle_file_tree();
    }
}

void editor_find() {
    char *query = editor_prompt("Search (ESC to cancel): %s",
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
        if (current_row < 0 || current_row >= E.num_lines || !E.lines) break;

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
            E.cx = (int)(match - line->text);
            E.last_match_row = E.cy;
            E.last_match_col = E.cx;
            editor_set_status_message("/%s [%d:%d]", E.search_query, E.cy + 1, E.cx + 1);
            editor_refresh_screen();
            return;
        }

        if (direction == 1) {
            current_row++;
            current_col = 0;
        } else {
            current_row--;
            current_col = (current_row >= 0) ? (int)E.lines[current_row].len - 1 : 0;
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
    editor_set_status_message("Pattern not found: %s", E.search_query);
    E.last_match_row = -1;
    E.last_match_col = -1;
    editor_refresh_screen();
}
