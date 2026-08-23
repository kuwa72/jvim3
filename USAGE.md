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
- [What you get beyond vi](#what-you-get-beyond-vi)
- [What JVim adds](#what-jvim-adds)
- [What was removed](#what-was-removed)
- [Where settings live](#where-settings-live)
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
`_jvimrc.sample` is Tsuchida's own from 2002, longer and worth reading, though
its `tags` lines point at a Visual C++ 6 that is not on your machine.

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
character outside the BMP counts as evidence like any other. Both of those were
wrong before this fork, which is why Japanese UTF-8 used to be read as Shift-JIS
and why one emoji could tip a whole file over.

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
character can sit unshown until you press RETURN — and was in 2002 too. There
are no known problems in the GUI.

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

Two known limits, both in the drawing:

- **GDI draws no colour emoji.** Colour glyph layers need DirectWrite; what
  appears is the fallback font's monochrome outline. Width and editing are
  correct.
- **An emoji presentation sequence gets its base character's width.** `⚠️` is
  U+26A0 (Neutral, so one column) plus a variation selector (zero), and fonts
  draw it double width, so it leans on its neighbour. Fixing this needs a screen
  cell that can hold a sequence rather than a single code point.

`crmark` (`cm`) marks the end of each line, with the character in `crchar`
(`cc`) — `list` does the same but converts tabs as well, which is often not what
you wanted. `trackset` (`trs`) picks the character set used for ruled lines:
`as` ASCII, `jp` the Japanese box drawing characters.

Syntax colouring is its own small language — colours, `syntax link`, regexp
rules per file type. `doc.j/readme.doc` §6.26 documents it in full, and
`syntax/README` says how this tree lays the rules out.

It used to be the Win32 GUI only. It now works on a terminal as well: the same
colour goes out as an SGR escape. Whether it can be asked for exactly depends on
the terminal, and `$COLORTERM` decides — `truecolor` or `24bit` gets the colour
itself, anything else the nearest of the sixteen a terminal has always had.
`set nosyntax` if the terminal you are in cannot colour at all.

One thing there is out of date. A multi-line region — the `p` search mode, which
is how a C comment is coloured — used to be found by searching `synlines` lines
in each direction from the line being drawn, so a comment or a string longer
than that lost its colour, and one that was never closed had none at all. This
tree remembers instead, for each line, which region was open when the line above
ended: length stops mattering, an unterminated region colours the rest of the
file, and the lines below the one you are typing on recolour as soon as you type
the token that opens or closes a region. §6.28's `synlines` now only reaches the
tag search (`t` mode).

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

## What you get beyond vi

Vim 3.0 is vi plus a short list, and the short list is why anyone used it:

- **Multi level undo.** `u` again and again; `CTRL-R` to redo. `undolevels`.
- **Several windows.** `:split`, `CTRL-W` commands, `:buffers`. See
  [doc/windows.doc](doc/windows.doc).
- **Command line history and completion.** Arrow keys through `:` and `/`
  history; `wildchar` (TAB) completes file names.
- **Block operations**, counts on nearly everything, `showmatch`, `smartindent`,
  digraphs, recording, `:!` filters, quickfix (`-e`, `:cn`).
- **Keyword completion** in insert mode, `CTRL-N` / `CTRL-P`.

The `Q` command is missing. That is the only thing from vi that is not here, and
[doc/difference.doc](doc/difference.doc) is the full account of what changed in
either direction.

## What JVim adds

Beyond the encoding and IME options above, the Japanization added these. The
full descriptions are in [doc.j/readme.doc](doc.j/readme.doc) §6 to §9.

| | |
| --- | --- |
| `jreplace` (`jrep`) | Replace mode counts characters rather than columns: `R` with `あいう` over `123456` leaves `あいう456`, not `あいう`. On by default. |
| `jtilde` (`jt`) | Let `~` work on Japanese too — `あ` to `ア`, `♂` to `♀`. Off by default. |
| `jignorecase` (`jic`) | Loose searching: `a`, `A`, `ａ` and `Ａ` all match each other. Off by default. |
| `jjoinspaces` (`jjs`) | Whether `J` puts a space between two Japanese lines. |
| `window` (`win`) | GUI window size, `:set win=80,25`, usable from a modeline. |
| `whichwrap` (`ww`) | The original bits 1–16, plus 32 and 64, which change where a word motion stops when Japanese, ASCII and spaces meet. |
| `option` | A bit mask of things that were "useful but certainly not standard": `1` makes `@` and `&` on the `:` line mean this file's name and its directory, `32` makes quickfix ignore everything that is not an error line, `128` restores the original binary mode. §6.19. |
| `foption` | The same, per file, applied on read and write: `1` strip trailing whitespace, `2` fullwidth space to space, `4` tabs to spaces, `8` leading spaces to tabs, `64` smarter `%` in C, `256` read and write CR-terminated (classic Mac) files. §6.20. |
| `gh`, `gx`, `gp`, `gn`, `gg` | Help; ruled-line drawing mode (ESC leaves it, `gp`/`gn` change the line style); and grep jump — put the output of `grep -n` in a buffer and `gg` on a line jumps there like a tag. |
| `gX`, `gC`, `gV` | GUI: cut, copy and paste through the clipboard, i.e. Windows' own CTRL-X, C, V. |
| Extended regexps | The Vim 5.7 set — `\d \w \s \a \l \u \h \i \k \f \p \x \o` and their negations, `\e \t \r \b \n`. Character classes compare whole characters, so `[あ]` matches `あ` and nothing else. §6.29. |
| Key names for `map` | `#[UP]`, `#[SLEFT]`, `#[F01]`–`#[F20]`, `#[HELP]`, `#[UNDO]`, `#1`–`#0`. §6.30. |
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

## Troubleshooting

| | |
| --- | --- |
| **Japanese comes out as garbage** | `:set jm?` and `:set jc?`. `jc` is what this file is; `jm`'s second letter is what the terminal is being sent. On Unix, `LANG` should set both — `LANG=ja_JP.UTF-8`. |
| **`:help` says the file was not found** | The editor wants `$VIM\vim.hlp` on Windows, or `/usr/local/lib/jvim3.hlp` on Unix. Packages up to `v3.0-j2.1b-utf8.4` shipped it as `jvim3.hlp`: rename it, or `set helpfile=$VIM\jvim3.hlp`. |
| **A new file is saved in the wrong encoding** | The fourth letter of `jmask`. `:set jm=SSST` writes new files as UTF-8 while keeping CP932 pipes. |
| **Text is soft or uneven on a scaled display** | It should not be; report it. As a workaround, Properties > Compatibility > Change high DPI settings on the exe. |
| **The font dialog does not offer the font I want** | Only fixed pitch fonts are listed, by design. A proportional font cannot be drawn on a character grid. |
| **Startup reports an unknown option** | An rc file from an older JVim — `decode` is the usual one; see [above](#what-was-removed). The `_jvimrc.sample` shipped from `v3.0-j2.1b-utf8.5` on no longer does this. |
| **Japanese input drops or delays characters** | Console mode on Windows. Use `jvim32w.exe`. |
| **Cursor keys insert letters, or the screen is wrong** | Unix: `$TERM`. The build links a curses/termcap library if it finds one and otherwise uses its own compiled-in entries, which cover fewer terminals; `./scripts/build-unix.sh` prints which. |
| **It crashed** | Windows writes a report to `%LOCALAPPDATA%\jvim3\`. `scripts/resolve-crash.sh <report.log>` turns the addresses in it into function names and line numbers, given the matching `.debug` file. Please attach it to an issue. |
| **Something else** | <https://github.com/kuwa72/jvim3/issues>. What you typed, what happened, which build, and `:set jm? jc?` if it involves text. |
