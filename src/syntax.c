#include "common.h"

#ifdef _WIN32
#ifndef strcasecmp
#define strcasecmp _stricmp
#endif
#endif

extern EditorConfig E;
extern EditorSyntax *E_syntax;

static EditorSyntax **dynamic_syntaxes = NULL;
static int dynamic_syntax_count = 0;

static char *C_HL_extensions[] = { ".c", ".h", ".cpp", ".hpp", ".cc", ".cxx", ".hh", NULL };
static char *C_HL_keywords[] = {
    "switch", "if", "while", "for", "break", "continue", "return", "else",
    "goto", "auto", "register", "extern", "const", "unsigned", "signed",
    "volatile", "do", "typeof", "case", "default", "sizeof", "enum",
    "union", "struct", "typedef", "class", "namespace", "template", "typename",
    "this", "new", "delete", "public", "private", "protected", "virtual",
    "friend", "inline", "constexpr", "nullptr", "static_assert", "override",
    "final", "try", "catch", "throw", "using", "noexcept", NULL
};
static char *C_HL_types[] = {
    "int", "char", "float", "double", "void", "long", "short", "bool",
    "size_t", "ssize_t", "int8_t", "int16_t", "int32_t", "int64_t",
    "uint8_t", "uint16_t", "uint32_t", "uint64_t", "uintptr_t", "intptr_t",
    "auto", "string", "vector", "map", "set", "unique_ptr", "shared_ptr", NULL
};
static EditorSyntax C_syntax = {
    "C/C++", C_HL_extensions, C_HL_keywords, C_HL_types, "//", "/*", "*/"
};

static char *PY_HL_extensions[] = { ".py", ".pyw", ".pyx", ".pyi", NULL };
static char *PY_HL_keywords[] = {
    "def", "class", "return", "if", "elif", "else", "while", "for", "in",
    "try", "except", "finally", "raise", "import", "from", "as", "with",
    "pass", "break", "continue", "lambda", "yield", "global", "nonlocal",
    "assert", "async", "await", "and", "or", "not", "is", "match", "case", NULL
};
static char *PY_HL_types[] = {
    "int", "str", "float", "bool", "list", "dict", "set", "tuple", "bytes",
    "True", "False", "None", "self", "cls", "object", "type", "print", "len",
    "range", "enumerate", "zip", "map", "filter", "any", "all", "isinstance", NULL
};
static EditorSyntax PY_syntax = {
    "Python", PY_HL_extensions, PY_HL_keywords, PY_HL_types, "#", "\"\"\"", "\"\"\""
};

static char *JS_HL_extensions[] = { ".js", ".mjs", ".cjs", ".jsx", NULL };
static char *JS_HL_keywords[] = {
    "function", "var", "let", "const", "if", "else", "for", "while", "do",
    "return", "break", "continue", "switch", "case", "default", "try",
    "catch", "finally", "throw", "new", "this", "super", "class", "extends",
    "import", "export", "from", "as", "await", "async", "yield", "typeof",
    "instanceof", "delete", "in", "of", "void", "debugger", "with", "static", NULL
};
static char *JS_HL_types[] = {
    "true", "false", "null", "undefined", "NaN", "Infinity", "Promise",
    "Array", "Object", "String", "Number", "Boolean", "Symbol", "BigInt",
    "Map", "Set", "JSON", "Math", "console", "window", "document", NULL
};
static EditorSyntax JS_syntax = {
    "JavaScript", JS_HL_extensions, JS_HL_keywords, JS_HL_types, "//", "/*", "*/"
};

static char *TS_HL_extensions[] = { ".ts", ".mts", ".cts", ".tsx", NULL };
static char *TS_HL_keywords[] = {
    "function", "var", "let", "const", "if", "else", "for", "while", "do",
    "return", "break", "continue", "switch", "case", "default", "try",
    "catch", "finally", "throw", "new", "this", "super", "class", "extends",
    "import", "export", "from", "as", "await", "async", "yield", "typeof",
    "instanceof", "delete", "in", "of", "void", "interface", "type", "enum",
    "implements", "declare", "namespace", "abstract", "readonly", "override",
    "keyof", "infer", "never", "unknown", "any", "is", "as", NULL
};
static char *TS_HL_types[] = {
    "string", "number", "boolean", "any", "unknown", "never", "void",
    "null", "undefined", "true", "false", "Promise", "Array", "Record",
    "Partial", "Required", "Readonly", "Pick", "Omit", NULL
};
static EditorSyntax TS_syntax = {
    "TypeScript", TS_HL_extensions, TS_HL_keywords, TS_HL_types, "//", "/*", "*/"
};

static char *RS_HL_extensions[] = { ".rs", NULL };
static char *RS_HL_keywords[] = {
    "as", "break", "const", "continue", "crate", "else", "enum", "extern",
    "false", "fn", "for", "if", "impl", "in", "let", "loop", "match", "mod",
    "move", "mut", "pub", "ref", "return", "self", "Self", "static", "struct",
    "super", "trait", "true", "type", "unsafe", "use", "where", "while",
    "async", "await", "dyn", "macro_rules", NULL
};
static char *RS_HL_types[] = {
    "i8", "i16", "i32", "i64", "i128", "isize", "u8", "u16", "u32", "u64",
    "u128", "usize", "f32", "f64", "bool", "char", "str", "String", "Vec",
    "Option", "Result", "Some", "None", "Ok", "Err", "Box", "Rc", "Arc", NULL
};
static EditorSyntax RS_syntax = {
    "Rust", RS_HL_extensions, RS_HL_keywords, RS_HL_types, "//", "/*", "*/"
};

