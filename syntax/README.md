# Nimki Language Syntax Definitions

Nimki includes built-in syntax highlighting for 40+ major programming, scripting, and markup languages:
- **Languages**: C, C++, Python, JavaScript, TypeScript, Rust, Go, Java, C#, PHP, Ruby, Swift, Kotlin, Shell/Bash, HTML, CSS, XML, JSON, YAML, TOML, INI, SQL, Markdown, Lua, Zig, Dart, Perl, Haskell, Scala, R, Julia, Elixir, Erlang, Clojure, Nim, Makefile, Dockerfile, Batch, PowerShell, GraphQL, Protobuf.

---

## Adding Support for New Languages

To add syntax highlighting for **any other language** (from Assembly and Fortran to Solidity, V, or your own custom DSL), simply drop a `.syntax` file into this directory or `~/.config/nimki/syntax/` (or `%APPDATA%\nimki\syntax\` on Windows).

Nimki automatically discovers and loads all `.syntax` files at launch.

### Syntax File Format

Create a file named `<language>.syntax` with the following structure:

```ini
# Language display name shown in the status bar
name: Assembly

# File extensions or exact filenames (separated by spaces)
extensions: .asm .s .inc

# Single-line comment delimiter (e.g., //, #, ;, --, %, !)
comment_single: ;

# Multi-line comment delimiters (optional)
comment_multi_start: /*
comment_multi_end: */

# Primary keywords (separated by spaces or across multiple lines)
keywords: mov add sub mul div jmp je jne jg jl call ret push pop nop xor and or not inc dec cmp test lea syscall int

# Secondary keywords / Types / Registers (separated by spaces)
types: eax ebx ecx edx esp ebp esi edi rax rbx rcx rdx rsp rbp rsi rdi r8 r9 r10 r11 r12 r13 r14 r15 al ah bl bh cl ch dl dh byte word dword qword ptr
```

### Precedence

External `.syntax` files take precedence over built-in definitions, allowing you to easily customize or override any language's keywords or comment styles without recompiling.
