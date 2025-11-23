#include "common.h"

extern EditorConfig E;
extern EditorSyntax *E_syntax;
extern FileTreeState FT;

EditorSyntax *EditorSyntaxes[];
// this ones for c and c++
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
// this ones for shell scripts (bash)
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
// this is for java iskript
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
// now we have html
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
// this is for cascading stylesheets
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
// thsi is for html
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

void editor_select_syntax_highlight();
void editor_update_syntax(int filerow);

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