static char *GO_HL_extensions[] = { ".go", NULL };
static char *GO_HL_keywords[] = {
    "break", "default", "func", "interface", "select", "case", "defer", "go",
    "map", "struct", "chan", "else", "goto", "package", "switch", "const",
    "fallthrough", "if", "range", "type", "continue", "for", "import", "return", "var", NULL
};
static char *GO_HL_types[] = {
    "bool", "byte", "complex64", "complex128", "error", "float32", "float64",
    "int", "int8", "int16", "int32", "int64", "rune", "string", "uint", "uint8",
    "uint16", "uint32", "uint64", "uintptr", "true", "false", "nil", "iota",
    "make", "new", "len", "cap", "append", "copy", "close", "delete", "panic", "recover", NULL
};
static EditorSyntax GO_syntax = {
    "Go", GO_HL_extensions, GO_HL_keywords, GO_HL_types, "//", "/*", "*/"
};

static char *JAVA_HL_extensions[] = { ".java", NULL };
static char *JAVA_HL_keywords[] = {
    "abstract", "assert", "break", "case", "catch", "class", "const", "continue",
    "default", "do", "else", "enum", "extends", "final", "finally", "for", "goto",
    "if", "implements", "import", "instanceof", "interface", "native", "new",
    "package", "private", "protected", "public", "return", "static", "strictfp",
    "super", "switch", "synchronized", "this", "throw", "throws", "transient",
    "try", "volatile", "while", "record", "sealed", "permits", NULL
};
static char *JAVA_HL_types[] = {
    "boolean", "byte", "char", "double", "float", "int", "long", "short", "void",
    "String", "Object", "Integer", "Boolean", "List", "Map", "Set", "true", "false", "null", NULL
};
static EditorSyntax JAVA_syntax = {
    "Java", JAVA_HL_extensions, JAVA_HL_keywords, JAVA_HL_types, "//", "/*", "*/"
};

static char *CS_HL_extensions[] = { ".cs", NULL };
static char *CS_HL_keywords[] = {
    "abstract", "as", "base", "break", "case", "catch", "checked", "class",
    "const", "continue", "default", "delegate", "do", "else", "enum", "event",
    "explicit", "extern", "finally", "fixed", "for", "foreach", "goto", "if",
    "implicit", "in", "interface", "internal", "is", "lock", "namespace", "new",
    "operator", "out", "override", "params", "private", "protected", "public",
    "readonly", "ref", "return", "sealed", "sizeof", "stackalloc", "static",
    "struct", "switch", "this", "throw", "try", "typeof", "unchecked", "unsafe",
    "using", "virtual", "void", "volatile", "while", "async", "await", "var",
    "record", "init", "get", "set", "yield", NULL
};
static char *CS_HL_types[] = {
    "bool", "byte", "sbyte", "char", "decimal", "double", "float", "int", "uint",
    "nint", "nuint", "long", "ulong", "short", "ushort", "object", "string",
    "dynamic", "true", "false", "null", "Task", "List", "Dictionary", NULL
};
static EditorSyntax CS_syntax = {
    "C#", CS_HL_extensions, CS_HL_keywords, CS_HL_types, "//", "/*", "*/"
};

static char *PHP_HL_extensions[] = { ".php", ".phtml", ".php5", ".php7", ".php8", NULL };
static char *PHP_HL_keywords[] = {
    "__halt_compiler", "abstract", "and", "array", "as", "break", "callable",
    "case", "catch", "class", "clone", "const", "continue", "declare", "default",
    "die", "do", "echo", "else", "elseif", "empty", "enddeclare", "endfor",
    "endforeach", "endif", "endswitch", "endwhile", "eval", "exit", "extends",
    "final", "finally", "fn", "for", "foreach", "function", "global", "goto",
    "if", "implements", "include", "include_once", "instanceof", "insteadof",
    "interface", "isset", "list", "match", "namespace", "new", "or", "print",
    "private", "protected", "public", "readonly", "require", "require_once",
    "return", "static", "switch", "throw", "trait", "try", "unset", "use",
    "var", "while", "xor", "yield", NULL
};
static char *PHP_HL_types[] = {
    "int", "float", "string", "bool", "array", "object", "iterable", "void",
    "mixed", "never", "true", "false", "null", NULL
};
static EditorSyntax PHP_syntax = {
    "PHP", PHP_HL_extensions, PHP_HL_keywords, PHP_HL_types, "//", "/*", "*/"
};

static char *RB_HL_extensions[] = { ".rb", ".rake", ".gemspec", "Gemfile", "Rakefile", NULL };
static char *RB_HL_keywords[] = {
    "alias", "and", "begin", "break", "case", "class", "def", "defined?",
    "do", "else", "elsif", "end", "ensure", "for", "if", "in", "module",
    "next", "not", "or", "redo", "rescue", "retry", "return", "self",
    "super", "then", "undef", "unless", "until", "when", "while", "yield",
    "require", "require_relative", "include", "extend", "attr_accessor",
    "attr_reader", "attr_writer", NULL
};
static char *RB_HL_types[] = {
    "true", "false", "nil", "Integer", "Float", "String", "Array", "Hash",
    "Symbol", "Regexp", "Range", NULL
};
static EditorSyntax RB_syntax = {
    "Ruby", RB_HL_extensions, RB_HL_keywords, RB_HL_types, "#", "=begin", "=end"
};

static char *SWIFT_HL_extensions[] = { ".swift", NULL };
static char *SWIFT_HL_keywords[] = {
    "associatedtype", "class", "deinit", "enum", "extension", "fileprivate",
    "func", "import", "init", "inout", "internal", "let", "open", "operator",
    "private", "precedencegroup", "protocol", "public", "rethrows", "static",
    "struct", "subscript", "typealias", "var", "break", "case", "catch",
    "continue", "default", "defer", "do", "else", "fallthrough", "for", "guard",
    "if", "in", "repeat", "return", "throw", "switch", "where", "while", "as",
    "Any", "false", "is", "nil", "self", "Self", "super", "true", "try",
    "async", "await", "actor", NULL
};
static char *SWIFT_HL_types[] = {
    "Int", "Double", "Float", "Bool", "String", "Character", "Optional",
    "Array", "Dictionary", "Set", "Result", "Void", NULL
};
static EditorSyntax SWIFT_syntax = {
    "Swift", SWIFT_HL_extensions, SWIFT_HL_keywords, SWIFT_HL_types, "//", "/*", "*/"
};

