#include "common.h"
#ifndef _WIN32
#include <pwd.h>
#endif
#include <sys/stat.h>
#include <stdint.h>

#ifdef _WIN32
#ifndef strcasecmp
#define strcasecmp _stricmp
#endif
#endif

extern EditorConfig E;
extern EditorSyntax *E_syntax;

typedef struct {
    int bg_color;
    int hl_normal;
    int hl_comment;
    int hl_keyword1;
    int hl_keyword2;
    int hl_string;
    int hl_number;
    int hl_match;
    int hl_preproc;
    int hl_selection;
    int hl_selection_bg;
    bool use_terminal_bg;
    char current_theme[64];
} SyntaxColors;

SyntaxColors SYNTAX_COLORS;

void load_config();
char *get_home_directory();
void create_default_config_file(const char *config_path);
int hex_to_ansi_color(const char *hex);

typedef struct {
    const char *name;
    const char *bg;
    const char *normal;
    const char *comment;
    const char *keyword1;
    const char *keyword2;
    const char *string;
    const char *number;
    const char *match;
    const char *preproc;
    const char *selection;
    const char *selection_bg;
} ThemeDef;

static ThemeDef BUILTIN_THEMES[] = {
    {
        "dracula",
        "#282A36",
        "#F8F8F2",
        "#6272A4",
        "#FF79C6",
        "#BD93F9",
        "#F1FA8C",
        "#FFB86C",
        "#50FA7B",
        "#8BE9FD",
        "#F8F8F2",
        "#44475A"
    },
    {
        "nord",
        "#2E3440",
        "#D8DEE9",
        "#616E88",
        "#81A1C1",
        "#88C0D0",
        "#A3BE8C",
        "#B48EAD",
        "#EBCB8B",
        "#5E81AC",
        "#ECEFF4",
        "#434C5E"
    },
    {
        "monokai",
        "#272822",
        "#F8F8F2",
        "#75715E",
        "#F92672",
        "#66D9EF",
        "#E6DB74",
        "#AE81FF",
        "#A6E22E",
        "#FD971F",
        "#F8F8F2",
        "#49483E"
    },
    {
        "gruvbox",
        "#282828",
        "#EBDBB2",
        "#928374",
        "#FB4934",
        "#FABD2F",
        "#B8BB26",
        "#D3869B",
        "#FE8019",
        "#83A598",
        "#EBDBB2",
        "#504945"
    },
    {
        "tokyo-night",
        "#1A1B26",
        "#C0CAF5",
        "#565F89",
        "#BB9AF7",
        "#7AA2F7",
        "#9ECE6A",
        "#FF9E64",
        "#E0AF68",
        "#7DCFFF",
        "#C0CAF5",
        "#33467C"
    },
    {
        "catppuccin",
        "#1E1E2E",
        "#CDD6F4",
        "#6C7086",
        "#CBA6F7",
        "#89B4FA",
        "#A6E3A1",
        "#FAB387",
        "#F9E2AF",
        "#F5C2E7",
        "#CDD6F4",
        "#585B70"
    },
    {
        "one-dark",
        "#282C34",
        "#ABB2BF",
        "#5C6370",
        "#C678DD",
        "#61AFEF",
        "#98C379",
        "#D19A66",
        "#E5C07B",
        "#E06C75",
        "#ABB2BF",
        "#3E4451"
    },
    {
        "solarized-dark",
        "#002B36",
        "#839496",
        "#586E75",
        "#859900",
        "#268BD2",
        "#2AA198",
        "#D33682",
        "#B58900",
        "#CB4B16",
        "#93A1A1",
        "#073642"
    },
    {
        "solarized-light",
        "#FDF6E3",
        "#657B83",
        "#93A1A1",
        "#859900",
        "#268BD2",
        "#2AA198",
        "#D33682",
        "#B58900",
        "#CB4B16",
        "#586E75",
        "#EEE8D5"
    },
    {
        "cyberpunk",
        "#100528",
        "#FDF500",
        "#727072",
        "#FF007F",
        "#00F0FF",
        "#37FF8B",
        "#FF7F00",
        "#FFFFFF",
        "#BD00FF",
        "#000000",
        "#00F0FF"
    },
    {
        "matrix",
        "#0D0208",
        "#00FF41",
        "#008F11",
        "#00FF66",
        "#33FF33",
        "#66FF66",
        "#99FF99",
        "#FFFFFF",
        "#005500",
        "#00FF41",
        "#003B00"
    },
    {
        "default",
        "#000000",
        "#FFFFFF",
        "#008080",
        "#FFFF00",
        "#00FF00",
        "#FF00FF",
        "#FF0000",
        "#000000",
        "#0000FF",
        "#FFFFFF",
        "#0000FF"
    },
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL }
};

