# Using JVim 3

**English** | [日本語](USAGE.ja.md)

Getting it running, and the parts of it that are not plain vi: encodings, IME
control, the Win32 GUI. For every command and option there is Vim 3.0's own
reference in [doc/reference.doc](doc/reference.doc), and JVim's Japanese manual
in [doc.j/readme.doc](doc.j/readme.doc); this page is the shorter path to a
working editor.

- [Starting it](#starting-it)
- [First run on Windows](#first-run-on-windows)
- [First run on Unix](#first-run-on-unix)
- [A `_vimrc` to start from](#a-_vimrc-to-start-from)
- [Encodings](#encodings)
- [Japanese input](#japanese-input)
- [Display and fonts](#display-and-fonts)
- [Colour schemes](#colour-schemes)
- [What you get beyond vi](#what-you-get-beyond-vi)
- [Differences between Ex commands and Vim commands](#differences-between-ex-commands-and-vim-commands)
- [What JVim adds](#what-jvim-adds)
- [What was removed](#what-was-removed)
- [Where settings live](#where-settings-live)
- [Setting scope: buffer, window, or the whole editor](#setting-scope-buffer-window-or-the-whole-editor)
- [Known limits](#known-limits)
- [Troubleshooting](#troubleshooting)

## Starting it

| | |
| --- | --- |
| `jvim32w.exe` | Windows, GUI. Its own window; no console. |
| `jvim32.exe` | Windows, from a console. Opens a GUI window anyway unless given `-nw`. |
| `jvim3` | Unix. Uses the terminal it is started in. |

In the 64 bit package those are `jvim64w.exe` and `jvim64.exe`.

Options, all of them from vi and Vim 3.0 except where marked:

```
jvim3 [options] [file ..]
jvim3 [options] -t tag
jvim3 [options] -e [errorfile]
```

| | |
| --- | --- |
| `-v`, `-R` | Read only (view). |
| `-n` | No swap file: keep everything in memory. |
| `-b` | Binary mode. |
| `-r` | Recovery mode — read the swap file back after a crash. |
| `-T terminal` | Terminal type, instead of `$TERM`. |
| `-o[N]` | Open N windows; by default one per file named. |
| `+`, `+lnum` | Start at the end of the file, or at line `lnum`. |
| `-c command` | Run an ex command before anything else. |
| `-s scriptin` | Read commands from a script file. |
| `-w scriptout` | Write every key typed to a script file. |
| `-k code` | JVim: the file encoding (`jcode`) — `E J S U T`, lower case to skip detection. |
| `-K mask` | JVim: `jmask`, four letters. See [Encodings](#encodings). |
| `-nw` | Windows: stay in the console rather than opening a window. |
| `-no` | Windows: single instance ("one window") — a second `jvim` hands its file to the running one instead of starting another. Also on the menu as `One Window`. |
| `-n0` … `-n3` | Windows: which of the four saved GUI profiles in the registry to use. |
| `-I section` | Windows: which section of `vim32.ini` to read. |

Environment:

| | |
| --- | --- |
| `VIM` | Where the help file, the system `vimrc` and the syntax rules are looked for. Unset, the editor fills it in: the directory holding the exe on Windows, `$PREFIX/lib/jvim3` on a Unix. |
| `HOME` | Where your `.vimrc` / `_vimrc` is. On Windows, if unset, `%HOMEDRIVE%%HOMEPATH%`, or the exe's directory. |
| `VIMINIT`, `EXINIT` | Ex commands to run at startup, instead of an rc file. |
| `TERM` | Unix: which terminal entry to use. |
| `LANG`, `LC_CTYPE` | Unix: sets the default `jmask` — see [Encodings](#encodings). |
| `TMP` | Where backup and swap files go, if `backupdir` / `directory` name it. |

## First run on Windows

Unpack the zip anywhere. `%VIM%` does not need setting: the editor takes the
exe's own directory. Then:

**1. Check `:help`, and switch it if you want English.** The help file in the
package is `vim.hlp`, which is what the editor looks for (`$VIM\vim.hlp`), so
`:help` works as unpacked. It is JVim's Japanese help; for the English one, copy
[doc/vim.hlp](doc/vim.hlp) from this repository over it, or keep both and point
`helpfile` at whichever you want:

```vim
set helpfile=$VIM\vim-en.hlp
```

Packages up to `v3.0-j2.1b-utf8.4` shipped the file as `jvim3.hlp`, which
nothing looks for. Rename it, or set `helpfile` at it.

**2. Put an rc file somewhere it is read.** In order, at startup:

1. `%VIM%\vimrc` — note, no underscore
2. then the **first** of: `%VIMINIT%`, `%HOME%\_vimrc`, `%EXINIT%`, `%HOME%\_exrc`
3. and, only if `exrc` is set, `_vimrc` then `_exrc` in the current directory

Wherever `_vimrc` is looked for, **`_jvimrc` is tried first** — and `.vimrc`
likewise gives way to `.jvimrc`. Nothing but JVim reads the "j" name, so that is
where to put settings an ordinary vim on the same machine would not understand:
`set fexrc`, the syntax rules, anything else from the list [below](#what-jvim-adds).

`%VIM%\vimrc` next to the exe is the simplest, and makes the unpacked directory
self-contained. Two samples come in the package. `jvimrc.sample` is short and
works on a Unix build too — copy it to `%HOME%\_jvimrc` and you are done.
`_jvimrc.sample` is Tsuchida's own from 2002; its `tags` lines point at a
Visual C++ 6 that is not on your machine.

**3. Set the font.** `Global > Font` in the menu. Only fixed pitch fonts are
offered, because the editor draws on a character grid — a proportional font
cannot work. Any font with Japanese coverage will do; MS Gothic is there on every
Windows, Myrica and BIZ UDGothic are nicer. Choose it once and it is remembered,
per display scale.

**4. Nothing else.** Font, window size, colours, menu and scrollbar are saved to
the registry when you leave, under `HKEY_CURRENT_USER\Software\Vim`.

## First run on Unix

```sh
./scripts/build-unix.sh          # src/jvim3
sudo install -m 755 src/jvim3     /usr/local/bin/
sudo install -m 644 doc/vim.hlp   /usr/local/lib/jvim3.hlp    # or doc.j/vim.hlp
```

`/usr/local/lib/jvim3.hlp` is where `:help` looks; override with
`set helpfile=...`. `/usr/local/etc/jvim3rc` is read before anything else, if it
exists — a good place for site-wide settings. Then, as on Windows: the first of
`$VIMINIT`, `$HOME/.vimrc`, `$EXINIT`, `$HOME/.exrc`, and `./.vimrc` or
`./.exrc` if `exrc` is set.

Your locale sets the default encoding, so a normal UTF-8 login needs no encoding
setup at all:

```sh
LANG=ja_JP.UTF-8   # jmask=TTTT — UTF-8 keyboard, display, pipes and new files
LANG=ja_JP.eucJP   # jmask=EEEE
LANG=C             # jmask=EEET — EUC display, UTF-8 for new files
```

If the terminal draws Japanese as blanks or the wrong width, that is the
terminal's font, not the editor. Check `echo $TERM` matches what you are
actually in.

## A `_vimrc` to start from

This is `jvimrc.sample`, which comes in the Windows package and which `make
install` puts in `$VIM` on a Unix. Copy it to `~/.jvimrc`, or `%HOME%\_jvimrc`
— the "j" name so an ordinary vim never reads it. Cut it down rather than up;
everything here exists in both builds.

```vim
" ---- editing
set autoindent
set backspace=2         " backspace over autoindent, line breaks and insert start
set tabstop=4
set shiftwidth=4
set noexpandtab
set textwidth=0
set whichwrap=11        " backspace, space and cursor keys wrap at line ends
set wildchar=9          " TAB completes file names on the : line
set history=100
set showcmd
set showmatch
set ruler
set laststatus=1
set nobackup            " or: set backup / set backupdir=>$TMP
" set directory=>$TMP  " swap files all in one place; >/tmp on a Unix,
                        " where $TMP is usually not set

" ---- Japanese
set nojkanaconv         " leave halfwidth kana alone (the default now)
" The IME settings are the Windows build only: uncomment them there.
"set    fepctrl         " turn the IME off when leaving insert mode
"set    fepkey=\\       " CTRL-\ toggles the IME
"set    jinsertmode=a   " insert mode starts in ASCII
" set jmask=TTTT        " only if the locale is not telling the truth

" ---- colour, on the GUI and on a terminal alike
set fexrc               " read the rules for the type of file being opened
set syntax
set syntype=cfp
source $VIM/syntax/filetype.jvsyn

" a marker at the end of every line, in crchar
"set    crmark
```

`fexrc` is worth knowing about if you work in several languages: with it set,
opening `main.c` reads `.vimrc.c` if that exists, so tab width can follow the
file type. `doc.j/readme.doc` §6.13 has the details, including how to put
per-suffix blocks inside one `.vimrc`.

**No such file, and it reads the plain rc instead — not nothing.** `main.py`
with no `.vimrc.py` around does not skip the per-suffix step; `open_buffer()`
falls back to sourcing `~/.jvimrc` itself, in full, for every file that opens
this way — not only the first. `set autoindent` and the rest of [a `_vimrc` to
start from](#a-_vimrc-to-start-from) are unaffected by running twice: the same
line sets the same value. A theme picked with `:colorscheme` partway through
the session is a different case, covered under
[Colour schemes](#colour-schemes).

## Encodings

**Inside the editor everything is UTF-8.** Conversion happens when text comes in
and goes out, and `jmask` says what those edges speak. It is four letters —
**key, display, system, file**:

| Position | Means | |
| --- | --- | --- |
| 1 | key | What arrives from the keyboard. In the Win32 GUI this is always UTF-8 whatever the letter says, because the window is a Unicode one; the letter describes the console. |
| 2 | display | What is written to the terminal. Ignored by the GUI, which draws Unicode directly. |
| 3 | system | Pipes — what a command run from `:!` reads and writes. On Windows this stays `S` (CP932): that is what Windows console programs still speak. |
| 4 | file | The encoding a **brand new** file is written in. |

Each letter is one of `E` EUC-JP, `J` ISO-2022-JP, `S` Shift-JIS, `T` UTF-8.
(`U`, UCS-2, is a `jcode` value only — it makes no sense for a keyboard or a
pipe.)

```
Windows default   SSST     CP932 console and pipes, new files in UTF-8
Unix default      EEET     unless LANG says otherwise; see above
Everything UTF-8  TTTT
```

Set it with `:set jm=TTTT`, ask with `:set jm?`, or start with `-K TTTT`. A three
letter `jmask` still works and means "write new files in the system code", which
is what it meant before the fourth letter existed.

**`jcode` is per window** and is the encoding of the file in it — `E J S U T`,
and the file it was read from was detected as that. Written in **upper case** it
means "detect the encoding when reading"; lower case means "do not". So:

```vim
:set jc?          " which encoding is this buffer?
:set jc=t         " write it as UTF-8 from now on
```

```sh
jvim3 -k s file.txt      # read and write this as Shift-JIS, no detection
jvim3 -k S file.txt      # Shift-JIS unless detection says otherwise
```

To convert a file: open it (detection gets it right), `:set jc=t`, `:w`.

**Detection** looks at the whole buffer. If it parses as UTF-8 and holds at least
one multi-byte character, it is UTF-8 — Shift-JIS and EUC text never passes that
test, because their trailing bytes are outside the UTF-8 continuation range. A
character outside the BMP counts as evidence like any other.

**What round trips, and what does not:**

| | |
| --- | --- |
| UTF-8, UCS-2 | Byte for byte, including a BOM, characters outside CP932 and characters outside the BMP. |
| EUC-JP, Shift-JIS, ISO-2022-JP | Only what those encodings can hold; anything else is written as `?`. Inherent. |
| Reading UCS-2 | Still pivots through CP932, so characters outside CP932 are lost **on the way in**. Convert to UTF-8 first. Writing UCS-2 is direct and lossless. |
| Combining marks | Kept and saved, but drawn in zero columns, so they are invisible. |
| Halfwidth kana | Left alone. `jkanaconv`, which used to rewrite them to fullwidth on reading, is off by default now; `:set jkc` brings it back. |

`jbigendian` picks the byte order for UCS-2; the default is little endian.

**File names** on Windows are UTF-8 all the way to the operating system, because
`src/jvim.manifest` asks for UTF-8 as the process code page. A file called
`🍣.txt` opens, shows correctly in the title bar and completes on the `:` line.
This needs Windows 10 1903 or later; on anything older, file names are CP932 as
before.

## Japanese input

The editor knows the difference between insert mode and command mode, and can
drive the IME accordingly. That is the point of `fepctrl`, and it is the thing
modern editors mostly do not do:

| | |
| --- | --- |
| `:set fepctrl` (`fc`) | Turn the IME **off** when leaving insert or replace mode, and back on when entering it. Per window. Off by default. |
| `:set fepkey=\\` (`fk`) | Which control key toggles the IME. The value is a character from `@` to `_`, and the key is that character with `0x1f` — so `\\` means CTRL-\ (and CTRL-@). In the Win32 GUI, `fepkey=[` makes it SHIFT+SPACE. |
| `:set jinsertmode=a` (`ji`) | Which mode insert mode starts in: `a` ASCII, `j` Japanese, `A`/`J` the same but switching automatically once `jiauto` characters of the other kind have been typed. |
| `:set jiauto=10` (`ja`) | The threshold for `A`/`J` above. 0, the default, turns automatic switching off. |
| `:set fepkeys=r` (`fo`) | Which **command mode** keys also open the IME. The keys that take text — `a A i I R cw` — always do; this is for the ones that take a single character or a pattern. `r` by default; `/?tTfF` are worth adding. |

**Use the GUI on Windows.** Japanese input in console mode is unreliable — a
character can sit unshown until you press RETURN. There are no known problems
in the GUI.

On Unix, IME control needs `FEPCTRL` compiled in with a `fepseq.c` that speaks
your input method's protocol; `scripts/build-unix.sh` leaves it out. Without it
the IME is your terminal's business, which on a modern desktop is what you want
anyway: the editor receives finished UTF-8 either way.
[doc.j/fepctrl.doc](doc.j/fepctrl.doc) has the protocol details.

## Display and fonts

**The Win32 GUI is DPI aware.** It asks for the font in real pixels rather than
being drawn at 96 DPI and stretched, so text is sharp at 125% and 150%. Font and
window sizes are stored with the DPI they were set at and restated for whatever
display they end up on, including when the window is dragged to a monitor at
another scale. To go back to the old stretched-but-larger rendering: the exe's
Properties > Compatibility > Change high DPI settings.

`linespace` and `charspace` in the font dialog are raw pixels on purpose — they
are one or two pixel nudges, and scaling them would make the dialog disagree
with itself.

**Width.** A character is one or two columns. The East Asian Ambiguous class —
`→`, `±`, Greek, Cyrillic — is treated as **two** columns, which is what a
Japanese font actually draws and what `'ambiwidth'` set to `double` means in
later Vim. Letters below U+2000 (Latin-1, IPA, spacing modifiers) are one, since
a mixed font takes those from its Latin half. Combining marks are zero.

Two known limits, both in the drawing, are in [Known limits](#known-limits):
GDI's lack of colour emoji, and an emoji presentation sequence taking its base
character's (narrower) width.

`crmark` (`cm`) marks the end of each line, with the character in `crchar`
(`cc`) — `list` does the same but converts tabs as well, which is often not what
you wanted. `trackset` (`trs`) picks the character set used for ruled lines:
`as` ASCII, `jp` the Japanese box drawing characters.

Syntax colouring is its own small language — colours, `syntax link`, regexp
rules per file type. `doc.j/readme.doc` §6.26 documents it in full, and
`syntax/README` says how this tree lays the rules out.

It works on the Win32 GUI and on a terminal alike: the same colour goes out as
an SGR escape. Whether it can be asked for exactly depends on the terminal,
and `$COLORTERM` decides — `truecolor` or `24bit` gets the colour itself,
anything else the nearest of the sixteen a terminal has always had. `set
nosyntax` if the terminal you are in cannot colour at all.

A region (the `p` search mode — how a C comment is coloured) or a tag's
delimiters (`<` and `>` for HTML) colour correctly regardless of length: an
unterminated region colours the rest of the file, and the lines below the one
you are typing on recolour as soon as you type the token that opens or closes
one. §6.28's `synlines` is still accepted but has no effect.

A colour can also say what goes behind it, with `on` and a second colour:

```vim
syntax link Error   bolic white on maroon
syntax link DiffAdd       green on #e6ffe6
syntax link MdCode              on #f0f0f0
```

Either a name a foreground would take, or the colour itself as `#rrggbb` —
which is what the rule files use, none of the sixteen named ones being pale
enough to read a whole line of text off. With nothing in front of the `on` the
text keeps the colour it would have had. `text` and `reverse` are refused:
neither names a colour, so neither can be behind anything. `bold`, `italic` and
`uline` belong to the text and go where they always did.

`Error` and `Todo` are drawn this way now. Both are groups vim gives a
background, and without one they had been standing in with `bolic red` and
`reverse` — and reverse is the terminal swapping two colours it already has,
which is not blue on yellow and is not the same twice on two terminals.

A pale ground is also what marks off a block that is not the language around it:
the added and removed lines of a diff, a fenced block in Markdown, and the body
of an HTML `<script>` or `<style>`. The last of those is as far as an HTML rule
can go — what is between `<script>` and `</script>` is JavaScript, and one rule
set has no way to hand a range of the buffer to another — but a page of script
now reads as a block instead of as prose that happens to have no colour in it.

Which file types have rules is a question about the rule files, not about the
editor. What came with JVim in 2002 was C/C++, Java, VBScript, HTML, `.bat`,
`.ini`, `.def`, `.rc` and `_vimrc` itself. This tree adds Python,
JavaScript/TypeScript, Go, Rust, Ruby, shell, Markdown, JSON, YAML, TOML, SQL,
CSS/SCSS, C#, PHP, Lua, XML, diff, Makefile and Dockerfile, and lets the C rules
cover `.cc`, `.cxx`, `.hpp`, `.hxx`, `.hh` and `.inl` as well.

They live one file per type in `syntax/`, which the package ships beside the exe
and `make install` puts in `$VIM`. One line in an rc reaches all of them:

```vim
set fexrc
set syntax
source $VIM/syntax/filetype.jvsyn
```

`filetype.jvsyn` is the dispatcher: it reads `common.jvsyn` for the colours and
group names, then the one file that goes with what you are opening. Adding a
type is `syntax/zig.jvsyn` plus three lines in `filetype.jvsyn`; `syntax/README`
has the details, including the things about the rule language that will
otherwise waste your afternoon.

A rule that colours the wrong thing has no other way of telling you, so ask:

```vim
:syntax dump /tmp/out
```

writes one line per coloured run — `3:4-6 Conditional w/if` is line 3, bytes 4
to 6, the group, and the rule. It comes from the same code the screen draws
from, and needs no window: `jvim3 -s cmds file.py` with `:syntax dump` in the
script is the whole loop. `scripts/test-syntax.sh` is that loop as a suite.

`$VIM` is where all this is looked for. On Windows the editor sets it to the
directory the exe is in, so an unpacked package needs nothing. On a Unix it is
`/usr/local/lib/jvim3` unless `PREFIX` said otherwise; running out of a build
tree, set it by hand.

## Colour schemes

`syntax link` above sets one group's colour by editing a rule file. A colour
scheme does the same thing from a command, and gives the result a name:

| | |
| --- | --- |
| `:colorscheme {name}` (`:colo`) | Load `{name}`. With no argument, reports the one that is active. |
| `:highlight ...` (`:hi`) | Vim's own syntax for setting one group, straight from the command line or a script — this is what a colour scheme file is built from. |
| `set background=dark\|light` (`bg`) | Only switches anything while the active scheme is `default` or `default-light`: it toggles between those two. A named theme (`dracula`, `nord`, ...) is left as it is. |

Sixteen themes are bundled, in `colors/`, packaged and installed the same way
as `syntax/`: `default` (dark), `default-light`, `catppuccin-mocha`, `catppuccin-latte`,
`dracula`, `everforest`, `gruvbox`, `kanagawa`, `monokai`, `nord`, `one-dark`,
`rose-pine`, `desert`, `tokyonight`, `solarized-dark`, `solarized-light`.

```vim
:colorscheme dracula
:colo                " with nothing after it, reports "dracula"
:set background=light   " default <-> default-light; named themes ignore this
```

**The theme is one setting for the whole editor, not one per file.** `set
autoindent`, `tabstop` and the like are each buffer's own — a new buffer starts
from whatever the current one has, and the two can then be set differently
without either noticing (see [Setting
scope](#setting-scope-buffer-window-or-the-whole-editor)). Colour is not:
`:colorscheme dracula` in one window is `:colorscheme dracula` everywhere,
including a file opened afterwards with `:split` or `:e`, because
it is a choice about how the editor looks rather than about the file in the
window. What does vary per file is which groups a buffer's rules can colour at
all — a `.py` only has the group names `python.jvsyn` uses — but the colour
behind a given group name is the same one theme, in every buffer, all the
time.

**Where a name is looked for**, first match wins:

1. `~/.jvim/colors/{name}.vim`, then `{name}.jvsyn`
2. Windows only: `%HOME%\_jvim\colors\{name}.vim`, then `{name}.jvsyn`
3. `$VIM/colors/{name}.vim`, then `{name}.jvsyn`
4. `$VIM/syntax/{name}.jvsyn`

A found file is simply sourced, so a scheme of your own is
`~/.jvim/colors/mine.vim`, written as `:hi` lines:

```vim
set background=dark
hi clear
let g:colors_name = "mine"

hi Comment    guifg=#6a9955
hi Statement  guifg=#c586c0 gui=bold
hi String     guifg=#ce9178
hi Error      guifg=#ffffff guibg=#f44747
hi DiffAdd    guifg=green   on #e6ffe6
hi link Type  Statement
```

What `:hi` takes:

| | |
| --- | --- |
| `guifg=`, `guibg=` | A colour: `#rrggbb`, or any name `syntax link` already takes (see [Display and fonts](#display-and-fonts) and `syntax/README`). `NONE`, `guifg=bg` or `guibg=fg` clears it. |
| `gui=bold,italic,underline` | Text attributes. Only one is kept: `bolic` if both bold and italic are given, otherwise whichever of bold, italic, underline came first. |
| `ctermfg=`, `ctermbg=`, `cterm=` | Read only when the matching `gui*` key is absent — a scheme written for a terminal-only Vim still colours something. |
| `hi link {Group} {Target}` | `{Group}` takes `{Target}`'s colour, and keeps following it if `{Target}` is relinked later. |
| `hi clear` | Drop every link and colour, back to plain text. |
| `hi Normal ...` | On the Windows GUI/console, `guifg`/`guibg` here override the Text Color / Back Color the user configured, for as long as the scheme keeps setting them; `hi clear` (or a scheme that never sets `Normal`) puts the configured colours back. Accepted but with nothing to override on a plain terminal. |

**This reads a fixed set of directives, not general Vimscript.** `set`,
`hi`, `let g:colors_name = "..."`, `finish`, and `if` / `elseif` / `else` /
`endif` are all recognised — enough for a scheme written the way the eleven
bundled ones are. The condition on an `if` is never evaluated, though: every
line inside runs regardless, in file order, so a scheme with one branch for
`has('gui_running')` and another for a terminal applies both, and whichever
`hi` came last wins. Anything else in a scheme downloaded from elsewhere —
functions, loops, anything not in this list — is silently skipped. If a
theme looks wrong after `:colorscheme`, check it against the bundled ones in
`colors/` before assuming the bug is here.

## What you get beyond vi

Vim 3.0 is vi plus this short list:

- **Multi level undo.** `u` again and again; `CTRL-R` to redo. `undolevels`.
- **Several windows.** `:split`, `CTRL-W` commands, `:buffers`. See
  [doc/windows.doc](doc/windows.doc).
- **Command line history and completion.** Arrow keys through `:` and `/`
  history; `wildchar` (TAB) completes file names.
- **Block operations**, counts on nearly everything, `showmatch`, `smartindent`,
  digraphs, recording, `:!` filters, quickfix (`-e`, `:cn`).
- **Keyword completion** in insert mode, `CTRL-N` / `CTRL-P`.
- **Text formatting** with the `Q` operator, e.g. `Q}` formats a paragraph to `textwidth`.

The `Q` command (go to Ex mode) from vi is missing. That is the only thing from vi that is not here, and
[doc/difference.doc](doc/difference.doc) is the full account of what changed in
either direction.

## Differences between Ex commands and Vim commands

JVim 3 (and vi/Vim in general) divides actions into two primary interaction models: **normal mode Vim commands** and **line-oriented Ex commands** prefixed with `:`.

| Category | Vim Commands (Normal mode) | Ex Commands (Command-line mode) |
|---|---|---|
| **Input method** | Direct keystrokes in normal mode (`dd`, `cw`, `gg`, `>>`, etc.) | Entered on the command-line after `:` (`:w`, `:s`, `:global`, etc.) |
| **Target scope** | Cursor position, characters, words, text motions | Primarily line ranges (line numbers, `%`, `'a,'b`, etc.) |
| **Primary purpose** | Immediate cursor navigation, focused in-place editing | File operations, global substitutions, settings, external pipes |
| **Execution** | Executes immediately on keystroke without <kbd>Enter</kbd> | Requires pressing <kbd>Enter</kbd> to confirm and execute |

### Key characteristics and guidelines

1. **Range operations are much more expressive in Ex commands**
   - In normal mode, deleting 5 lines requires `5dd`. In Ex mode, `:10,20d` deletes lines 10 through 20 directly, and `:%s/old/new/g` substitutes text across the entire buffer without moving the cursor manually across every line.
2. **Operations present in both forms**
   - Indenting: normal mode `>>` vs Ex command `:>` (e.g., `:10,20>`).
   - Toggling case: normal mode `~` vs Ex command `:~`.
3. **Rule of thumb**
   - Use **Vim commands** (`ci"`, `dw`, `x`, `p`) for visual and tactile local editing around the cursor.
   - Use **Ex commands** (`:w`, `:q`, `:%s`, `:set`, `:r`) for buffer lifecycle actions, bulk replacements, and editor configuration.

## What JVim adds

Beyond the encoding and IME options above, the Japanization added these. The
full descriptions are in [doc.j/readme.doc](doc.j/readme.doc) §6 to §9.

| | |
| --- | --- |
| `jreplace` (`jrep`) | Replace mode counts characters rather than columns: `R` with `あいう` over `123456` leaves `あいう456`, not `あいう`. On by default. |
| `jtilde` (`jt`) | Let `~` work on Japanese too — `あ` to `ア`, `♂` to `♀`. Off by default. |
| `jignorecase` (`jic`) | Loose searching: `a`, `A`, `ａ` and `Ａ` all match each other. Off by default. |
| `smartcase` (`scs`) | Override `ignorecase` when the search pattern contains uppercase characters. Off by default. |
| `jjoinspaces` (`jjs`) | Whether `J` puts a space between two Japanese lines. |
| `window` (`win`) | GUI window size, `:set win=80,25`, usable from a modeline. |
| `whichwrap` (`ww`) | The original bits 1–16, plus 32 and 64, which change where a word motion stops when Japanese, ASCII and spaces meet. |
| `option` | A bit mask of things that were "useful but certainly not standard": `1` makes `@` and `&` on the `:` line mean this file's name and its directory, `32` makes quickfix ignore everything that is not an error line, `128` restores the original binary mode. §6.19. |
| `foption` | The same, per file, applied on read and write: `1` strip trailing whitespace, `2` fullwidth space to space, `4` tabs to spaces, `8` leading spaces to tabs, `64` smarter `%` in C, `256` read and write CR-terminated (classic Mac) files. §6.20. |
| `gh`, `gx`, `gp`, `gn`, `gg` | Help; ruled-line drawing mode (ESC leaves it, `gp`/`gn` change the line style); and grep jump — put the output of `grep -n` in a buffer and `gg` on a line jumps there like a tag. |
| `gX`, `gC`, `gV` | GUI: cut, copy and paste through the clipboard, i.e. Windows' own CTRL-X, C, V. |
| Extended regexps | The Vim 5.7 set — `\d \w \s \a \l \u \h \i \k \f \p \x \o` and their negations, `\e \t \r \b \n`. Character classes compare whole characters, so `[あ]` matches `あ` and nothing else. §6.29. |
| Key names for `map` | `#[UP]`, `#[SLEFT]`, `#[F01]`–`#[F20]`, `#[HELP]`, `#[UNDO]`, `#1`–`#0`. §6.30. |
| Character names for `map` | `<CR>` `<NL>` `<LF>` `<Esc>` `<Tab>` `<Space>` `<BS>` `<Nul>`, in either half and in any case, so a mapping can press Enter without holding a carriage return of its own: `map q ihello<CR>`. A rule that does hold one still works, but only in a file with CRLF endings — `dosource` takes one CR off the end of every line and cannot tell that one from a separator, which is why the same rc could not be written for a Unix and for Windows before. A `<` that starts nothing in the list stays a `<`, so a mapping that types `<div>` still says so; `CTRL-V` before it holds off one that would otherwise be read as a name. |
| `=` operator / internal re-indent | Re-indents the specified range using `=` commands (`==`, `=G`, etc.). When `equalprg` (`ep`) is empty, JVim re-indents internally using buffer settings (`smartindent`, `cinwords`, `shiftwidth`) without requiring external tools. For external formatters, set `:set equalprg=clang-format`. Windows releases include a standalone lightweight C code formatter `tools/cformat.exe`. |
| `CTRL-G` | Reports the character code under the cursor and the file's encoding as well as the position: `line 36 of 36 --100%-- col 0/1 ch 0x31 [E]`. |

| Windows GUI | Menus, scrollbars, drag and drop, a tray icon, four saved profiles, an about box that reports what it found. |

## What was removed

Three features of JVim 2.1b are gone from this tree, sources and all, because
their terms made it awkward to redistribute — see
[README.md](README.md#licence). If you find them in `doc.j/readme.doc`, that is
history:

| Gone | Was |
| --- | --- |
| BDF bitmap font rendering | `USE_BDF`, and the `Global > BDF Font` menu items. Any outline font covers more characters than a BDF ever did. |
| Editing inside LHA / ZIP / CAB / TAR, and over ftp | `USE_EXFILE`. It also needed UNLHA32.DLL and friends at run time. |
| MIME / uuencode / base64 decoding — the `gu` command and the `decode` option | `USE_MATOME`, part of the same directory. **An old `_vimrc` with `set decode=a` in it will now report an unknown option.** |

`Global > Unicode Font` is gone as well: it chose between the ANSI and the
Unicode drawing API, and the drawing has been unconditionally Unicode since the
UTF-8 rewrite.

The standalone `grep.exe`, `clip.exe` and `vim32s.exe` are not built either,
though they are still in `src/makjnt.mak` for anyone who wants them. The
in-editor search extensions (`USE_GREP`, `gg`) are included.

## Where settings live

| | |
| --- | --- |
| Unix | `/usr/local/etc/jvim3rc`, then `$VIMINIT` / `$HOME/.vimrc` / `$EXINIT` / `$HOME/.exrc`, then `./.vimrc` or `./.exrc` if `exrc` is set. |
| Windows | `%VIM%\vimrc`, then `%VIMINIT%` / `%HOME%\_vimrc` / `%EXINIT%` / `%HOME%\_exrc`, then `_vimrc` or `_exrc` in the current directory if `exrc` is set. |
| Windows, GUI state | `HKEY_CURRENT_USER\Software\Vim`, and `\Software\Vim\0` to `\3` for the four profiles `-n0` to `-n3` select. Font, window size, colours, menu and scrollbar. |
| Windows, `vim32.ini` | An ini file next to the exe, read for the things that have to be known before the window exists — rows, columns, printer, bitmap, sound. `-I section` picks the section. `doc.j/readme.doc` §9.3. |

One migration note if you are coming from an older JVim: a `vim32.ini` written
by it has its non-ASCII values in CP932, and Windows reads a BOM-less ini through
the process code page, which is UTF-8 here — so they are misread. Saving the
settings again from JVim fixes it. Font names in the registry are converted
automatically.

## Setting scope: buffer, window, or the whole editor

`:set` covers all three, and nothing here says which is which. Three groups:

| | |
| --- | --- |
| Per buffer | `autoindent`, `tabstop`, `shiftwidth`, `expandtab`, `binary`, `endofline`, `modeline`, `readonly`, `list`, `number`, `jcode`, and most of what a rule file or a per-suffix rc would plausibly want to change per file type. |
| Per window | Whether syntax colouring is drawn at all (`syntax`/`syt`) — two windows on the same buffer can differ. |
| The whole editor | The active colour scheme and every `:highlight` group in it (see [Colour schemes](#colour-schemes)), `backup`, and anything else that is a plain global rather than a per-buffer or per-window one. |

**A buffer option is not read from the rc again for a new buffer — it is
copied from whichever buffer is current** the moment the new one is created
(`buf_copy_options()`). Two files opened from the same session normally end up
with the same `tabstop` only because they both trace back to the same `set
tabstop=4` in `~/.jvimrc`; `:set shiftwidth=2` in one buffer and then `:e`
another file does not carry the 2 across, because the new buffer copies
whatever was current at that moment, which was 2 — it is not going back to the
rc file at all. `fexrc`, covered under [a `_vimrc` to start
from](#a-_vimrc-to-start-from), then runs on top of that copy, so a per-suffix
rc's own `set` lines are the last word for that one buffer.

A colour scheme is the odd one out precisely because it looks like a per-buffer
thing — `:highlight` reads a rule file's group names — but is stored once, for
every buffer, the same way `backup` is. That is also why it does not need
copying into a new buffer the way `tabstop` does: there is only one copy to
begin with.

## Known limits

- **Nothing types at the Windows builds in CI.** Both are now *run* there, by
  `scripts/test-winrun.sh` — 6 cases through script input, on the packages the
  release page serves — but script input cannot reach a cursor key or CTRL-@,
  and the 16 cases in `scripts/test-winkeys.sh` that do need a Windows machine
  with a compiler and are run by hand.
- **GDI draws no colour emoji.** A colour glyph needs DirectWrite; what you get
  is the fallback font's monochrome outline. It is the right width and it edits
  correctly.
- **An emoji presentation sequence gets its base character's width.** `⚠️` is
  allotted one column where a font draws two, so it leans on its neighbour.
  Fixing it needs a screen cell that can hold a sequence rather than one code
  point.
- **Reading a UCS-2 file still pivots through CP932** (see
  [Encodings](#encodings)), so characters outside CP932 are lost on the way
  in. Writing UCS-2 is direct and lossless. Convert the file to UTF-8 first.
- **Saving to EUC-JP, Shift-JIS or ISO-2022-JP keeps only what those encodings
  have.** Inherent, not a bug. UTF-8 and UCS-2 round trip byte for byte.
- **A byte that is not valid text is replaced by `?` on the way in, and saving
  writes the `?`.** The buffer holds only valid UTF-8 — every width
  calculation, cursor motion and character length in the editor is built on
  that — so bytes that are not characters cannot be carried through it. In a
  file that is mostly text with one bad byte, that byte is all that is lost. In
  a file that is mostly not text, the character before a bad byte can go with
  it, and a multi-byte sequence cut off by the end of the file comes back as an
  ASCII letter. **`jvim3 -b file` round trips any file byte for byte**, and is
  the answer whenever the bytes matter more than the text.
- **Colouring a line with thousands of things to colour on it still costs more
  than the line is long.** A minified page or bundle is one line, and one
  ordinary one is fine now: 17 kB of tags on a line draws in about a second,
  where it took 73. But two of the searches behind the colouring still start
  from the top of the line at every match — the word index the `w` rules are
  looked up in, and the walk that says whether a byte is inside a `<`…`>` — so a
  line dense enough to hold twelve thousand coloured runs takes about two
  seconds to draw rather than the tenth of one it should. `:set nosyntax` is
  instant on any of them.
- **An HTML tag with a line break inside a quoted value is not coloured over the
  break.** `<div class="a` / `b" id="c">` is what a page written with a long list
  of classes looks like, and the value is left plain on the lines the break
  separates — with any word in it that is also a tag name, `a` or `center` or
  `var`, coloured as a tag name. What comes after the value is right: the
  attribute names and values that follow the break keep their colours. Reaching
  across the break needs a region, and the rule language has no way to say
  "a region, but only inside a tag" — the same missing idea that leaves a
  `<script>` body one grey block instead of JavaScript.
- **Japanese input in console mode is unreliable on Windows** (see
  [Japanese input](#japanese-input)), and was in 2002 too. Use the GUI.
- **A colour scheme file is read by a fixed set of directives, not a
  Vimscript interpreter** — see [Colour schemes](#colour-schemes) for what
  that means for a theme brought in from elsewhere.
- **Nothing here has been tried on real hardware with a real IME at length.**
  CI is runners, ptys and serial consoles. Reports are welcome.

## Troubleshooting

| | |
| --- | --- |
| **Japanese comes out as garbage** | `:set jm?` and `:set jc?`. `jc` is what this file is; `jm`'s second letter is what the terminal is being sent. On Unix, `LANG` should set both — `LANG=ja_JP.UTF-8`. |
| **`:help` says the file was not found** | The editor wants `$VIM\vim.hlp` on Windows, or `/usr/local/lib/jvim3.hlp` on Unix. Packages up to `v3.0-j2.1b-utf8.4` shipped it as `jvim3.hlp`: rename it, or `set helpfile=$VIM\jvim3.hlp`. |
| **A new file is saved in the wrong encoding** | The fourth letter of `jmask`. `:set jm=SSST` writes new files as UTF-8 while keeping CP932 pipes. |
| **Text is soft or uneven on a scaled display** | It should not be; report it. As a workaround, Properties > Compatibility > Change high DPI settings on the exe. |
| **The font dialog does not offer the font I want** | Only fixed pitch fonts are listed, by design. A proportional font cannot be drawn on a character grid. |
| **Startup reports an unknown option** | An rc file from an older JVim — `decode` is the usual one; see [above](#what-was-removed). The `_jvimrc.sample` shipped from `v3.0-j2.1b-utf8.5` on no longer does this. |
| **`:colorscheme` says nothing changed** | `:colo` with no argument reports the active name; if it is not the one expected, the file was not found under any of the paths in [Colour schemes](#colour-schemes). |
| **A downloaded colour scheme colours only part of the buffer** | It likely branches on `has('gui_running')` or similar — see [Colour schemes](#colour-schemes) for why every branch of an `if` runs. |
| **Japanese input drops or delays characters** | Console mode on Windows. Use `jvim32w.exe`. |
| **Cursor keys insert letters, or the screen is wrong** | Unix: `$TERM`. The build links a curses/termcap library if it finds one and otherwise uses its own compiled-in entries, which cover fewer terminals; `./scripts/build-unix.sh` prints which. |
| **It crashed** | Windows writes a report to `%LOCALAPPDATA%\jvim3\`. `scripts/resolve-crash.sh <report.log>` turns the addresses in it into function names and line numbers, given the matching `.debug` file. Please attach it to an issue. |
| **It crashed, or the connection dropped, on Unix** | It says which signal it caught, writes the swap files out and names them. `jvim3 -r <file>` reads your work back; delete the `.swp` afterwards. Please quote the line it printed in an issue. A closed terminal or a dropped ssh session (SIGHUP) is the same path, and not a crash. |
| **Something else** | <https://github.com/kuwa72/jvim3/issues>. What you typed, what happened, which build, and `:set jm? jc?` if it involves text. |