static char *KT_HL_extensions[] = { ".kt", ".kts", NULL };
static char *KT_HL_keywords[] = {
    "as", "break", "class", "continue", "do", "else", "false", "for", "fun",
    "if", "in", "interface", "is", "null", "object", "package", "return",
    "super", "this", "throw", "true", "try", "typealias", "val", "var",
    "when", "while", "by", "catch", "constructor", "delegate", "dynamic",
    "field", "file", "finally", "get", "import", "init", "param", "property",
    "receiver", "set", "setparam", "where", "actual", "abstract", "companion",
    "const", "crossinline", "data", "enum", "expect", "external", "final",
    "infix", "inline", "inner", "internal", "lateinit", "noinline", "open",
    "operator", "out", "override", "private", "protected", "public", "reified",
    "sealed", "suspend", "tailrec", "vararg", NULL
};
static char *KT_HL_types[] = {
    "Byte", "Short", "Int", "Long", "Float", "Double", "Boolean", "Char",
    "String", "Array", "List", "Map", "Set", "Unit", "Any", "Nothing", NULL
};
static EditorSyntax KT_syntax = {
    "Kotlin", KT_HL_extensions, KT_HL_keywords, KT_HL_types, "//", "/*", "*/"
};

static char *SH_HL_extensions[] = { ".sh", ".bash", ".zsh", ".ksh", ".csh", NULL };
static char *SH_HL_keywords[] = {
    "if", "then", "else", "elif", "fi", "for", "in", "do", "done", "while",
    "until", "case", "esac", "function", "return", "export", "local", "read",
    "echo", "printf", "test", "exit", "break", "continue", "set", "unset",
    "trap", "eval", "exec", "source", "shift", "cd", "pwd", "select", NULL
};
static char *SH_HL_types[] = { NULL };
static EditorSyntax SH_syntax = {
    "Shell", SH_HL_extensions, SH_HL_keywords, SH_HL_types, "#", NULL, NULL
};

static char *HTML_HL_extensions[] = { ".html", ".htm", ".xhtml", ".vue", ".svelte", NULL };
static char *HTML_HL_keywords[] = {
    "html", "head", "body", "title", "meta", "link", "script", "style", "div",
    "p", "a", "img", "ul", "ol", "li", "table", "tr", "td", "th", "form",
    "input", "button", "span", "h1", "h2", "h3", "h4", "h5", "h6", "br", "hr",
    "em", "strong", "b", "i", "code", "pre", "header", "footer", "nav",
    "section", "article", "aside", "main", "canvas", "video", "audio", "svg",
    "path", "iframe", "textarea", "select", "option", "label", NULL
};
static char *HTML_HL_types[] = {
    "class", "id", "href", "src", "type", "name", "value", "rel", "alt",
    "style", "width", "height", "placeholder", "target", "charset", NULL
};
static EditorSyntax HTML_syntax = {
    "HTML", HTML_HL_extensions, HTML_HL_keywords, HTML_HL_types, NULL, "<!--", "-->"
};

static char *CSS_HL_extensions[] = { ".css", ".scss", ".sass", ".less", NULL };
static char *CSS_HL_keywords[] = {
    "color", "background", "background-color", "font-size", "margin", "padding",
    "border", "display", "position", "width", "height", "top", "right", "bottom",
    "left", "text-align", "line-height", "font-family", "font-weight", "float",
    "clear", "overflow", "z-index", "opacity", "transform", "transition",
    "animation", "flex", "grid", "gap", "justify-content", "align-items",
    "box-shadow", "border-radius", "cursor", "content", "important", NULL
};
static char *CSS_HL_types[] = {
    "none", "block", "inline", "inline-block", "flex", "grid", "absolute",
    "relative", "fixed", "sticky", "hidden", "visible", "auto", "inherit",
    "initial", "px", "em", "rem", "vh", "vw", "%", NULL
};
static EditorSyntax CSS_syntax = {
    "CSS", CSS_HL_extensions, CSS_HL_keywords, CSS_HL_types, NULL, "/*", "*/"
};

static char *XML_HL_extensions[] = { ".xml", ".svg", ".xaml", ".plist", ".rss", ".atom", NULL };
static char *XML_HL_keywords[] = {
    "xml", "version", "encoding", "root", "element", "node", "attribute",
    "item", "data", "id", "name", "value", "svg", "path", "g", "rect",
    "circle", "line", "text", NULL
};
static char *XML_HL_types[] = { NULL };
static EditorSyntax XML_syntax = {
    "XML", XML_HL_extensions, XML_HL_keywords, XML_HL_types, NULL, "<!--", "-->"
};

static char *JSON_HL_extensions[] = { ".json", ".jsonc", ".json5", NULL };
static char *JSON_HL_keywords[] = { "true", "false", "null", NULL };
static char *JSON_HL_types[] = { NULL };
static EditorSyntax JSON_syntax = {
    "JSON", JSON_HL_extensions, JSON_HL_keywords, JSON_HL_types, "//", "/*", "*/"
};

static char *YAML_HL_extensions[] = { ".yml", ".yaml", NULL };
static char *YAML_HL_keywords[] = {
    "true", "false", "yes", "no", "null", "on", "off", "~", NULL
};
static char *YAML_HL_types[] = { NULL };
static EditorSyntax YAML_syntax = {
    "YAML", YAML_HL_extensions, YAML_HL_keywords, YAML_HL_types, "#", NULL, NULL
};

static char *TOML_HL_extensions[] = { ".toml", NULL };
static char *TOML_HL_keywords[] = { "true", "false", "nan", "inf", NULL };
static char *TOML_HL_types[] = { NULL };
static EditorSyntax TOML_syntax = {
    "TOML", TOML_HL_extensions, TOML_HL_keywords, TOML_HL_types, "#", NULL, NULL
};

