/**
 * NIMKI CLI TEXT EDITOR - THE WEBSITE IS THE TEXT EDITOR
 * Pure C/ncurses terminal emulator logic, virtual directory tree,
 * line numbers, reverse status bar, message bar, and mouse/keyboard handlers.
 */

(function () {
  'use strict';

  // =========================================================================
  // 1. VIRTUAL FILESYSTEM (Hierarchical tree exactly as in filetree.c)
  // =========================================================================
  const VFS_TREE = {
    name: 'nimki',
    isDir: true,
    expanded: true,
    children: [
      {
        name: 'docs',
        isDir: true,
        expanded: true,
        children: [
          { name: 'README.txt', isDir: false },
          { name: 'how_to_use.txt', isDir: false },
          { name: 'shortcuts.txt', isDir: false },
          { name: 'install.txt', isDir: false },
          { name: 'architecture.txt', isDir: false }
        ]
      },
      {
        name: 'src',
        isDir: true,
        expanded: false,
        children: [
          { name: 'main.c', isDir: false },
          { name: 'editor.c', isDir: false },
          { name: 'ui.c', isDir: false },
          { name: 'syntax.c', isDir: false },
          { name: 'filetree.c', isDir: false },
          { name: 'input.c', isDir: false },
          { name: 'clipboard.c', isDir: false },
          { name: 'config.c', isDir: false },
          { name: 'common.h', isDir: false }
        ]
      },
      { name: 'hello.c', isDir: false },
      { name: '.nimkirc', isDir: false },
      { name: 'Makefile', isDir: false },
      { name: 'LICENSE', isDir: false }
    ]
  };

  const VFS_FILES = {
    'README.txt': {
      lang: 'text',
      content: [
        '==============================================================================',
        ' NIMKI - Simple & Lightweight CLI Text Editor in C',
        ' Repository: https://github.com/Anik200/nimki',
        ' Version:    0.1.4',
        ' Author:     Anik200 (GPL-3.0 License)',
        '==============================================================================',
        '',
        'Nimki is a fast, lightweight terminal text editor written in pure C99 with',
        'zero dependencies other than curses/ncurses.',
        '',
        'KEY FEATURES:',
        '  * Zero startup lag (~180KB compiled binary)',
        '  * Built-in interactive file tree explorer (Ctrl+N)',
        '  * Native terminal mouse support (click cursor, wheel scroll, drag select)',
        '  * Fast lexical syntax highlighting for C, Shell, JS, HTML, CSS, XML',
        '  * Multi-level snapshot undo buffer (Ctrl+Z, up to 20 states)',
        '  * Real-time search with match highlights (Ctrl+F)',
        '  * Dynamic margin line numbers (Ctrl+T)',
        '  * Ambient digital clock (top-right header)',
        '  * Atomic file writes & unsaved change safety shield (Ctrl+Q / Ctrl+C)',
        '  * Fully customizable themes via ~/.nimkirc (hex RGB, named, ANSI)',
        '',
        '------------------------------------------------------------------------------',
        'QUICK INSTALLATION:',
        '------------------------------------------------------------------------------',
        '  Windows (Winget):',
        '    winget install Anik200.nimki',
        '',
        '  Linux / macOS (Build from source):',
        '    git clone https://github.com/Anik200/nimki.git',
        '    cd nimki',
        '    make',
        '    sudo make install',
        '',
        '------------------------------------------------------------------------------',
        'NAVIGATION (USE MOUSE OR ARROW KEYS ON THE FILE TREE ON THE LEFT):',
        '------------------------------------------------------------------------------',
        '  Select any file from the file tree on the left to read and test:',
        '    [-] docs/',
        '        how_to_use.txt    <- Complete user guide & keyboard shortcuts',
        '        shortcuts.txt     <- Quick keybindings reference table',
        '        install.txt       <- Distro-specific install instructions (apt, dnf, brew)',
        '        architecture.txt  <- C99 codebase structure & design breakdown',
        '    [-] src/',
        '        main.c            <- CLI args & main editor event loop',
        '        editor.c          <- Buffer management & 20-snapshot undo stack',
        '        ui.c              <- ncurses screen rendering & status bar',
        '        syntax.c          <- Lexical syntax highlighter engine',
        '        filetree.c        <- Directory crawler & flat tree indexer',
        '        common.h          <- Editor data structures & definitions',
        '    .nimkirc              <- Configuration file (colors & settings)',
        '    hello.c               <- Live interactive C playground! Feel free to edit!',
        '',
        'Tip: You can edit, type, and test keyboard shortcuts directly in this buffer!'
      ]
    },

    'how_to_use.txt': {
      lang: 'text',
      content: [
        '==============================================================================',
        ' HOW TO USE NIMKI - USER GUIDE',
        '==============================================================================',
        '',
        '1. STARTING NIMKI',
        '   $ nimki                 # Open blank scratchpad',
        '   $ nimki file.c          # Open existing or create new file',
        '   $ nimki -v              # Print version (nimki 0.1.4)',
        '   $ nimki -h              # Print command line options',
        '',
        '2. WRITING & EDITING CODE',
        '   Nimki is modeless. There is no insert mode or normal mode.',
        '   * Type printable characters: inserts directly at cursor position.',
        '   * Tab: inserts 4-space tab stop.',
        '   * Backspace / Delete: deletes character or merges lines.',
        '   * Enter: inserts newline and splits current row.',
        '   * Ctrl + Z: Undo last change (preserves up to 20 snapshots).',
        '',
        '3. SAVING & SAFE EXIT',
        '   * Save: Press Ctrl + S.',
        '     If the buffer is untitled, Nimki prompts for filename at the bottom.',
        '   * Quit: Press Ctrl + Q (or Ctrl + C).',
        '   * Unsaved Changes Shield:',
        '     If modifications exist, Nimki displays:',
        '     "WARNING! File has unsaved changes. Press Ctrl+Q/C again to force quit."',
        '',
        '4. SIDEBAR FILE TREE (Ctrl + N)',
        '   * Toggle: Press Ctrl + N to open or hide the file tree.',
        '   * Navigate: Up / Down arrow keys.',
        '   * Expand / Collapse: Press Tab or Left / Right arrow keys.',
        '   * Open: Press Enter or Left-Click on any file.',
        '',
        '5. MOUSE SUPPORT',
        '   * Click: Snaps cursor directly to clicked line and column.',
        '   * Scroll Wheel: Smoothly scrolls editor rows up or down.',
        '   * Click & Drag: Highlights text selection.',
        '   * Right Click: Displays floating ncurses context menu:',
        '       [ Copy ]',
        '       [ Select All ]',
        '',
        '6. REAL-TIME SEARCH (Ctrl + F / Ctrl + W)',
        '   * Press Ctrl + F to open search prompt.',
        '   * Matches highlight immediately across the buffer.',
        '   * Navigate matches with Up / Down arrows.',
        '   * Press Enter to jump or Esc to exit search.',
        '',
        '7. LINE NUMBERS & STATUS',
        '   * Toggle line numbers: Press Ctrl + T.',
        '   * Status Bar: Inverted bar shows filename, lines, modified, and coordinates.',
        '   * Clock: Top right corner displays system time (HH:MM).'
      ]
    },

    'shortcuts.txt': {
      lang: 'text',
      content: [
        '==============================================================================',
        ' NIMKI KEYBOARD & MOUSE SHORTCUTS CHEATSHEET',
        '==============================================================================',
        '',
        'KEY COMBINATION        ACTION',
        '------------------     -------------------------------------------------------',
        'Ctrl + S               Save active buffer to disk',
        'Ctrl + Q / Ctrl + C    Quit editor (warns if unsaved changes exist)',
        'Ctrl + Z               Undo last modification (20 snapshot states)',
        'Ctrl + F / Ctrl + W    Search / Find text with live match highlighting',
        'Ctrl + T               Toggle line numbers margin ON / OFF',
        'Ctrl + N               Toggle sidebar file tree explorer ON / OFF',
        'Ctrl + K               Start selection / Copy selected text to clipboard',
        'Ctrl + A               Select all text in current buffer',
        'Ctrl + Shift + V       Paste text from OS clipboard into terminal',
        'Tab                    Indent 4 spaces (or toggle folder in file tree)',
        'Enter                  Newline (or open file in file tree)',
        'Up / Down              Move cursor up / down (or navigate file tree)',
        'Left / Right           Move cursor left / right (or expand/collapse folder)',
        'Home / End             Jump to beginning / end of current line',
        'PageUp / PageDown      Scroll up / down one screenful',
        'Esc                    Cancel active prompt or close context menu',
        '',
        'MOUSE GESTURE          ACTION',
        '------------------     -------------------------------------------------------',
        'Left Click             Snap cursor instantly to clicked row & column',
        'Mouse Scroll Wheel     Scroll editor viewport up / down',
        'Click & Drag           Highlight selection range',
        'Right Click            Open ncurses Context Menu (Copy / Select All)',
        'Click File in Tree     Open file into editor buffer',
        'Click Dir in Tree      Expand / Collapse folder'
      ]
    },

    'install.txt': {
      lang: 'text',
      content: [
        '==============================================================================',
        ' INSTALLING NIMKI (MULTI-PLATFORM GUIDE)',
        '==============================================================================',
        '',
        '[1] WINDOWS (MICROSOFT WINGET)',
        '    Open PowerShell or Command Prompt and run:',
        '    $ winget install Anik200.nimki',
        '',
        '[2] UBUNTU / DEBIAN / LINUX MINT',
        '    $ sudo apt update',
        '    $ sudo apt install -y gcc make libncurses-dev git',
        '    $ git clone https://github.com/Anik200/nimki.git',
        '    $ cd nimki',
        '    $ make',
        '    $ sudo make install',
        '',
        '[3] FEDORA / RHEL / CENTOS',
        '    $ sudo dnf install -y gcc make ncurses-devel git',
        '    $ git clone https://github.com/Anik200/nimki.git',
        '    $ cd nimki',
        '    $ make',
        '    $ sudo make install',
        '',
        '[4] ARCH LINUX / MANJARO',
        '    $ sudo pacman -S --needed base-devel ncurses git',
        '    $ git clone https://github.com/Anik200/nimki.git',
        '    $ cd nimki',
        '    $ make',
        '    $ sudo make install',
        '',
        '[5] MACOS (HOMEBREW)',
        '    $ brew install ncurses',
        '    $ git clone https://github.com/Anik200/nimki.git',
        '    $ cd nimki',
        '    $ make',
        '    $ sudo make install',
        '',
        '[6] VERIFY INSTALLATION',
        '    $ nimki --version',
        '    Output: nimki 0.1.4'
      ]
    },

    'architecture.txt': {
      lang: 'text',
      content: [
        '==============================================================================',
        ' NIMKI ARCHITECTURE & C99 DESIGN',
        '==============================================================================',
        '',
        'Nimki is architected around speed, clarity, and zero external baggage.',
        '',
        '1. ZERO-LATENCY EVENT LOOP',
        '   Nimki bypasses heavy GUI rendering frameworks entirely. Written in C99',
        '   using ncurses (and PDCurses for Windows wincon), it refreshes only',
        '   dirty lines and updates the terminal in microseconds.',
        '',
        '2. PURE MODULAR DECOUPLING (SRC DIRECTORY):',
        '   • main.c:       CLI parsing & main infinite keypress loop',
        '   • editor.c:     Memory buffers & 20-level snapshot undo stack',
        '   • fileops.c:    Disk file reading and atomic buffer saving',
        '   • ui.c:         Terminal drawing, margin line numbers & status bar',
        '   • filetree.c:   Recursive directory tree crawler & flat indexer',
        '   • syntax.c:     Lexical syntax highlighting state machine',
        '   • input.c:      Input dispatcher & mouse coordinate translation',
        '   • clipboard.c:  Cross-platform clipboard bridging (Win32, xclip, pbcopy)',
        '   • config.c:     Hex RGB, ANSI, and named color parser for ~/.nimkirc',
        '   • common.h:     Global editor configuration and struct definitions',
        '',
        '3. MEMORY SAFETY & ATOMIC WRITES',
        '   When saving files, Nimki ensures data integrity so that sudden power',
        '   loss or interruptions do not leave corrupted or truncated files.',
        '',
        '4. ZERO BLOAT',
        '   Binary size is ~180KB, memory usage under 5MB.'
      ]
    },

    'hello.c': {
      lang: 'c',
      content: [
        '#include <stdio.h>',
        '',
        '// Welcome to Nimki!',
        '// This is a live interactive buffer. You can edit this file right now.',
        '// Try typing, deleting, or pressing Ctrl+Z to undo.',
        '',
        'int main(int argc, char *argv[]) {',
        '    char *editor = "Nimki";',
        '    int version_major = 0;',
        '    int version_minor = 1;',
        '    int patch = 4;',
        '',
        '    printf("Running %s %d.%d.%d\\n", editor, version_major, version_minor, patch);',
        '    printf("Zero startup latency, pure C99.\\n");',
        '',
        '    return 0;',
        '}'
      ]
    },

    '.nimkirc': {
      lang: 'config',
      content: [
        '# Nimki Configuration File',
        '# Location: ~/.nimkirc or %USERPROFILE%\\.nimkirc',
        '# You can use hex colors, named colors, or ANSI numbers',
        '',
        'hl_normal=#FFFFFF',
        'hl_comment=#008080',
        'hl_keyword1=#FFFF00',
        'hl_keyword2=#00FF00',
        'hl_string=#FF00FF',
        'hl_number=#FF0000',
        'hl_match=#000000',
        'hl_preproc=#0000FF',
        'hl_selection=#FFFFFF',
        '',
        '# Restart Nimki after editing for changes to take effect'
      ]
    },

    'Makefile': {
      lang: 'sh',
      content: [
        'ifeq ($(OS),Windows_NT)',
        '    CC = gcc',
        '    TARGET = nimki.exe',
        '    PDCURSES_DIR = vendor/pdcurses',
        '    CFLAGS = -Wall -Wextra -s -Isrc -I$(PDCURSES_DIR)',
        '    LDFLAGS = $(PDCURSES_DIR)/wincon/pdcurses.a',
        'else',
        '    CC ?= cc',
        '    CFLAGS = -Wall -Wextra -s',
        '    LDFLAGS = -lncurses -lm',
        '    INSTALL_DIR = /usr/local/bin',
        '    TARGET = nimki',
        'endif',
        '',
        'SRCS = src/main.c src/editor.c src/ui.c src/fileops.c src/syntax.c src/input.c src/clipboard.c src/filetree.c src/config.c',
        '',
        'all: $(TARGET)',
        '',
        '$(TARGET): $(SRCS)',
        '\t$(CC) $(SRCS) -o $(TARGET) $(CFLAGS) $(LDFLAGS)',
        '',
        'install: all',
        '\t@mkdir -p $(INSTALL_DIR)',
        '\t@install -m 755 $(TARGET) $(INSTALL_DIR)/$(TARGET)',
        '',
        'clean:',
        '\t@rm -f $(TARGET)'
      ]
    },

    'LICENSE': {
      lang: 'text',
      content: [
        '                    GNU GENERAL PUBLIC LICENSE',
        '                       Version 3, 29 June 2007',
        '',
        ' Copyright (C) 2007 Free Software Foundation, Inc. <https://fsf.org/>',
        ' Everyone is permitted to copy and distribute verbatim copies',
        ' of this license document, but changing it is not allowed.',
        '',
        ' Nimki is Free Software under GPL-3.0.',
        ' Developed by Anik200.'
      ]
    },

    'main.c': {
      lang: 'c',
      content: [
        '#include "common.h"',
        '',
        'extern EditorConfig E;',
        '',
        'int main(int argc, char *argv[]) {',
        '    if (argc >= 2) {',
        '        if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0) {',
        '            printf("nimki %s\\n", EDITOR_VERSION);',
        '            return 0;',
        '        }',
        '        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {',
        '            printf("Usage: nimki [filename]\\n");',
        '            return 0;',
        '        }',
        '    }',
        '',
        '    init_editor();',
        '',
        '    if (argc >= 2) {',
        '        editor_read_file(argv[1]);',
        '    } else {',
        '        E.lines = malloc(sizeof(EditorLine));',
        '        E.lines[0].text = strdup("");',
        '        E.lines[0].len = 0;',
        '        E.lines[0].hl = NULL;',
        '        E.lines[0].hl_open_comment = 0;',
        '        E.num_lines = 1;',
        '        editor_update_syntax(0);',
        '        editor_set_status_message("Welcome to Nimki! Press Ctrl+Q to quit. Ctrl+S to save.");',
        '    }',
        '',
        '    editor_refresh_screen();',
        '',
        '    while (1) {',
        '        editor_process_keypress();',
        '    }',
        '',
        '    return 0;',
        '}'
      ]
    },

    'editor.c': {
      lang: 'c',
      content: [
        '#include "common.h"',
        '',
        'extern EditorConfig E;',
        '',
        'void editor_save_state() {',
        '    if (E.undo_history_len < MAX_UNDO_STATES) {',
        '        E.undo_history_idx = E.undo_history_len++;',
        '    } else {',
        '        editor_free_snapshot(&E.undo_history[0]);',
        '        for (int i = 1; i < MAX_UNDO_STATES; i++) {',
        '            E.undo_history[i-1] = E.undo_history[i];',
        '        }',
        '        E.undo_history_idx = MAX_UNDO_STATES - 1;',
        '    }',
        '    // Save snapshot: lines, cursor, dirty state',
        '}',
        '',
        'void editor_undo() {',
        '    if (E.undo_history_idx <= 0) {',
        '        editor_set_status_message("Already at oldest change");',
        '        return;',
        '    }',
        '    E.undo_history_idx--;',
        '    // Restore snapshot lines and cursor...',
        '}'
      ]
    },

    'ui.c': {
      lang: 'c',
      content: [
        '#include "common.h"',
        '',
        'extern EditorConfig E;',
        '',
        'void editor_draw_status_bar() {',
        '    attron(A_REVERSE);',
        '',
        '    int x_offset = E.file_tree_visible ? FILE_TREE_WIDTH : 0;',
        '    int max_width = E.screen_cols - x_offset;',
        '',
        '    mvprintw(E.screen_rows, x_offset, "%.*s - %d lines %s",',
        '             max_width - 15,',
        '             E.filename ? E.filename : "[No Name]", E.num_lines,',
        '             E.dirty ? "(modified)" : "");',
        '',
        '    char rstatus[80];',
        '    snprintf(rstatus, sizeof(rstatus), "%d/%d", E.cy + 1, E.num_lines);',
        '    mvprintw(E.screen_rows, x_offset + max_width - strlen(rstatus), "%s", rstatus);',
        '',
        '    attroff(A_REVERSE);',
        '}',
        '',
        'void editor_draw_clock() {',
        '    time_t rawtime; struct tm *info; char time_str[6];',
        '    time(&rawtime); info = localtime(&rawtime);',
        '    strftime(time_str, sizeof(time_str), "%H:%M", info);',
        '    mvprintw(0, E.screen_cols - strlen(time_str), "%s", time_str);',
        '}'
      ]
    },

    'syntax.c': {
      lang: 'c',
      content: [
        '#include "common.h"',
        '',
        'char *C_HL_extensions[] = { ".c", ".h", ".cpp", ".hpp", ".cc", NULL };',
        'char *C_HL_keywords[] = {',
        '    "switch", "if", "while", "for", "break", "continue", "return", "else",',
        '    "goto", "auto", "register", "extern", "const", "unsigned", "signed",',
        '    "volatile", "do", "typeof", "case", "default", "sizeof", "enum",',
        '    "union", "struct", "typedef", NULL',
        '};',
        'char *C_HL_types[] = {',
        '    "int", "char", "float", "double", "void", "long", "short", NULL',
        '};',
        '',
        'EditorSyntax C_syntax = {',
        '    C_HL_extensions,',
        '    C_HL_keywords,',
        '    C_HL_types,',
        '    "//", "/*", "*/"',
        '};'
      ]
    },

    'filetree.c': {
      lang: 'c',
      content: [
        '#include "common.h"',
        '',
        'void draw_file_tree() {',
        '    if (!E.file_tree_visible || !FT.flat_nodes) return;',
        '    int max_rows = E.screen_rows;',
        '    int start = E.file_tree_offset;',
        '    int end = start + max_rows;',
        '    if (end > FT.flat_node_count) end = FT.flat_node_count;',
        '',
        '    for (int i = start; i < end; i++) {',
        '        FileTreeNode *node = FT.flat_nodes[i];',
        '        int y = i - start;',
        '        int indent = get_node_depth(node) * 2;',
        '        if (indent > 20) indent = 20;',
        '',
        '        if (node->is_dir) {',
        '            if (node->expanded) mvprintw(y, indent, "[-] %s", node->name);',
        '            else mvprintw(y, indent, "[+] %s", node->name);',
        '        } else {',
        '            mvprintw(y, indent, " %s", node->name);',
        '        }',
        '        if (i == E.file_tree_cursor) {',
        '            attron(A_REVERSE);',
        '            mvchgat(y, 0, -1, A_REVERSE, 0, NULL);',
        '            attroff(A_REVERSE);',
        '        }',
        '    }',
        '    for (int y = 0; y < E.screen_rows; y++) {',
        '        mvaddch(y, FILE_TREE_WIDTH - 1, ACS_VLINE);',
        '    }',
        '}'
      ]
    },

    'input.c': {
      lang: 'c',
      content: [
        '#include "common.h"',
        '',
        'void editor_process_keypress() {',
        '    int c = getch();',
        '    switch (c) {',
        '        case CTRL(\'q\'): case CTRL(\'c\'):',
        '            if (E.dirty) {',
        '                editor_set_status_message("WARNING! File has unsaved changes. Press Ctrl+Q/C again to force quit.");',
        '                editor_refresh_screen();',
        '                int c2 = getch();',
        '                if (c2 != CTRL(\'q\') && c2 != CTRL(\'c\')) return;',
        '            }',
        '            cleanup_editor(); exit(0); break;',
        '        case CTRL(\'s\'): editor_save_file(); break;',
        '        case CTRL(\'z\'): editor_undo(); break;',
        '        case CTRL(\'f\'): editor_find(); break;',
        '        case CTRL(\'t\'):',
        '            E.show_line_numbers = !E.show_line_numbers;',
        '            editor_set_status_message("Line numbers %s", E.show_line_numbers ? "ON" : "OFF");',
        '            break;',
        '        case CTRL(\'n\'): toggle_file_tree(); break;',
        '    }',
        '}'
      ]
    },

    'clipboard.c': {
      lang: 'c',
      content: [
        '#include "common.h"',
        '',
        '// Cross-platform clipboard bridging',
        '#ifdef _WIN32',
        'void copy_to_clipboard(const char *text) {',
        '    OpenClipboard(NULL);',
        '    EmptyClipboard();',
        '    HGLOBAL hGlob = GlobalAlloc(GMEM_FIXED, strlen(text) + 1);',
        '    strcpy((char*)hGlob, text);',
        '    SetClipboardData(CF_TEXT, hGlob);',
        '    CloseClipboard();',
        '}',
        '#endif'
      ]
    },

    'config.c': {
      lang: 'c',
      content: [
        '#include "common.h"',
        '',
        '// Parses ~/.nimkirc color declarations into ncurses color pairs',
        'int hex_to_ansi_color(const char* hex) {',
        '    if (hex[0] == \'#\' && strlen(hex) >= 7) {',
        '        // Extracts 24-bit RGB and maps to 256-color ANSI cube',
        '    }',
        '    return COLOR_WHITE;',
        '}'
      ]
    },

    'common.h': {
      lang: 'c',
      content: [
        '#ifndef COMMON_H',
        '#define COMMON_H',
        '',
        '#define EDITOR_VERSION "0.1.4"',
        '#define TAB_STOP 4',
        '#define CTRL(k) ((k) & 0x1f)',
        '#define MAX_UNDO_STATES 20',
        '#define FILE_TREE_WIDTH 30',
        '',
        'typedef struct {',
        '    char *text;',
        '    size_t len;',
        '    char *hl;',
        '    int hl_open_comment;',
        '} EditorLine;',
        '',
        'typedef struct {',
        '    EditorLine *lines;',
        '    int num_lines;',
        '    int cx, cy;',
        '    int dirty;',
        '    bool file_tree_visible;',
        '    bool show_line_numbers;',
        '} EditorConfig;',
        '',
        '#endif'
      ]
    }
  };

  // =========================================================================
  // 2. SYNTAX HIGHLIGHTING (Port of Nimki syntax.c)
  // =========================================================================
  const C_KEYWORDS1 = [
    'switch', 'if', 'while', 'for', 'break', 'continue', 'return', 'else',
    'goto', 'auto', 'register', 'extern', 'const', 'unsigned', 'signed',
    'volatile', 'do', 'typeof', 'case', 'default', 'sizeof', 'enum',
    'union', 'struct', 'typedef'
  ];
  const C_KEYWORDS2 = [
    'int', 'char', 'float', 'double', 'void', 'long', 'short', 'bool', 'size_t'
  ];
  const SH_KEYWORDS = [
    'if', 'then', 'else', 'fi', 'for', 'in', 'do', 'done', 'while', 'until',
    'case', 'esac', 'function', 'return', 'export', 'local', 'read', 'echo',
    'printf', 'test', 'exit', 'break', 'continue', 'set', 'unset'
  ];

  function escapeHTML(s) {
    return s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
  }

  function highlightLine(text, lang, searchQuery) {
    if (!text) return '';
    let res = '';

    // Config (.nimkirc)
    if (lang === 'config') {
      if (text.startsWith('#')) {
        res = `<span class="hl-com">${escapeHTML(text)}</span>`;
      } else if (text.includes('=')) {
        let p = text.split('=');
        res = `<span class="hl-kw2">${escapeHTML(p[0])}</span>=<span class="hl-str">${escapeHTML(p.slice(1).join('='))}</span>`;
      } else {
        res = escapeHTML(text);
      }
      return applySearch(res, searchQuery);
    }

    // Preprocessor (#include, #define, #ifdef)
    if ((lang === 'c') && text.trim().startsWith('#')) {
      res = `<span class="hl-pre">${escapeHTML(text)}</span>`;
      return applySearch(res, searchQuery);
    }

    // Comments
    let commentPrefix = (lang === 'sh') ? '#' : '//';
    let commentIdx = text.indexOf(commentPrefix);
    if (commentIdx !== -1) {
      let codePart = text.slice(0, commentIdx);
      let commentPart = text.slice(commentIdx);
      res = highlightTokens(codePart, lang) + `<span class="hl-com">${escapeHTML(commentPart)}</span>`;
      return applySearch(res, searchQuery);
    }

    res = highlightTokens(text, lang);
    return applySearch(res, searchQuery);
  }

  function highlightTokens(text, lang) {
    let regex = /("(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*')|(\b\d+\b)|([a-zA-Z_]\w*)|([^\w\s"']+)/g;
    return text.replace(regex, (match, str, num, word) => {
      if (str) return `<span class="hl-str">${escapeHTML(str)}</span>`;
      if (num) return `<span class="hl-num">${escapeHTML(num)}</span>`;
      if (word) {
        if (lang === 'c') {
          if (C_KEYWORDS1.includes(word)) return `<span class="hl-kw1">${escapeHTML(word)}</span>`;
          if (C_KEYWORDS2.includes(word)) return `<span class="hl-kw2">${escapeHTML(word)}</span>`;
        } else if (lang === 'sh') {
          if (SH_KEYWORDS.includes(word)) return `<span class="hl-kw1">${escapeHTML(word)}</span>`;
        }
        return escapeHTML(word);
      }
      return escapeHTML(match);
    });
  }

  function applySearch(html, query) {
    if (!query) return html;
    let safe = query.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
    let re = new RegExp(`(${safe})`, 'gi');
    return html.replace(re, '<span class="hl-mat">$1</span>');
  }

  // =========================================================================
  // 3. PURE CLI TERMINAL ENGINE
  // =========================================================================
  const NimkiTerminal = {
    currentFile: 'README.txt',
    lines: [],
    cx: 0,
    cy: 0,
    dirty: false,
    showLineNumbers: true,
    fileTreeVisible: true,
    treeCursor: 2, // starts on README.txt
    flatTreeNodes: [],
    undoHistory: [],
    searchQuery: '',
    searchActive: false,
    contextMenuActive: false,
    activeFocus: 'editor', // 'editor' or 'tree'

    init() {
      this.rebuildFlatTree();
      this.loadFile(this.currentFile, false);
      this.bindEvents();
      this.startClock();
      this.render();
    },

    rebuildFlatTree() {
      this.flatTreeNodes = [];
      const traverse = (node, depth) => {
        this.flatTreeNodes.push({
          node: node,
          depth: depth
        });
        if (node.isDir && node.expanded && node.children) {
          node.children.forEach(child => traverse(child, depth + 1));
        }
      };
      traverse(VFS_TREE, 0);
    },

    loadFile(filename, markDirty = false) {
      if (!VFS_FILES[filename]) return;
      this.currentFile = filename;
      this.lines = [...VFS_FILES[filename].content];
      this.cx = 0;
      this.cy = 0;
      this.dirty = markDirty;
      this.undoHistory = [];
      this.searchActive = false;
      this.searchQuery = '';
      this.closeSearchPrompt();
      this.closeContextMenu();

      // Find in tree and set cursor
      let idx = this.flatTreeNodes.findIndex(item => item.node.name === filename);
      if (idx !== -1) {
        this.treeCursor = idx;
      }

      this.setStatusMessage(`Opened ${filename} (${this.lines.length} lines)`);
      this.render();

      let scrollEl = document.getElementById('terminalScrollArea');
      if (scrollEl) scrollEl.scrollTop = 0;
    },

    saveSnapshot() {
      if (this.undoHistory.length >= 20) this.undoHistory.shift();
      this.undoHistory.push({
        lines: [...this.lines],
        cx: this.cx,
        cy: this.cy,
        dirty: this.dirty
      });
    },

    undo() {
      if (this.undoHistory.length === 0) {
        this.setStatusMessage('Already at oldest change');
        return;
      }
      let snap = this.undoHistory.pop();
      this.lines = snap.lines;
      this.cx = snap.cx;
      this.cy = snap.cy;
      this.dirty = snap.dirty;
      this.setStatusMessage('Undo change');
      this.render();
    },

    saveFile() {
      this.dirty = false;
      this.setStatusMessage(`[OK] ${this.lines.length} lines written to ${this.currentFile}`);
      this.render();
    },

    toggleFileTree() {
      this.fileTreeVisible = !this.fileTreeVisible;
      let treeEl = document.getElementById('terminalFiletree');
      if (this.fileTreeVisible) {
        treeEl?.classList.remove('hidden');
        this.activeFocus = 'tree';
        this.setStatusMessage('File tree OPEN (Use arrows + Enter to open, or Ctrl+N / Alt+N to close)');
      } else {
        treeEl?.classList.add('hidden');
        this.activeFocus = 'editor';
        this.setStatusMessage('File tree CLOSED (Press Ctrl+N or Alt+N to open)');
      }
      this.render();
    },

    toggleLineNumbers() {
      this.showLineNumbers = !this.showLineNumbers;
      let gutterEl = document.getElementById('terminalLineNumbers');
      if (this.showLineNumbers) {
        gutterEl?.classList.remove('hidden');
        this.setStatusMessage('Line numbers ON (Ctrl+T)');
      } else {
        gutterEl?.classList.add('hidden');
        this.setStatusMessage('Line numbers OFF (Ctrl+T)');
      }
      this.render();
    },

    setStatusMessage(msg) {
      let msgEl = document.getElementById('terminalMsgbar');
      if (msgEl) msgEl.textContent = msg;
    },

    startClock() {
      const update = () => {
        let clockEl = document.getElementById('terminalClock');
        if (!clockEl) return;
        let d = new Date();
        let h = String(d.getHours()).padStart(2, '0');
        let m = String(d.getMinutes()).padStart(2, '0');
        clockEl.textContent = `${h}:${m}`;
      };
      update();
      setInterval(update, 30000);
    },

    insertChar(ch) {
      this.saveSnapshot();
      let line = this.lines[this.cy] || '';
      this.lines[this.cy] = line.slice(0, this.cx) + ch + line.slice(this.cx);
      this.cx++;
      this.dirty = true;
      this.render();
    },

    insertNewline() {
      this.saveSnapshot();
      let line = this.lines[this.cy] || '';
      let rest = line.slice(this.cx);
      this.lines[this.cy] = line.slice(0, this.cx);
      this.lines.splice(this.cy + 1, 0, rest);
      this.cy++;
      this.cx = 0;
      this.dirty = true;
      this.render();
    },

    deleteChar() {
      if (this.cx === 0 && this.cy === 0) return;
      this.saveSnapshot();
      let line = this.lines[this.cy] || '';
      if (this.cx > 0) {
        this.lines[this.cy] = line.slice(0, this.cx - 1) + line.slice(this.cx);
        this.cx--;
      } else if (this.cy > 0) {
        let prevLen = this.lines[this.cy - 1].length;
        this.lines[this.cy - 1] += line;
        this.lines.splice(this.cy, 1);
        this.cy--;
        this.cx = prevLen;
      }
      this.dirty = true;
      this.render();
    },

    openSearchPrompt() {
      this.searchActive = true;
      let promptEl = document.getElementById('searchPrompt');
      let msgEl = document.getElementById('terminalMsgbar');
      let inputEl = document.getElementById('searchInput');

      if (msgEl) msgEl.style.display = 'none';
      if (promptEl) promptEl.classList.add('active');
      if (inputEl) {
        inputEl.value = '';
        inputEl.focus();
      }
    },

    closeSearchPrompt() {
      this.searchActive = false;
      this.searchQuery = '';
      let promptEl = document.getElementById('searchPrompt');
      let msgEl = document.getElementById('terminalMsgbar');

      if (promptEl) promptEl.classList.remove('active');
      if (msgEl) msgEl.style.display = 'block';
      this.render();
    },

    openContextMenu(x, y) {
      let menu = document.getElementById('terminalContextMenu');
      if (!menu) return;
      menu.style.left = `${Math.min(x, window.innerWidth - 150)}px`;
      menu.style.top = `${Math.min(y, window.innerHeight - 80)}px`;
      menu.classList.add('active');
      this.contextMenuActive = true;
    },

    closeContextMenu() {
      let menu = document.getElementById('terminalContextMenu');
      if (menu) menu.classList.remove('active');
      this.contextMenuActive = false;
    },

    treeToggleExpand() {
      let cur = this.flatTreeNodes[this.treeCursor];
      if (cur && cur.node.isDir) {
        cur.node.expanded = !cur.node.expanded;
        this.rebuildFlatTree();
        this.renderTree();
      }
    },

    treeOpenFile() {
      let cur = this.flatTreeNodes[this.treeCursor];
      if (cur) {
        if (cur.node.isDir) {
          this.treeToggleExpand();
        } else {
          this.loadFile(cur.node.name);
          this.activeFocus = 'editor';
        }
      }
    },

    renderTree() {
      let treeEl = document.getElementById('terminalFiletree');
      if (!treeEl || !this.fileTreeVisible) return;

      let html = '';
      this.flatTreeNodes.forEach((item, idx) => {
        let isSelected = (idx === this.treeCursor);
        let indent = '  '.repeat(item.depth);
        let prefix = item.node.isDir
          ? (item.node.expanded ? '[-] ' : '[+] ')
          : ' ';
        let text = `${indent}${prefix}${item.node.name}`;

        // Pad line to 29 characters wide (before column 30 ACS_VLINE separator)
        if (text.length < 29) {
          text = text.padEnd(29, ' ');
        } else {
          text = text.slice(0, 29);
        }

        html += `<div class="tree-row ${isSelected ? 'active' : ''}" data-idx="${idx}">${escapeHTML(text)}</div>`;
      });

      treeEl.innerHTML = html;
    },

    render() {
      this.renderTree();

      let gutterEl = document.getElementById('terminalLineNumbers');
      let codeEl = document.getElementById('terminalCodeBody');
      let statusLeftEl = document.getElementById('statusLeft');
      let statusCoordEl = document.getElementById('statusCoord');

      if (!gutterEl || !codeEl) return;

      // Render line numbers margin
      if (this.showLineNumbers) {
        let maxDigits = String(this.lines.length).length;
        if (maxDigits < 2) maxDigits = 2;
        let numHtml = '';
        for (let i = 1; i <= this.lines.length; i++) {
          let numStr = String(i).padStart(maxDigits, ' ');
          numHtml += `<div>${numStr} </div>`;
        }
        gutterEl.innerHTML = numHtml;
      }

      // Render highlighted code rows
      let lang = VFS_FILES[this.currentFile]?.lang || 'text';
      let codeHtml = '';

      this.lines.forEach((lineText, idx) => {
        let isCur = (idx === this.cy && this.activeFocus === 'editor');
        if (isCur) {
          let before = lineText.slice(0, this.cx);
          let at = lineText.slice(this.cx, this.cx + 1) || ' ';
          let after = lineText.slice(this.cx + 1);

          let hBefore = highlightLine(before, lang, this.searchQuery);
          let hAfter = highlightLine(after, lang, this.searchQuery);

          codeHtml += `<div class="term-line">${hBefore}<span class="term-cursor">${escapeHTML(at)}</span>${hAfter}</div>`;
        } else {
          codeHtml += `<div class="term-line">${highlightLine(lineText, lang, this.searchQuery) || ' '}</div>`;
        }
      });

      codeEl.innerHTML = codeHtml;

      // Inverted status bar (attron(A_REVERSE))
      if (statusLeftEl) {
        statusLeftEl.textContent = `${this.currentFile} - ${this.lines.length} lines ${this.dirty ? '(modified)' : ''}`;
      }
      if (statusCoordEl) {
        statusCoordEl.textContent = `${this.cy + 1}/${this.lines.length}`;
      }
    },

    bindEvents() {
      let scrollArea = document.getElementById('terminalScrollArea');
      let treeEl = document.getElementById('terminalFiletree');
      let searchInput = document.getElementById('searchInput');

      // Click inside editor buffer to jump cursor
      scrollArea?.addEventListener('click', (e) => {
        this.closeContextMenu();
        this.activeFocus = 'editor';
        let targetLine = e.target.closest('.term-line');
        if (targetLine) {
          let allLines = Array.from(scrollArea.querySelectorAll('.term-line'));
          let clickedIdx = allLines.indexOf(targetLine);
          if (clickedIdx !== -1) {
            this.cy = clickedIdx;
            let len = (this.lines[this.cy] || '').length;
            this.cx = Math.min(Math.floor((e.offsetX - 4) / 9.6), len);
            if (this.cx < 0) this.cx = 0;
            this.render();
          }
        }
      });

      // Right-click context menu (ui.c)
      scrollArea?.addEventListener('contextmenu', (e) => {
        e.preventDefault();
        this.openContextMenu(e.clientX, e.clientY);
      });

      // Tree row clicks
      treeEl?.addEventListener('click', (e) => {
        this.closeContextMenu();
        let target = e.target.closest('.tree-row');
        if (target) {
          let idx = parseInt(target.dataset.idx, 10);
          if (!isNaN(idx)) {
            this.treeCursor = idx;
            this.activeFocus = 'tree';
            let cur = this.flatTreeNodes[this.treeCursor];
            if (cur) {
              if (cur.node.isDir) {
                this.treeToggleExpand();
              } else {
                this.treeOpenFile();
              }
            }
          }
        }
      });

      // Search input typing
      searchInput?.addEventListener('input', (e) => {
        this.searchQuery = e.target.value;
        this.render();
      });

      // Request keyboard lock if supported so browsers don't intercept Ctrl+N / Ctrl+T
      try {
        if (navigator.keyboard && navigator.keyboard.lock) {
          navigator.keyboard.lock(['KeyN', 'KeyT', 'KeyW', 'KeyS']).catch(() => {});
        }
      } catch (err) {}

      // Robust keydown handler with capture phase
      const handleKeydown = (e) => {
        this.closeContextMenu();

        const keyLower = (e.key || '').toLowerCase();
        const code = e.code || '';
        const isCtrlOrMeta = (e.ctrlKey || e.metaKey);
        const isAlt = e.altKey;

        // Toggle File Tree: Ctrl+N, Alt+N, F2, or Ctrl+B
        const isN = (keyLower === 'n' || code === 'KeyN' || e.keyCode === 78 || e.which === 78);
        if ((isCtrlOrMeta && isN) || (isAlt && isN) || (code === 'F2') || (isCtrlOrMeta && (keyLower === 'b' || code === 'KeyB'))) {
          e.preventDefault();
          e.stopPropagation();
          e.stopImmediatePropagation();
          this.toggleFileTree();
          return false;
        }

        // Control Keys
        if (isCtrlOrMeta) {
          switch (keyLower) {
            case 's':
              e.preventDefault();
              e.stopPropagation();
              this.saveFile();
              return false;
            case 't':
              e.preventDefault();
              e.stopPropagation();
              this.toggleLineNumbers();
              return false;
            case 'z':
              e.preventDefault();
              e.stopPropagation();
              this.undo();
              return false;
            case 'f':
            case 'w':
              e.preventDefault();
              e.stopPropagation();
              this.openSearchPrompt();
              return false;
            case 'k':
              e.preventDefault();
              e.stopPropagation();
              navigator.clipboard?.writeText(this.lines.join('\n'));
              this.setStatusMessage('Copied buffer to clipboard! (Ctrl+K)');
              return false;
            case 'q':
            case 'c':
              e.preventDefault();
              e.stopPropagation();
              if (this.dirty) {
                this.setStatusMessage('WARNING! File has unsaved changes. Press Ctrl+Q/C again to force quit.');
              } else {
                this.setStatusMessage('Session terminated. Press F5 or reload to restart.');
              }
              return false;
            case 'a':
              e.preventDefault();
              e.stopPropagation();
              this.setStatusMessage('Selected all buffer lines.');
              return false;
          }
        }

        // Search Active Key Handling
        if (this.searchActive) {
          if (e.key === 'Escape' || e.key === 'Enter') {
            e.preventDefault();
            this.closeSearchPrompt();
            return;
          }
          return;
        }

        // Tab Navigation when file tree is active
        if (this.activeFocus === 'tree') {
          switch (e.key) {
            case 'ArrowUp':
              e.preventDefault();
              if (this.treeCursor > 0) this.treeCursor--;
              this.render();
              return;
            case 'ArrowDown':
              e.preventDefault();
              if (this.treeCursor < this.flatTreeNodes.length - 1) this.treeCursor++;
              this.render();
              return;
            case 'Tab':
            case 'ArrowLeft':
            case 'ArrowRight':
              e.preventDefault();
              this.treeToggleExpand();
              return;
            case 'Enter':
              e.preventDefault();
              this.treeOpenFile();
              return;
            case 'Escape':
              e.preventDefault();
              this.activeFocus = 'editor';
              this.render();
              return;
          }
        }

        // Editor Navigation
        switch (e.key) {
          case 'ArrowUp':
            e.preventDefault();
            if (this.cy > 0) this.cy--;
            this.cx = Math.min(this.cx, (this.lines[this.cy] || '').length);
            this.render();
            return;
          case 'ArrowDown':
            e.preventDefault();
            if (this.cy < this.lines.length - 1) this.cy++;
            this.cx = Math.min(this.cx, (this.lines[this.cy] || '').length);
            this.render();
            return;
          case 'ArrowLeft':
            e.preventDefault();
            if (this.cx > 0) {
              this.cx--;
            } else if (this.cy > 0) {
              this.cy--;
              this.cx = (this.lines[this.cy] || '').length;
            }
            this.render();
            return;
          case 'ArrowRight':
            e.preventDefault();
            if (this.cx < (this.lines[this.cy] || '').length) {
              this.cx++;
            } else if (this.cy < this.lines.length - 1) {
              this.cy++;
              this.cx = 0;
            }
            this.render();
            return;
          case 'Home':
            e.preventDefault();
            this.cx = 0;
            this.render();
            return;
          case 'End':
            e.preventDefault();
            this.cx = (this.lines[this.cy] || '').length;
            this.render();
            return;
          case 'Backspace':
            e.preventDefault();
            this.deleteChar();
            return;
          case 'Enter':
            e.preventDefault();
            this.insertNewline();
            return;
          case 'Tab':
            e.preventDefault();
            if (this.fileTreeVisible && this.activeFocus === 'tree') {
              this.treeToggleExpand();
            } else {
              this.insertChar(' ');
              this.insertChar(' ');
              this.insertChar(' ');
              this.insertChar(' ');
            }
            return;
          case 'Escape':
            e.preventDefault();
            this.closeSearchPrompt();
            this.closeContextMenu();
            this.setStatusMessage('');
            return;
        }

        // Printable keys typing
        if (e.key.length === 1 && !e.ctrlKey && !e.altKey && !e.metaKey) {
          if (!this.searchActive && document.activeElement !== searchInput) {
            e.preventDefault();
            this.insertChar(e.key);
          }
        }
      };

      window.addEventListener('keydown', handleKeydown, { capture: true, passive: false });
      document.addEventListener('keydown', handleKeydown, { capture: true, passive: false });

      // Context menu clicks
      document.getElementById('ctxCopy')?.addEventListener('click', () => {
        navigator.clipboard?.writeText(this.lines.join('\n'));
        this.setStatusMessage('Copied buffer to clipboard!');
        this.closeContextMenu();
      });

      document.getElementById('ctxSelectAll')?.addEventListener('click', () => {
        this.setStatusMessage('All lines selected.');
        this.closeContextMenu();
      });

      // Footer / Top quick CLI links
      document.querySelectorAll('[data-cli-action]').forEach(el => {
        el.addEventListener('click', (e) => {
          let act = el.dataset.cliAction;
          if (act === 'winget') {
            let cmd = 'winget install Anik200.nimki';
            navigator.clipboard?.writeText(cmd);
            this.setStatusMessage(`Copied: ${cmd}`);
          } else if (act === 'tree') {
            this.toggleFileTree();
          } else if (act === 'nums') {
            this.toggleLineNumbers();
          } else if (act === 'find') {
            this.openSearchPrompt();
          } else if (act === 'save') {
            this.saveFile();
          } else if (act === 'undo') {
            this.undo();
          } else if (act === 'quit') {
            this.setStatusMessage('Nimki: press Ctrl+Q again to exit.');
          }
        });
      });
    }
  };

  document.addEventListener('DOMContentLoaded', () => {
    NimkiTerminal.init();
  });

})();
