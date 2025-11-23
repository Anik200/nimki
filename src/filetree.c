#include "common.h"

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
    }
    // Always refresh screen when toggling file tree to prevent rendering issues
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