static char *INI_HL_extensions[] = { ".ini", ".cfg", ".conf", ".properties", NULL };
static char *INI_HL_keywords[] = { "true", "false", "yes", "no", "on", "off", NULL };
static char *INI_HL_types[] = { NULL };
static EditorSyntax INI_syntax = {
    "INI/Config", INI_HL_extensions, INI_HL_keywords, INI_HL_types, ";", NULL, NULL
};

static char *SQL_HL_extensions[] = { ".sql", NULL };
static char *SQL_HL_keywords[] = {
    "SELECT", "FROM", "WHERE", "INSERT", "INTO", "UPDATE", "DELETE", "CREATE",
    "DROP", "ALTER", "TABLE", "INDEX", "VIEW", "JOIN", "INNER", "LEFT", "RIGHT",
    "FULL", "OUTER", "ON", "GROUP", "BY", "HAVING", "ORDER", "ASC", "DESC",
    "LIMIT", "OFFSET", "UNION", "ALL", "DISTINCT", "AS", "AND", "OR", "NOT",
    "IN", "BETWEEN", "LIKE", "IS", "NULL", "PRIMARY", "KEY", "FOREIGN",
    "REFERENCES", "DEFAULT", "CHECK", "CONSTRAINT", "select", "from", "where",
    "insert", "into", "update", "delete", "create", "drop", "table", "join",
    "on", "order", "by", "group", "having", NULL
};
static char *SQL_HL_types[] = {
    "INT", "INTEGER", "BIGINT", "SMALLINT", "VARCHAR", "CHAR", "TEXT", "BOOLEAN",
    "DATE", "TIME", "TIMESTAMP", "DECIMAL", "NUMERIC", "FLOAT", "DOUBLE", "BLOB",
    "int", "varchar", "text", "boolean", NULL
};
static EditorSyntax SQL_syntax = {
    "SQL", SQL_HL_extensions, SQL_HL_keywords, SQL_HL_types, "--", "/*", "*/"
};

static char *MD_HL_extensions[] = { ".md", ".markdown", ".mdown", NULL };
static char *MD_HL_keywords[] = {
    "#", "##", "###", "####", "#####", "######", "*", "-", "+", ">",
    "[", "]", "(", ")", "```", NULL
};
static char *MD_HL_types[] = { NULL };
static EditorSyntax MD_syntax = {
    "Markdown", MD_HL_extensions, MD_HL_keywords, MD_HL_types, NULL, "<!--", "-->"
};

static char *LUA_HL_extensions[] = { ".lua", NULL };
static char *LUA_HL_keywords[] = {
    "and", "break", "do", "else", "elseif", "end", "false", "for", "function",
    "goto", "if", "in", "local", "nil", "not", "or", "repeat", "return",
    "then", "true", "until", "while", NULL
};
static char *LUA_HL_types[] = {
    "nil", "boolean", "number", "string", "table", "function", "thread",
    "userdata", "print", "type", "tostring", "tonumber", "pairs", "ipairs",
    "require", "assert", "error", "pcall", NULL
};
static EditorSyntax LUA_syntax = {
    "Lua", LUA_HL_extensions, LUA_HL_keywords, LUA_HL_types, "--", "--[[", "]]"
};

static char *ZIG_HL_extensions[] = { ".zig", NULL };
static char *ZIG_HL_keywords[] = {
    "addrspace", "align", "and", "asm", "async", "await", "break", "catch",
    "comptime", "const", "continue", "defer", "else", "enum", "errdefer",
    "error", "export", "extern", "fn", "for", "if", "inline", "noalias",
    "nosuspend", "opaque", "or", "orelse", "packed", "pub", "resume",
    "return", "struct", "suspend", "switch", "test", "threadlocal", "try",
    "union", "unreachable", "usingnamespace", "var", "volatile", "while", NULL
};
static char *ZIG_HL_types[] = {
    "i8", "i16", "i32", "i64", "i128", "isize", "u8", "u16", "u32", "u64",
    "u128", "usize", "f16", "f32", "f64", "f80", "f128", "bool", "void",
    "noreturn", "type", "anyerror", "true", "false", "null", "undefined", NULL
};
static EditorSyntax ZIG_syntax = {
    "Zig", ZIG_HL_extensions, ZIG_HL_keywords, ZIG_HL_types, "//", NULL, NULL
};

static char *DART_HL_extensions[] = { ".dart", NULL };
static char *DART_HL_keywords[] = {
    "abstract", "as", "assert", "async", "await", "break", "case", "catch",
    "class", "const", "continue", "default", "do", "else", "enum", "export",
    "extends", "extension", "external", "factory", "false", "final", "finally",
    "for", "get", "if", "implements", "import", "in", "is", "late", "mixin",
    "new", "null", "on", "operator", "part", "required", "rethrow", "return",
    "set", "static", "super", "switch", "sync", "this", "throw", "true", "try",
    "typedef", "var", "void", "while", "with", "yield", NULL
};
static char *DART_HL_types[] = {
    "int", "double", "num", "String", "bool", "List", "Map", "Set",
    "dynamic", "Object", "Future", "Stream", NULL
};
static EditorSyntax DART_syntax = {
    "Dart", DART_HL_extensions, DART_HL_keywords, DART_HL_types, "//", "/*", "*/"
};

static char *PL_HL_extensions[] = { ".pl", ".pm", ".t", NULL };
static char *PL_HL_keywords[] = {
    "if", "else", "elsif", "unless", "while", "until", "for", "foreach",
    "return", "my", "our", "local", "sub", "use", "require", "package",
    "last", "next", "redo", "die", "warn", "eval", "do", "print", "say", NULL
};
static char *PL_HL_types[] = { NULL };
static EditorSyntax PL_syntax = {
    "Perl", PL_HL_extensions, PL_HL_keywords, PL_HL_types, "#", "=pod", "=cut"
};

