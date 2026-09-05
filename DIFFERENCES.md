# Differences Between JVim 3 and Modern Vim / Neovim

**English** | [日本語](DIFFERENCES.ja.md)

JVim 3 is based on **JVim 3.0-j2.1b** (2002), Tsuchida Ken'ichi's Japanized build of Bram Moolenaar's **Vim 3.0** (1994), modernized with an internal UTF-8 buffer, Unicode Win32 GUI, and support for current 64-bit platforms.

Spanning three decades of evolution, JVim 3 deliberately keeps the simplicity and ultra-fast startup of Vim 3.0 without the massive complexity of modern Vim 9.x or Neovim. This document provides a quick overview of what is supported and what is omitted for users transitioning from or comparing against modern Vim / Neovim.

---

## Comparison Table

| Feature / Area | JVim 3 | Modern Vim (9.x) / Neovim |
|---|---|---|
| **Binary size / Startup time** | Single executable (~1MB), instant start (0ms) | Tens to hundreds of megabytes, complex plugin loading |
| **Internal Encoding** | **Fixed UTF-8** (converts on file I/O) | `encoding=utf-8` (configurable) |
| **Plugin Architecture** | **None** (self-contained single binary) | Vim9 script, Lua, RPC, remote plugins |
| **Scripting Language** | **None** (no functions, loops, dictionaries) | Vim script / Vim9 script / Lua |
| **LSP / Tree-sitter** | **None** | Native in Neovim, plugins in Vim |
| **Async Jobs / Built-in Terminal** | **None** (synchronous `:!` only) | `:terminal`, `job_start()`, async tasks |
| **Window Splitting** | Horizontal only (`:split`) | Horizontal and vertical (`:split`, `:vsplit`) |
| **Tab Pages** | **None** | Available (`:tabnew`, etc.) |
| **Undo History** | Multi-level linear undo (`undolevels`) | Undo tree (branching, persistent undo) |
| **Regular Expressions** | Henry Spencer regex + character classes | NFA / backtracking engines, Perl-like features |
| **Syntax Highlighting** | Custom syntax rules (`syntax/*.jvsyn`) | Vim standard syntax / Tree-sitter |
| **Colour Schemes** | `:colorscheme` supported (GUI & 24-bit SGR) | Supported |
| **Quickfix** | Yes (`-e`, `:cn`, `:cp`, `:cl`) | Yes (multiple quickfix lists, location lists) |
| **Keyboard Macros** | Yes (`qa ... q`, `@a`) | Yes |
| **Registers & Marks** | Yes (named registers `a-z`, marks `'a`) | Yes |
| **IME Control** | Mode-aware IME control (Windows GUI) | Requires plugins or OS-specific tweaks |

---

## What JVim 3 Does Not Have (Omissions)

1. **Plugin Managers and Full Scripting**
   - No user functions (`function`), loops (`for`, `while`), lists, or dictionaries.
   - Configuration is purely declarative (`set`, `map`, `hi`, `colo`, `source`).
2. **LSP and Tree-sitter**
   - Code navigation relies on local buffer keyword completion (`CTRL-N` / `CTRL-P`) and tags (`:tag`, `CTRL-]`).
3. **Vertical Splits (`:vsplit`) and Tabs (`:tabnew`)**
   - Windows can be split horizontally (`:split`, `CTRL-W s`).
4. **Built-in Terminal (`:terminal`)**
   - External shell commands are invoked via `:!cmd` or filtered via `!motion cmd`.
5. **Modern Visual Modes**
   - Blockwise visual (`CTRL-V`) and linewise visual (`V`) are absent; text manipulation relies on classic vi operators with motions (`d}`, `y$`), marks, or Ex range commands (`:10,20...`).
6. **Advanced Regex Extensions**
   - Lookaround assertions, lazy quantifiers (`\{-} `), and complex engine features are not available.

---

## What JVim 3 Offers (Strengths & Features)

1. **Zero Configuration, Lightweight**
   - Single ~1MB binary with no runtime dependencies.
2. **True UTF-8 Buffer**
   - Unlike the original 2002 Shift-JIS JVim, text is held natively in UTF-8. Non-BMP characters (emoji, mathematical symbols, etc.) and multilingual text survive without corruption.
   - Automatic detection and seamless conversion for Shift-JIS (CP932), EUC-JP, ISO-2022-JP, and UTF-8.
3. **Modern Windows & Terminal Integration**
   - Per-Monitor DPI awareness on Windows, modern Win32 Unicode APIs, automatic IME switching on leaving insert mode, and 24-bit TrueColor SGR support in terminal mode.
4. **Modern Themes Bundled**
   - Includes popular modern themes (Dracula, Gruvbox, Nord, TokyoNight, etc.) usable out of the box.
5. **Full Vi / Vim 3.0 Core Workflow**
   - Multi-level undo/redo, multiple horizontal windows, tags, quickfix, macros, and command-line history and completion.

---

## Configuration Compatibility

Modern `.vimrc` or `init.lua` files cannot be parsed by JVim 3. JVim 3 prefers `~/.jvimrc` (or `_jvimrc` on Windows).

- Standard options (`number`, `autoindent`, `tabstop`, `shiftwidth`, `expandtab`, `ignorecase`, etc.) work as expected.
- Key mappings (`:map`, `:imap`) recognize special keys such as `<CR>`, `<Esc>`, and `<Tab>`.
- See [USAGE.md](USAGE.md#a-_vimrc-to-start-from) for a recommended starter configuration.

---

## Related Documentation

- **[README.md](README.md)**: Overview and installation
- **[USAGE.md](USAGE.md)**: Usage guide, encodings, themes, limitations
- **[doc/difference.doc](doc/difference.doc)**: Detailed differences between original Vim 3.0 and vi
- **[doc/reference.doc](doc/reference.doc)**: Vim 3.0 Reference Manual