int hex_to_ansi_color(const char *hex) {
    if (!hex) return COLOR_WHITE;
    while (*hex && isspace((unsigned char)*hex)) hex++;
    if (!*hex) return COLOR_WHITE;

    if (strcasecmp(hex, "terminal") == 0 || strcasecmp(hex, "default") == 0 ||
        strcasecmp(hex, "transparent") == 0 || strcasecmp(hex, "none") == 0 ||
        strcmp(hex, "-1") == 0) {
        return -1;
    }

    if (isdigit((unsigned char)hex[0]) || (hex[0] == '-' && isdigit((unsigned char)hex[1]))) {
        int color = atoi(hex);
        if (color >= -1 && color <= 255) return color;
        return COLOR_WHITE;
    }

    if (hex[0] == '#' && strlen(hex) >= 7) {
        char r_str[3] = {hex[1], hex[2], '\0'};
        char g_str[3] = {hex[3], hex[4], '\0'};
        char b_str[3] = {hex[5], hex[6], '\0'};

        int r = (int)strtol(r_str, NULL, 16);
        int g = (int)strtol(g_str, NULL, 16);
        int b = (int)strtol(b_str, NULL, 16);

        if (abs(r - g) < 8 && abs(g - b) < 8 && abs(r - b) < 8) {
            int gray = (r + g + b) / 3;
            if (gray < 8) return 16;
            if (gray > 248) return 231;
            return 232 + (int)(((double)(gray - 8) / 240.0) * 23.0 + 0.5);
        }

        int r_idx = (r * 5 + 127) / 255;
        int g_idx = (g * 5 + 127) / 255;
        int b_idx = (b * 5 + 127) / 255;

        if (r_idx > 5) r_idx = 5;
        if (g_idx > 5) g_idx = 5;
        if (b_idx > 5) b_idx = 5;

        return 16 + 36 * r_idx + 6 * g_idx + b_idx;
    }

    if (strcasecmp(hex, "black") == 0) return COLOR_BLACK;
    if (strcasecmp(hex, "red") == 0) return COLOR_RED;
    if (strcasecmp(hex, "green") == 0) return COLOR_GREEN;
    if (strcasecmp(hex, "yellow") == 0) return COLOR_YELLOW;
    if (strcasecmp(hex, "blue") == 0) return COLOR_BLUE;
    if (strcasecmp(hex, "magenta") == 0) return COLOR_MAGENTA;
    if (strcasecmp(hex, "cyan") == 0) return COLOR_CYAN;
    if (strcasecmp(hex, "white") == 0) return COLOR_WHITE;
    if (strcasecmp(hex, "orange") == 0) return 208;
    if (strcasecmp(hex, "purple") == 0) return 141;
    if (strcasecmp(hex, "pink") == 0) return 212;
    if (strcasecmp(hex, "gray") == 0 || strcasecmp(hex, "grey") == 0) return 244;
    if (strcasecmp(hex, "darkgray") == 0 || strcasecmp(hex, "darkgrey") == 0) return 238;

    return COLOR_WHITE;
}