static char *HS_HL_extensions[] = { ".hs", ".lhs", NULL };
static char *HS_HL_keywords[] = {
    "case", "class", "data", "default", "deriving", "do", "else", "if",
    "import", "in", "infix", "infixl", "infixr", "instance", "let", "module",
    "newtype", "of", "then", "type", "where", "_", NULL
};
static char *HS_HL_types[] = {
    "Int", "Integer", "Float", "Double", "Char", "String", "Bool", "True",
    "False", "Maybe", "Just", "Nothing", "Either", "Left", "Right", "IO", NULL
};
static EditorSyntax HS_syntax = {
    "Haskell", HS_HL_extensions, HS_HL_keywords, HS_HL_types, "--", "{-", "-}"
};

static char *SCALA_HL_extensions[] = { ".scala", ".sc", NULL };
static char *SCALA_HL_keywords[] = {
    "abstract", "case", "catch", "class", "def", "do", "else", "extends",
    "false", "final", "finally", "for", "forSome", "if", "implicit", "import",
    "lazy", "match", "new", "null", "object", "override", "package", "private",
    "protected", "return", "sealed", "super", "this", "throw", "trait", "try",
    "true", "type", "val", "var", "while", "with", "yield", NULL
};
static char *SCALA_HL_types[] = {
    "Byte", "Short", "Int", "Long", "Float", "Double", "Boolean", "Char",
    "String", "Unit", "Any", "AnyRef", "Nothing", "Option", "Some", "None", NULL
};
static EditorSyntax SCALA_syntax = {
    "Scala", SCALA_HL_extensions, SCALA_HL_keywords, SCALA_HL_types, "//", "/*", "*/"
};

static char *R_HL_extensions[] = { ".r", ".R", NULL };
static char *R_HL_keywords[] = {
    "if", "else", "repeat", "while", "function", "for", "in", "next", "break",
    "TRUE", "FALSE", "NULL", "Inf", "NaN", "NA", "NA_integer_", "NA_real_",
    "NA_complex_", "NA_character_", "library", "require", NULL
};
static char *R_HL_types[] = { NULL };
static EditorSyntax R_syntax = {
    "R", R_HL_extensions, R_HL_keywords, R_HL_types, "#", NULL, NULL
};

static char *JL_HL_extensions[] = { ".jl", NULL };
static char *JL_HL_keywords[] = {
    "baremodule", "begin", "break", "catch", "const", "continue", "do", "else",
    "elseif", "end", "export", "false", "finally", "for", "function", "global",
    "if", "import", "let", "local", "macro", "module", "quote", "return",
    "struct", "true", "try", "using", "while", NULL
};
static char *JL_HL_types[] = {
    "Int", "Int8", "Int16", "Int32", "Int64", "UInt", "UInt8", "UInt16",
    "UInt32", "UInt64", "Float16", "Float32", "Float64", "Bool", "Char",
    "String", "Vector", "Matrix", "Array", "Dict", "Set", "Tuple", NULL
};
static EditorSyntax JL_syntax = {
    "Julia", JL_HL_extensions, JL_HL_keywords, JL_HL_types, "#", "#=", "=#"
};

static char *EX_HL_extensions[] = { ".ex", ".exs", NULL };
static char *EX_HL_keywords[] = {
    "def", "defp", "defmodule", "defprotocol", "defimpl", "defmacro", "defmacrop",
    "do", "end", "if", "unless", "case", "cond", "with", "for", "receive",
    "after", "try", "rescue", "catch", "after", "raise", "import", "require",
    "use", "alias", "fn", "true", "false", "nil", NULL
};
static char *EX_HL_types[] = { NULL };
static EditorSyntax EX_syntax = {
    "Elixir", EX_HL_extensions, EX_HL_keywords, EX_HL_types, "#", NULL, NULL
};

static char *ERL_HL_extensions[] = { ".erl", ".hrl", NULL };
static char *ERL_HL_keywords[] = {
    "after", "and", "andalso", "band", "bnot", "bor", "bsl", "bsr", "bxor",
    "case", "catch", "cond", "div", "end", "fun", "if", "let", "not", "of",
    "or", "orelse", "receive", "rem", "try", "when", "xor", NULL
};
static char *ERL_HL_types[] = { NULL };
static EditorSyntax ERL_syntax = {
    "Erlang", ERL_HL_extensions, ERL_HL_keywords, ERL_HL_types, "%", NULL, NULL
};

static char *CLJ_HL_extensions[] = { ".clj", ".cljs", ".cljc", ".edn", NULL };
static char *CLJ_HL_keywords[] = {
    "def", "defn", "defn-", "defmacro", "defmulti", "defmethod", "defprotocol",
    "defrecord", "deftype", "fn", "let", "loop", "recur", "if", "if-not",
    "when", "when-not", "cond", "case", "do", "ns", "require", "use", "import", NULL
};
static char *CLJ_HL_types[] = {
    "true", "false", "nil", "nil?", "int?", "string?", "boolean?", NULL
};
static EditorSyntax CLJ_syntax = {
    "Clojure", CLJ_HL_extensions, CLJ_HL_keywords, CLJ_HL_types, ";", NULL, NULL
};

static char *NIM_HL_extensions[] = { ".nim", ".nims", ".nimble", NULL };
static char *NIM_HL_keywords[] = {
    "addr", "and", "as", "asm", "bind", "block", "break", "case", "cast",
    "concept", "const", "continue", "converter", "defer", "discard", "distinct",
    "div", "do", "elif", "else", "end", "enum", "except", "export", "finally",
    "for", "from", "func", "if", "import", "in", "include", "interface", "is",
    "isnot", "iterator", "let", "macro", "method", "mixin", "mod", "nil", "not",
    "notin", "object", "of", "or", "out", "proc", "ptr", "raise", "ref",
    "return", "shl", "shr", "static", "template", "try", "type", "using",
    "var", "when", "while", "xor", "yield", NULL
};
static char *NIM_HL_types[] = {
    "int", "int8", "int16", "int32", "int64", "uint", "uint8", "uint16",
    "uint32", "uint64", "float", "float32", "float64", "bool", "char",
    "string", "cstring", "pointer", "auto", "seq", "array", "openArray",
    "void", "true", "false", NULL
};
static EditorSyntax NIM_syntax = {
    "Nim", NIM_HL_extensions, NIM_HL_keywords, NIM_HL_types, "#", "#[", "]#"
};

