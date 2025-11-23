#include "common.h"
#include <pwd.h>
#include <sys/stat.h>
#include <stdint.h>

extern EditorConfig E;
extern EditorSyntax *E_syntax;

typedef struct {
    int hl_normal;
    int hl_comment;
    int hl_keyword1;
    int hl_keyword2;
    int hl_string;
    int hl_number;
    int hl_match;
    int hl_preproc;
    int hl_selection;
} SyntaxColors;

SyntaxColors SYNTAX_COLORS;

void load_config();
char* get_home_directory();
void create_default_config_file(const char* config_path);
int hex_to_ansi_color(const char* hex);

int hex_to_ansi_color(const char* hex) {
    if (!hex) return COLOR_WHITE;

    if (isdigit(hex[0]) || (hex[0] == '-' && isdigit(hex[1]))) {
        int color = atoi(hex);
        if (color >= -1 && color <= 255) return color;
        return COLOR_WHITE;
    }

    if (hex[0] == '#' && strlen(hex) >= 7) {
        char r_str[3] = {hex[1], hex[2], '\0'};
        char g_str[3] = {hex[3], hex[4], '\0'};
        char b_str[3] = {hex[5], hex[6], '\0'};

        int r = strtol(r_str, NULL, 16);
        int g = strtol(g_str, NULL, 16);
        int b = strtol(b_str, NULL, 16);

        int r_idx = (int)((double)r / 255.0 * 5);
        int g_idx = (int)((double)g / 255.0 * 5);
        int b_idx = (int)((double)b / 255.0 * 5);

        if (r_idx > 5) r_idx = 5;
        if (g_idx > 5) g_idx = 5;
        if (b_idx > 5) b_idx = 5;

        int ansi_code = 16 + 36 * r_idx + 6 * g_idx + b_idx;

        return ansi_code;
    }

    if (strcmp(hex, "black") == 0) return COLOR_BLACK;
    if (strcmp(hex, "red") == 0) return COLOR_RED;
    if (strcmp(hex, "green") == 0) return COLOR_GREEN;
    if (strcmp(hex, "yellow") == 0) return COLOR_YELLOW;
    if (strcmp(hex, "blue") == 0) return COLOR_BLUE;
    if (strcmp(hex, "magenta") == 0) return COLOR_MAGENTA;
    if (strcmp(hex, "cyan") == 0) return COLOR_CYAN;
    if (strcmp(hex, "white") == 0) return COLOR_WHITE;

    return COLOR_WHITE;
}

void init_syntax_colors() {
    SYNTAX_COLORS.hl_normal = COLOR_WHITE;
    SYNTAX_COLORS.hl_comment = COLOR_CYAN;
    SYNTAX_COLORS.hl_keyword1 = COLOR_YELLOW;
    SYNTAX_COLORS.hl_keyword2 = COLOR_GREEN;
    SYNTAX_COLORS.hl_string = COLOR_MAGENTA;
    SYNTAX_COLORS.hl_number = COLOR_RED;
    SYNTAX_COLORS.hl_match = COLOR_BLACK;
    SYNTAX_COLORS.hl_preproc = COLOR_BLUE;
    SYNTAX_COLORS.hl_selection = COLOR_WHITE;
}

void load_config() {
    init_syntax_colors();

    char *home_dir = get_home_directory();
    if (!home_dir) return;

    char config_path[512];
    snprintf(config_path, sizeof(config_path), "%s/.nimkirc", home_dir);

    FILE *config_file = fopen(config_path, "r");
    if (!config_file) {
        create_default_config_file(config_path);
        return;
    }

    char line[512];
    while (fgets(line, sizeof(line), config_file)) {
        line[strcspn(line, "\n")] = 0;

        if (line[0] == '#' || line[0] == '\0') continue;

        if (strncmp(line, "hl_normal=", 10) == 0) {
            SYNTAX_COLORS.hl_normal = hex_to_ansi_color(line + 10);
        }
        else if (strncmp(line, "hl_comment=", 11) == 0) {
            SYNTAX_COLORS.hl_comment = hex_to_ansi_color(line + 11);
        }
        else if (strncmp(line, "hl_keyword1=", 12) == 0) {
            SYNTAX_COLORS.hl_keyword1 = hex_to_ansi_color(line + 12);
        }
        else if (strncmp(line, "hl_keyword2=", 12) == 0) {
            SYNTAX_COLORS.hl_keyword2 = hex_to_ansi_color(line + 12);
        }
        else if (strncmp(line, "hl_string=", 10) == 0) {
            SYNTAX_COLORS.hl_string = hex_to_ansi_color(line + 10);
        }
        else if (strncmp(line, "hl_number=", 10) == 0) {
            SYNTAX_COLORS.hl_number = hex_to_ansi_color(line + 10);
        }
        else if (strncmp(line, "hl_match=", 9) == 0) {
            SYNTAX_COLORS.hl_match = hex_to_ansi_color(line + 9);
        }
        else if (strncmp(line, "hl_preproc=", 11) == 0) {
            SYNTAX_COLORS.hl_preproc = hex_to_ansi_color(line + 11);
        }
        else if (strncmp(line, "hl_selection=", 13) == 0) {
            SYNTAX_COLORS.hl_selection = hex_to_ansi_color(line + 13);
        }
    }
    fclose(config_file);
}

char* get_home_directory() {
    char *home_dir = getenv("HOME");
    if (!home_dir) {
        struct passwd *pw = getpwuid(getuid());
        if (pw) {
            home_dir = pw->pw_dir;
        }
    }
    return home_dir;
}

void create_default_config_file(const char* config_path) {
    FILE *config_file = fopen(config_path, "w");
    if (!config_file) return;

    fprintf(config_file,
        "# Nimki Configuration File\n"
        "# You can use hex colors, named colors, or ANSI numbers\n\n"

        "hl_normal=#FFFFFF\n"
        "hl_comment=#008080\n"
        "hl_keyword1=#FFFF00\n"
        "hl_keyword2=#00FF00\n"
        "hl_string=#FF00FF\n"
        "hl_number=#FF0000\n"
        "hl_match=#000000\n"
        "hl_preproc=#0000FF\n"
        "hl_selection=#FFFFFF\n"
        "\n# Restart Nimki after editing for changes to take effect\n"
    );

    fclose(config_file);
}

void initialize_syntax_colors() {
    load_config();

    // Reinitialize color pairs with loaded values
    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(HL_NORMAL, SYNTAX_COLORS.hl_normal, COLOR_BLACK);
        init_pair(HL_COMMENT, SYNTAX_COLORS.hl_comment, COLOR_BLACK);
        init_pair(HL_KEYWORD1, SYNTAX_COLORS.hl_keyword1, COLOR_BLACK);
        init_pair(HL_KEYWORD2, SYNTAX_COLORS.hl_keyword2, COLOR_BLACK);
        init_pair(HL_STRING, SYNTAX_COLORS.hl_string, COLOR_BLACK);
        init_pair(HL_NUMBER, SYNTAX_COLORS.hl_number, COLOR_BLACK);
        init_pair(HL_MATCH, SYNTAX_COLORS.hl_match, COLOR_YELLOW);
        init_pair(HL_PREPROC, SYNTAX_COLORS.hl_preproc, COLOR_BLACK);
        init_pair(HL_SELECTION, SYNTAX_COLORS.hl_selection, COLOR_BLUE);
    }
}