static bool parse_bool(const char *val) {
    if (!val) return false;
    while (*val && isspace((unsigned char)*val)) val++;
    if (strcasecmp(val, "true") == 0 || strcasecmp(val, "yes") == 0 ||
        strcasecmp(val, "1") == 0 || strcasecmp(val, "on") == 0) {
        return true;
    }
    return false;
}

static bool apply_theme(const char *theme_name) {
    if (!theme_name) return false;
    while (*theme_name && isspace((unsigned char)*theme_name)) theme_name++;

    for (int i = 0; BUILTIN_THEMES[i].name != NULL; i++) {
        if (strcasecmp(theme_name, BUILTIN_THEMES[i].name) == 0 ||
            (strcasecmp(theme_name, "catppuccin-mocha") == 0 && strcmp(BUILTIN_THEMES[i].name, "catppuccin") == 0)) {
            ThemeDef *t = &BUILTIN_THEMES[i];
            strncpy(SYNTAX_COLORS.current_theme, t->name, sizeof(SYNTAX_COLORS.current_theme) - 1);
            SYNTAX_COLORS.bg_color = hex_to_ansi_color(t->bg);
            SYNTAX_COLORS.hl_normal = hex_to_ansi_color(t->normal);
            SYNTAX_COLORS.hl_comment = hex_to_ansi_color(t->comment);
            SYNTAX_COLORS.hl_keyword1 = hex_to_ansi_color(t->keyword1);
            SYNTAX_COLORS.hl_keyword2 = hex_to_ansi_color(t->keyword2);
            SYNTAX_COLORS.hl_string = hex_to_ansi_color(t->string);
            SYNTAX_COLORS.hl_number = hex_to_ansi_color(t->number);
            SYNTAX_COLORS.hl_match = hex_to_ansi_color(t->match);
            SYNTAX_COLORS.hl_preproc = hex_to_ansi_color(t->preproc);
            SYNTAX_COLORS.hl_selection = hex_to_ansi_color(t->selection);
            SYNTAX_COLORS.hl_selection_bg = hex_to_ansi_color(t->selection_bg);
            return true;
        }
    }
    return false;
}

void init_syntax_colors() {
    SYNTAX_COLORS.use_terminal_bg = false;
    apply_theme("dracula");
}

static void parse_config_file(const char *config_path) {
    FILE *config_file = fopen(config_path, "r");
    if (!config_file) return;

    char line[512];
    while (fgets(line, sizeof(line), config_file)) {
        char *p = line;
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p == '#' || *p == '\0') continue;

        char *newline = strpbrk(p, "\r\n");
        if (newline) *newline = '\0';

        char *eq = strchr(p, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = p;
        char *val = eq + 1;
        while (*val && isspace((unsigned char)*val)) val++;

        if (strcasecmp(key, "theme") == 0) {
            apply_theme(val);
        } else if (strcasecmp(key, "use_terminal_bg") == 0 ||
                   strcasecmp(key, "transparent_bg") == 0 ||
                   strcasecmp(key, "ignore_bg") == 0) {
            SYNTAX_COLORS.use_terminal_bg = parse_bool(val);
        } else if (strcasecmp(key, "vi_nav") == 0 ||
                   strcasecmp(key, "vim_mode") == 0 ||
                   strcasecmp(key, "vim") == 0) {
            E.vim_enabled = parse_bool(val);
        } else if (strcasecmp(key, "bg_color") == 0 || strcasecmp(key, "bg") == 0) {
            SYNTAX_COLORS.bg_color = hex_to_ansi_color(val);
        } else if (strcasecmp(key, "hl_normal") == 0) {
            SYNTAX_COLORS.hl_normal = hex_to_ansi_color(val);
        } else if (strcasecmp(key, "hl_comment") == 0) {
            SYNTAX_COLORS.hl_comment = hex_to_ansi_color(val);
        } else if (strcasecmp(key, "hl_keyword1") == 0) {
            SYNTAX_COLORS.hl_keyword1 = hex_to_ansi_color(val);
        } else if (strcasecmp(key, "hl_keyword2") == 0) {
            SYNTAX_COLORS.hl_keyword2 = hex_to_ansi_color(val);
        } else if (strcasecmp(key, "hl_string") == 0) {
            SYNTAX_COLORS.hl_string = hex_to_ansi_color(val);
        } else if (strcasecmp(key, "hl_number") == 0) {
            SYNTAX_COLORS.hl_number = hex_to_ansi_color(val);
        } else if (strcasecmp(key, "hl_match") == 0) {
            SYNTAX_COLORS.hl_match = hex_to_ansi_color(val);
        } else if (strcasecmp(key, "hl_preproc") == 0) {
            SYNTAX_COLORS.hl_preproc = hex_to_ansi_color(val);
        } else if (strcasecmp(key, "hl_selection") == 0) {
            SYNTAX_COLORS.hl_selection = hex_to_ansi_color(val);
        } else if (strcasecmp(key, "hl_selection_bg") == 0) {
            SYNTAX_COLORS.hl_selection_bg = hex_to_ansi_color(val);
        }
    }
    fclose(config_file);
}