static char *MAKE_HL_extensions[] = { "Makefile", "makefile", ".mk", NULL };
static char *MAKE_HL_keywords[] = {
    "ifeq", "ifneq", "else", "endif", "ifdef", "ifndef", "include", "-include",
    "override", "export", "unexport", "vpath", "all", "clean", "install", NULL
};
static char *MAKE_HL_types[] = {
    "CC", "CXX", "CFLAGS", "CXXFLAGS", "LDFLAGS", "SHELL", ".PHONY", ".PRECIOUS", NULL
};
static EditorSyntax MAKE_syntax = {
    "Makefile", MAKE_HL_extensions, MAKE_HL_keywords, MAKE_HL_types, "#", NULL, NULL
};

static char *DOCKER_HL_extensions[] = { "Dockerfile", "dockerfile", ".dockerfile", NULL };
static char *DOCKER_HL_keywords[] = {
    "FROM", "RUN", "CMD", "LABEL", "EXPOSE", "ENV", "ADD", "COPY",
    "ENTRYPOINT", "VOLUME", "USER", "WORKDIR", "ARG", "ONBUILD", "STOPSIGNAL",
    "HEALTHCHECK", "SHELL", NULL
};
static char *DOCKER_HL_types[] = { "AS", "as", NULL };
static EditorSyntax DOCKER_syntax = {
    "Dockerfile", DOCKER_HL_extensions, DOCKER_HL_keywords, DOCKER_HL_types, "#", NULL, NULL
};

static char *BAT_HL_extensions[] = { ".bat", ".cmd", NULL };
static char *BAT_HL_keywords[] = {
    "echo", "off", "on", "set", "if", "else", "goto", "call", "exit", "pause",
    "rem", "for", "in", "do", "shift", "start", "cls", "choice", NULL
};
static char *BAT_HL_types[] = { NULL };
static EditorSyntax BAT_syntax = {
    "Batch", BAT_HL_extensions, BAT_HL_keywords, BAT_HL_types, "rem", NULL, NULL
};

static char *PS1_HL_extensions[] = { ".ps1", ".psm1", ".psd1", NULL };
static char *PS1_HL_keywords[] = {
    "function", "filter", "workflow", "configuration", "if", "else", "elseif",
    "switch", "while", "do", "until", "for", "foreach", "in", "break", "continue",
    "return", "param", "throw", "trap", "try", "catch", "finally", "class",
    "enum", "using", "process", "begin", "end", NULL
};
static char *PS1_HL_types[] = {
    "string", "int", "bool", "array", "hashtable", "true", "false", "null", NULL
};
static EditorSyntax PS1_syntax = {
    "PowerShell", PS1_HL_extensions, PS1_HL_keywords, PS1_HL_types, "#", "<#", "#>"
};

static char *GQL_HL_extensions[] = { ".graphql", ".gql", NULL };
static char *GQL_HL_keywords[] = {
    "query", "mutation", "subscription", "fragment", "type", "input", "interface",
    "union", "enum", "scalar", "directive", "on", "schema", "extend", NULL
};
static char *GQL_HL_types[] = {
    "Int", "Float", "String", "Boolean", "ID", NULL
};
static EditorSyntax GQL_syntax = {
    "GraphQL", GQL_HL_extensions, GQL_HL_keywords, GQL_HL_types, "#", NULL, NULL
};

static char *PROTO_HL_extensions[] = { ".proto", NULL };
static char *PROTO_HL_keywords[] = {
    "syntax", "package", "import", "message", "enum", "service", "rpc", "returns",
    "option", "required", "optional", "repeated", "oneof", "map", "reserved", NULL
};
static char *PROTO_HL_types[] = {
    "double", "float", "int32", "int64", "uint32", "uint64", "sint32", "sint64",
    "fixed32", "fixed64", "sfixed32", "sfixed64", "bool", "string", "bytes", NULL
};
static EditorSyntax PROTO_syntax = {
    "Protobuf", PROTO_HL_extensions, PROTO_HL_keywords, PROTO_HL_types, "//", "/*", "*/"
};

static EditorSyntax *EditorSyntaxes[] = {
    &C_syntax,
    &PY_syntax,
    &JS_syntax,
    &TS_syntax,
    &RS_syntax,
    &GO_syntax,
    &JAVA_syntax,
    &CS_syntax,
    &PHP_syntax,
    &RB_syntax,
    &SWIFT_syntax,
    &KT_syntax,
    &SH_syntax,
    &HTML_syntax,
    &CSS_syntax,
    &XML_syntax,
    &JSON_syntax,
    &YAML_syntax,
    &TOML_syntax,
    &INI_syntax,
    &SQL_syntax,
    &MD_syntax,
    &LUA_syntax,
    &ZIG_syntax,
    &DART_syntax,
    &PL_syntax,
    &HS_syntax,
    &SCALA_syntax,
    &R_syntax,
    &JL_syntax,
    &EX_syntax,
    &ERL_syntax,
    &CLJ_syntax,
    &NIM_syntax,
    &MAKE_syntax,
    &DOCKER_syntax,
    &BAT_syntax,
    &PS1_syntax,
    &GQL_syntax,
    &PROTO_syntax,
    NULL
};

