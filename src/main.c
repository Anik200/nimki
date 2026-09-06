#include"common.h"

extern EditorConfig E;

int main(int argc, char *argv[]) {
    if (argc >= 2) {
        if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0) {
            printf("nimki %s\n", EDITOR_VERSION);
            return 0;
        }
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            printf("Usage: nimki [filename]\n");
            return 0;
        }
    }

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