void load_config() {
    init_syntax_colors();

    char *home_dir = get_home_directory();
    if (home_dir) {
        char config_path[512];
        snprintf(config_path, sizeof(config_path), "%s/.nimkirc", home_dir);
        FILE *test_fp = fopen(config_path, "r");
        if (test_fp) {
            fclose(test_fp);
            parse_config_file(config_path);
        } else {
            create_default_config_file(config_path);
            parse_config_file(config_path);
        }
    }

    FILE *local_fp = fopen(".nimkirc", "r");
    if (local_fp) {
        fclose(local_fp);
        parse_config_file(".nimkirc");
    }
}

char *get_home_directory() {
#ifdef _WIN32
    char *home_dir = getenv("USERPROFILE");
    if (!home_dir) home_dir = getenv("HOME");
    return home_dir;
#else
    char *home_dir = getenv("HOME");
    if (!home_dir) {
        struct passwd *pw = getpwuid(getuid());
        if (pw) {
            home_dir = pw->pw_dir;
        }
    }
    return home_dir;
#endif
}

void create_default_config_file(const char *config_path) {
    FILE *config_file = fopen(config_path, "w");
    if (!config_file) return;

    fprintf(config_file,
        "theme=dracula\n"
        "use_terminal_bg=true\n"
        "vi_nav=false\n"
    );

    fclose(config_file);
}

void initialize_syntax_colors() {
    load_config();

    if (has_colors()) {
        start_color();
        use_default_colors();

        int bg = SYNTAX_COLORS.use_terminal_bg ? -1 : SYNTAX_COLORS.bg_color;
        int sel_bg = (SYNTAX_COLORS.hl_selection_bg >= 0) ? SYNTAX_COLORS.hl_selection_bg : COLOR_BLUE;

        init_pair(HL_NORMAL, SYNTAX_COLORS.hl_normal, bg);
        init_pair(HL_COMMENT, SYNTAX_COLORS.hl_comment, bg);
        init_pair(HL_KEYWORD1, SYNTAX_COLORS.hl_keyword1, bg);
        init_pair(HL_KEYWORD2, SYNTAX_COLORS.hl_keyword2, bg);
        init_pair(HL_STRING, SYNTAX_COLORS.hl_string, bg);
        init_pair(HL_NUMBER, SYNTAX_COLORS.hl_number, bg);
        init_pair(HL_MATCH, SYNTAX_COLORS.hl_match, COLOR_YELLOW);
        init_pair(HL_PREPROC, SYNTAX_COLORS.hl_preproc, bg);
        init_pair(HL_SELECTION, SYNTAX_COLORS.hl_selection, sel_bg);

        bkgd(COLOR_PAIR(HL_NORMAL));
    }
}