static char **split_whitespace_tokens(const char *str, int *count) {
    int cap = 16;
    int n = 0;
    char **arr = malloc(cap * sizeof(char *));
    if (!arr) return NULL;

    const char *p = str;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        const char *start = p;
        while (*p && !isspace((unsigned char)*p)) p++;
        size_t len = p - start;
        if (n + 1 >= cap) {
            cap *= 2;
            char **new_arr = realloc(arr, cap * sizeof(char *));
            if (!new_arr) break;
            arr = new_arr;
        }
        arr[n] = malloc(len + 1);
        if (arr[n]) {
            memcpy(arr[n], start, len);
            arr[n][len] = '\0';
            n++;
        }
    }
    arr[n] = NULL;
    if (count) *count = n;
    return arr;
}

static void append_tokens(char ***arr_ptr, int *count, const char *str) {
    if (!*arr_ptr) {
        *arr_ptr = split_whitespace_tokens(str, count);
        return;
    }
    int extra_count = 0;
    char **extra = split_whitespace_tokens(str, &extra_count);
    if (!extra) return;

    int new_total = *count + extra_count;
    char **new_arr = realloc(*arr_ptr, (new_total + 1) * sizeof(char *));
    if (!new_arr) {
        for (int i = 0; i < extra_count; i++) free(extra[i]);
        free(extra);
        return;
    }
    *arr_ptr = new_arr;
    for (int i = 0; i < extra_count; i++) {
        (*arr_ptr)[*count + i] = extra[i];
    }
    *count = new_total;
    (*arr_ptr)[new_total] = NULL;
    free(extra);
}

static void load_syntax_file(const char *filepath) {
    FILE *fp = fopen(filepath, "r");
    if (!fp) return;

    EditorSyntax *syntax = calloc(1, sizeof(EditorSyntax));
    if (!syntax) {
        fclose(fp);
        return;
    }

    int ext_count = 0;
    int kw1_count = 0;
    int kw2_count = 0;

    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        char *p = line;
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p || *p == '#') continue;

        char *newline = strpbrk(p, "\r\n");
        if (newline) *newline = '\0';

        char *colon = strchr(p, ':');
        if (!colon) continue;
        *colon = '\0';
        char *key = p;
        char *val = colon + 1;
        while (*val && isspace((unsigned char)*val)) val++;

        if (strcasecmp(key, "name") == 0) {
            syntax->name = strdup(val);
        } else if (strcasecmp(key, "extensions") == 0 || strcasecmp(key, "extension") == 0) {
            append_tokens(&syntax->filetype_extensions, &ext_count, val);
        } else if (strcasecmp(key, "comment_single") == 0 || strcasecmp(key, "comment") == 0) {
            if (*val) syntax->singleline_comment_start = strdup(val);
        } else if (strcasecmp(key, "comment_multi_start") == 0) {
            if (*val) syntax->multiline_comment_start = strdup(val);
        } else if (strcasecmp(key, "comment_multi_end") == 0) {
            if (*val) syntax->multiline_comment_end = strdup(val);
        } else if (strcasecmp(key, "keywords") == 0 || strcasecmp(key, "keywords1") == 0) {
            append_tokens(&syntax->keywords1, &kw1_count, val);
        } else if (strcasecmp(key, "types") == 0 || strcasecmp(key, "keywords2") == 0) {
            append_tokens(&syntax->keywords2, &kw2_count, val);
        }
    }
    fclose(fp);

    if (syntax->filetype_extensions && ext_count > 0) {
        if (!syntax->name) {
            syntax->name = strdup(syntax->filetype_extensions[0]);
        }
        EditorSyntax **new_list = realloc(dynamic_syntaxes, (dynamic_syntax_count + 1) * sizeof(EditorSyntax *));
        if (new_list) {
            dynamic_syntaxes = new_list;
            dynamic_syntaxes[dynamic_syntax_count++] = syntax;
        }
    } else {
        if (syntax->name) free(syntax->name);
        if (syntax->singleline_comment_start) free(syntax->singleline_comment_start);
        if (syntax->multiline_comment_start) free(syntax->multiline_comment_start);
        if (syntax->multiline_comment_end) free(syntax->multiline_comment_end);
        free(syntax);
    }
}

static void load_syntax_files_from_dir(const char *dir_path) {
    DIR *dir = opendir(dir_path);
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        char *dot = strrchr(entry->d_name, '.');
        if (dot && strcasecmp(dot, ".syntax") == 0) {
            char filepath[PATH_MAX];
#ifdef _WIN32
            snprintf(filepath, sizeof(filepath), "%s\\%s", dir_path, entry->d_name);
#else
            snprintf(filepath, sizeof(filepath), "%s/%s", dir_path, entry->d_name);
#endif
            load_syntax_file(filepath);
        }
    }
    closedir(dir);
}

void init_syntax_system() {

    load_syntax_files_from_dir("syntax");

#ifdef _WIN32
    char exepath[MAX_PATH];
    if (GetModuleFileNameA(NULL, exepath, sizeof(exepath))) {
        char *slash = strrchr(exepath, '\\');
        if (slash) {
            *slash = '\0';
            char syntax_dir[MAX_PATH];
            snprintf(syntax_dir, sizeof(syntax_dir), "%s\\syntax", exepath);
            load_syntax_files_from_dir(syntax_dir);
        }
    }
    char *appdata = getenv("APPDATA");
    if (appdata) {
        char user_syntax_dir[MAX_PATH];
        snprintf(user_syntax_dir, sizeof(user_syntax_dir), "%s\\nimki\\syntax", appdata);
        load_syntax_files_from_dir(user_syntax_dir);
    }
#else
    char *home = getenv("HOME");
    if (home) {
        char user_syntax_dir[PATH_MAX];
        snprintf(user_syntax_dir, sizeof(user_syntax_dir), "%s/.config/nimki/syntax", home);
        load_syntax_files_from_dir(user_syntax_dir);
        snprintf(user_syntax_dir, sizeof(user_syntax_dir), "%s/.nimki/syntax", home);
        load_syntax_files_from_dir(user_syntax_dir);
    }
#endif
}

void cleanup_syntax_system() {
    if (dynamic_syntaxes) {
        for (int i = 0; i < dynamic_syntax_count; i++) {
            EditorSyntax *s = dynamic_syntaxes[i];
            if (!s) continue;
            if (s->name) free(s->name);
            if (s->singleline_comment_start) free(s->singleline_comment_start);
            if (s->multiline_comment_start) free(s->multiline_comment_start);
            if (s->multiline_comment_end) free(s->multiline_comment_end);
            if (s->filetype_extensions) {
                for (int j = 0; s->filetype_extensions[j]; j++) free(s->filetype_extensions[j]);
                free(s->filetype_extensions);
            }
            if (s->keywords1) {
                for (int j = 0; s->keywords1[j]; j++) free(s->keywords1[j]);
                free(s->keywords1);
            }
            if (s->keywords2) {
                for (int j = 0; s->keywords2[j]; j++) free(s->keywords2[j]);
                free(s->keywords2);
            }
            free(s);
        }
        free(dynamic_syntaxes);
        dynamic_syntaxes = NULL;
        dynamic_syntax_count = 0;
    }
}

static bool matches_syntax(const EditorSyntax *syntax, const char *basename, const char *ext) {
    if (!syntax || !syntax->filetype_extensions) return false;

    for (int j = 0; syntax->filetype_extensions[j]; j++) {
        const char *pattern = syntax->filetype_extensions[j];
        if (pattern[0] == '.') {
            if (ext && strcasecmp(ext, pattern) == 0) return true;
        } else {
            if (strcasecmp(basename, pattern) == 0) return true;
        }
    }
    return false;
}

void editor_select_syntax_highlight() {
    E_syntax = NULL;
    if (!E.filename) return;

    const char *basename = E.filename;
    const char *slash = strrchr(E.filename, '/');
#ifdef _WIN32
    const char *bslash = strrchr(E.filename, '\\');
    if (!slash || (bslash && bslash > slash)) slash = bslash;
#endif
    if (slash) basename = slash + 1;

    const char *ext = strrchr(basename, '.');

    for (int i = 0; i < dynamic_syntax_count; i++) {
        if (matches_syntax(dynamic_syntaxes[i], basename, ext)) {
            E_syntax = dynamic_syntaxes[i];
            return;
        }
    }

    for (int i = 0; EditorSyntaxes[i]; i++) {
        if (matches_syntax(EditorSyntaxes[i], basename, ext)) {
            E_syntax = EditorSyntaxes[i];
            return;
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
    if (line->hl == NULL) return;
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
        unsigned char prev_hl = (i > 0) ? line->hl[i - 1] : HL_NORMAL;

        if (mc_start && mc_end) {
            size_t mc_end_len = strlen(mc_end);
            size_t mc_start_len = strlen(mc_start);
            if (in_multiline_comment) {
                line->hl[i] = HL_COMMENT;
                if (i + mc_end_len <= line->len && strncmp(&line->text[i], mc_end, mc_end_len) == 0) {
                    for (size_t j = 0; j < mc_end_len; j++) line->hl[i + j] = HL_COMMENT;
                    i += mc_end_len;
                    in_multiline_comment = 0;
                    prev_sep = 1;
                    continue;
                }
                i++;
                continue;
            } else if (i + mc_start_len <= line->len && strncmp(&line->text[i], mc_start, mc_start_len) == 0) {
                for (size_t j = 0; j < mc_start_len; j++) line->hl[i + j] = HL_COMMENT;
                i += mc_start_len;
                in_multiline_comment = 1;
                continue;
            }
        }

        if (sc_start) {
            size_t sc_len = strlen(sc_start);
            if (i + sc_len <= line->len && strncmp(&line->text[i], sc_start, sc_len) == 0) {
                for (size_t j = i; j < line->len; j++) {
                    line->hl[j] = HL_COMMENT;
                }
                break;
            }
        }

        if (in_string) {
            line->hl[i] = HL_STRING;
            if (c == '\\' && i + 1 < line->len) {
                line->hl[i + 1] = HL_STRING;
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
            if (c == '"' || c == '\'' || c == '`') {
                in_string = c;
                line->hl[i] = HL_STRING;
                i++;
                prev_sep = 0;
                continue;
            }
        }

        if (isdigit((unsigned char)c) && (prev_sep || prev_hl == HL_NUMBER)) {
            line->hl[i] = HL_NUMBER;
            i++;
            prev_sep = 0;
            continue;
        }

        if (i == 0 && c == '#' && strcmp(E_syntax->name, "C/C++") == 0) {
            for (size_t j = 0; j < line->len; j++) {
                line->hl[j] = HL_PREPROC;
            }
            break;
        }

        if (prev_sep) {
            if (keywords1) {
                for (size_t k = 0; keywords1[k]; k++) {
                    size_t kwlen = strlen(keywords1[k]);
                    if (i + kwlen <= line->len &&
                        strncmp(&line->text[i], keywords1[k], kwlen) == 0 &&
                        is_separator(line->text[i + kwlen])) {
                        for (size_t j = 0; j < kwlen; j++) line->hl[i + j] = HL_KEYWORD1;
                        i += kwlen;
                        prev_sep = 0;
                        goto next_char_in_loop;
                    }
                }
            }
            if (keywords2) {
                for (size_t k = 0; keywords2[k]; k++) {
                    size_t kwlen = strlen(keywords2[k]);
                    if (i + kwlen <= line->len &&
                        strncmp(&line->text[i], keywords2[k], kwlen) == 0 &&
                        is_separator(line->text[i + kwlen])) {
                        for (size_t j = 0; j < kwlen; j++) line->hl[i + j] = HL_KEYWORD2;
                        i += kwlen;
                        prev_sep = 0;
                        goto next_char_in_loop;
                    }
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
