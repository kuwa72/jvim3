# Building JVim 3 for Windows with mingw-w64

The original Windows build (`src/makjnt.mak`) needs the MS SDK and `nmake`.
This adds a mingw-w64 build of a minimal configuration: core editor, Win32 GUI,
and kanji/UTF-8 file I/O.

For Linux, the BSDs and macOS see [BUILDING-unix.md](BUILDING-unix.md).
日本語の手順は [BUILDING.ja.md](BUILDING.ja.md) にあります。

## Build

Cross compile from Linux/WSL:

```sh
sudo apt install gcc-mingw-w64-i686-win32 gcc-mingw-w64-x86-64-win32
./scripts/build-mingw.sh both       # jvim32w.exe (GUI) + jvim32.exe (console)
```

Those package names, and not the `mingw-w64` metapackage or a Homebrew
toolchain, because **the C runtime has to be msvcrt** -- see [Which C
runtime](#which-c-runtime) below. `build-mingw.sh` says which one it is on every
build and refuses to package the wrong one.

Or natively, in the MSYS2 **MINGW32** shell:

```sh
pacman -S mingw-w64-i686-gcc make
./scripts/build-mingw.sh both
```

Output goes to `dist/i686/`, together with `vim.hlp` and a sample `_jvimrc`.
Copy that directory to the Windows side and point `%VIM%` at it.

For something to hand over, `release` builds both architectures and zips them:

```sh
./scripts/build-mingw.sh release        # release/jvim3-<version>-win{32,64}.zip
VERSION=v3.0-j2.1b-utf8.2 ./scripts/build-mingw.sh release
```

The version comes from `git describe` unless `VERSION` says otherwise. The
executables are named for the architecture inside the package -- `jvim64w.exe`
in the 64 bit one -- because the makefile calls its targets `jvim32*.exe`
whichever architecture it is building. This is what CI runs for a release.

Other targets: `clean`, `warn` (compile with warnings shown), `split` (move the
debug info into `jvim32w.exe.debug` and strip the exe).

Switching `ARCH`, or turning `warn` on or off, needs no `clean` in between: the
object directory and the exe names are shared between the two architectures, so
`obj-mingw` holds a stamp naming what its contents were built for, and a change
of toolchain or of warning flags rebuilds and relinks everything. Building the
same way twice still compiles nothing. Without that stamp, a 64 bit build over
32 bit objects stopped at `file format not recognized`, and going back the other
way was worse: the objects were up to date, so make left the exe from the other
architecture where it was.

### 32 bit or 64 bit

**32 bit is what is released**, and it runs fine on Windows 11 x64 under WoW64.

`ARCH=x86_64` used to compile with 37 places where a pointer went through an
`int` or a `long` and lost half of itself, plus 9 hard errors. Those are gone:
menu handles and `ShellExecute()` results are `UINT_PTR`/`INT_PTR`, the dialog
procedures return `INT_PTR` as a 64 bit `DLGPROC` must, `_beginthread()`'s
result is a `uintptr_t`, and the numbers that used to be dressed up as `char *`
for a `"%ld"` go through `emsgn()` or an `intptr_t` now.

```
$ ARCH=i686   ./scripts/build-mingw.sh warn 2>&1 | grep -c warning:   # 37
$ ARCH=x86_64 ./scripts/build-mingw.sh warn 2>&1 | grep -c warning:   # 37
$ ARCH=x86_64 ./scripts/build-mingw.sh warn 2>&1 \
        | grep -cE 'pointer-to-int-cast|int-to-pointer-cast'          # 0
```

Both are built in CI. What is left in either is 22 `%d` against a `long` or a
`DWORD` -- the same width on Windows -- and a dozen cosmetic ones.

**The 64 bit build has never been run.** It compiles and nothing truncates a
pointer any more, which is not the same thing as working; there is no Windows
runtime test here at all. The release stays 32 bit until somebody runs it.

## Which C runtime

msvcrt, not UCRT. `msvcrt.dll` is on every Windows, so the release zips need no
redistributable and import nothing but it and the OS DLLs, and the UTF-8 file
name support rides on it: msvcrt's `open()` and `stat()` go through the `...A`
entry points, which the manifest's `activeCodePage=UTF-8` redirects. UCRT would
buy better `printf` and locale behaviour and cost the Windows 7 story.

Which one a mingw-w64 targets is decided when the toolchain itself is built --
the headers define `_UCRT` or they do not, and `libmingwex` is compiled to match
-- so **no compiler flag here can change it.** The only lever is which toolchain
is on `PATH`, and the two obvious ways to install one do not agree: Debian's
`gcc-mingw-w64-*-win32` packages are msvcrt, Homebrew's `mingw-w64` is UCRT.

This is not a detail that stays in the printf corners. msvcrt's `tmpnam()`
returns a name under `P_tmpdir`, which is `"\\"`, so the file it names is in the
root of the current drive and cannot be written by an ordinary process; UCRT's
consults `TEMP` and returns a real path. `:r !cmd` redirects the command's
output into that file, so it worked in every local build made with a Homebrew
toolchain and failed in every release, through two releases, until the sources
stopped using `tmpnam()` at all.

So `scripts/build-mingw.sh` prints the runtime it found on every build, warns
when it is not msvcrt, and refuses `release` outright -- including when it
cannot ask the compiler at all. To pick a toolchain without reordering `PATH`:

```sh
CROSS=/usr/bin/i686-w64-mingw32- ./scripts/build-mingw.sh both
```

There is no Windows runtime test in CI, so testing on Windows by hand is the
only Windows testing there is, and a build with the other runtime does not
count as it.

## What is and is not in this build

Included: `KANJI` `UCODE` (UTF-8 / UCS-2 file I/O) `FEPCTRL` (IME control)
`SYNTAX` `TRACK` `CRMARK` `FEXRC` `NT106KEY` `USE_GREP` `USE_TAGEX` `USE_OPT`
`USE_HISTORY` `WEBB_COMPLETE` `WEBB_KEYWORD_COMPL` `TERMCAP` `XARGS`, and the
full Win32 GUI (`winjnt.c`).

Left out, re-enablable from `src/makjnt.mak`: `grep.exe`, `clip.exe` and
`vim32s.exe` (`src/grep/`, `src/clip/`, `src/vim32s/`). The in-editor search
extensions (`USE_GREP`, in `search.c`) *are* included; only the standalone
`grep.exe` is not.

### Gone, not just switched off

Three features were removed outright, sources and all, because their terms make
the tree awkward to redistribute — a packager for a ports tree or a distro has
to ship the source, whether or not the build compiles it. `doc.j/readme.doc`
§12 states them: `src/bdf/` "requires distribution under the GPL" with no
licence header in the files themselves, and for `src/exfile/` "please contact
Tsuchida if you use these sources", which is a request for permission and so
not a free licence at all. Everything else here is Vim 3.0's public domain plus
a Japanization whose author explicitly abandoned copyright.

| Feature | Was | Note |
| --- | --- | --- |
| BDF font rendering | `USE_BDF`, `src/bdf/` | A bitmap font cannot scale, and the point of it was showing Japanese before outline fonts were everywhere. Any modern font covers more characters than a BDF ever did. |
| Editing inside LHA/ZIP/CAB/TAR archives and over ftp | `USE_EXFILE`, `src/exfile/` | Also needed UNLHA32.DLL and friends at run time. To come back one day as an implementation with a licence behind it. |
| MIME / uuencode / base64 decode (`gu`) | `USE_MATOME`, `src/exfile/matome.c` | Part of the same directory. |

The menu items and settings that drove them are gone too, along with `Global >
Unicode Font`, which had stopped doing anything: it used to choose between the
ANSI and the Unicode drawing API, and the drawing path has been unconditionally
Unicode since the UTF-8 rewrite.

## Changes the mingw build needed

| Where | Why |
| --- | --- |
| `src/vim.h` | `<windows.h>` is now included before `ascii.h`. mingw-w64's `winnt.h` declares struct bit-fields named `CR` and friends, which are otherwise replaced by the `ascii.h` macros and break every file. Also `<sys\stat.h>` → `<sys/stat.h>`. |
| `src/winjnt.c` | Dropped the `__try/__except` around `main()` (see below), `char *` / `char_u *` mix in `LoadConfig()`, `SetClassLong(GCL_HICON)` → `SetClassLongPtr(GCLP_HICON)`. |
| `src/vim32.rc` | The dialog named `BITMAP` is now `BITMAPSEL`; `BITMAP` is a `windres` keyword. `winjnt.c` follows. |
| `src/unix.c` | Removed the local `extern int errno;` and `sys_errlist[]` (unbuildable against current libcs). Affects the Unix build only. |

## Crash reports

`WinMain()` used to run the editor as

```c
__try { main(argc, argv); }
__except (EXCEPTION_CONTINUE_EXECUTION) { ; }
```

`EXCEPTION_CONTINUE_EXECUTION` resumes at the *faulting instruction*, so an
access violation was swallowed and the editor kept running on corrupted state.
That is gone. `src/w32crash.c` installs an unhandled-exception filter that
records the fault and then lets the process die.

On an abnormal exit you get, in `%LOCALAPPDATA%\jvim3\` (override with
`%JVIM_CRASHDIR%`):

- `jvim-crash-<pid>-<time>.log` — exception code and address, registers, stack
  with module+offset, and the editor's own state (file, cursor line, mode)
- `jvim-crash-<pid>-<time>.dmp` — minidump, if `dbghelp.dll` is present

Turn the log into source locations:

```sh
./scripts/resolve-crash.sh /path/to/jvim-crash-1234-20260819-184000.log
```

```
stack:
  #00  jvim32w.exe 0x0040dc70
        edit  at  src/edit.c:46
  #01  jvim32w.exe 0x0041fd50
        ml_get_buf  at  src/memline.c:949
```

Addresses are logged twice, as loaded and relocated back to the link-time base
(`static=`), so ASLR does not get in the way. Keep the unstripped exe (or the
`.exe.debug` from `make split`) that produced the report.

Environment variables:

- `JVIM_CRASHDIR` — where reports go
- `JVIM_CRASH_QUIET` — suppress the message box / stderr notice
- `VIM32DEBUG` — do not install the filter at all, so a debugger or Windows
  Error Reporting gets the exception instead

## UTF-8 is the internal representation

JVim used to hold buffer text in Shift-JIS: a character was always two bytes,
and everything outside CP932 was lost on the way in. The buffer now holds UTF-8,
so hangul, accented Latin and characters outside the BMP survive being edited
and saved. `src/utf8.c` has the primitives; the classification the rest of the
editor relies on is in `src/utf8.h`.

Conversion happens only at the edges:

| Edge | Where |
| --- | --- |
| Files | `kanjiconvsfrom()` / `kanjiconvsto()` in `kanji.c`. UTF-8 and UCS-2 are handled directly; EUC-JP, Shift-JIS and ISO-2022-JP pivot through Shift-JIS, which is what the existing conversion tables are built around. |
| Terminal | `flushbuf()` in `term.c` converts to `jmask`'s display code. |
| Win32 GUI | `PrintChar()` in `winjnt.c` builds UTF-16 from the screen's code point plane and draws with `ExtTextOutW`. |
| Keyboard and IME | The GUI window is a Unicode one, so `WM_CHAR` carries UTF-16; `winjnt.c` joins surrogate pairs and pushes UTF-8. `edit.c` and `cmdline.c` read a whole character, however many bytes it is. |
| Clipboard | `CF_UNICODETEXT` both ways (`clip_put()` / `clip_get()` in `winjnt.c`), falling back to `CF_TEXT` when that is all a program offers. |
| Window title | `SetWindowTextW`, so a file name outside CP932 shows as itself. |
| Pipes | `jmask`'s system code, still CP932 on Windows: that is what a command run from `:!` writes. |
| File names | UTF-8 on Windows, see below. |

The GUI window has to be Unicode for this to work: an ANSI window only ever
receives `WM_CHAR` in the ANSI code page, so anything outside CP932 arrived as
`?` — an emoji, being a surrogate pair, arrived as two of them. That means
`RegisterClassW`, `CreateWindowW`, `DefWindowProcW` and the `W` forms of the
message loop functions, because `GetMessageA` would translate the character back
to the ANSI code page on its way out of the queue. In the GUI the key code is
therefore UTF-8 whatever `jmask` says; that setting still describes the console.

Two invariants make the change tractable:

- **`ISkanjiPosition()` keeps its meaning.** It returns 0 for a single byte
  character, 1 for the first byte of a multi-byte one, and 2 for a trailing
  byte, which reads the same in UTF-8 as it did in Shift-JIS. What is gone is
  the assumption that a multi-byte character is two bytes: use `utf_lenat()` for
  the length, `utf_width()` for the columns, `utf_head()`/`utf_prev()` to step.
- **A trailing byte is zero columns wide.** `charsize()` and `chartabsize()` take
  a pointer and report the width of the whole character on its first byte and 0
  on the rest, so the loops that walk bytes and add up widths still get the
  right answer.

The screen keeps one byte per cell for the redraw comparison and adds a parallel
plane of code points (`ScreenCP` in `screen.c`), because a character can be one
to four bytes but only ever one or two cells. `SCRCP_CONT` marks the right half
of a double width character.

### File names outside CP932

`src/jvim.manifest` asks for UTF-8 as the process ANSI code page
(`activeCodePage`, Windows 10 1903 and later). Every `...A` API then takes
UTF-8, so `open()`, `stat()`, `FindFirstFileA()` and the rest accept a file name
with characters CP932 has no room for — an emoji, say. File names reach them
through `fileconvsto()` / `fileconvsfrom()`, which use `FILECODE` (UTF-8 on
Windows) rather than the system code.

The system code stays CP932, because it is also the encoding of what a command
run from `:!` writes back, and those still speak CP932. The manifest leaves out
comctl32 v6 (it would theme the dialogs) and `supportedOS` (it would change what
`GetVersionEx()` reports, which parts of `winjnt.c` still branch on).

### Display scaling

The manifest declares the process DPI aware (`dpiAwareness` = PerMonitorV2, with
the older `dpiAware` = `true/pm` for systems that predate it). A process that
does not is drawn at 96 DPI and stretched to the display scale, which resamples
glyphs that were already rendered: at 125% or 150% the text goes soft and
uneven, and so does the menu bar.

Being aware means the font has to be asked for in the pixels the display really
has, and the pixel sizes in the registry (`font`, `jfont`, `width`, `height`)
only mean the same thing on screen at the DPI they were stored at. So the key
also holds `dpi`, and `dpi_scale_to()` in `winjnt.c` restates those sizes for
the DPI in front of it — at startup, when the window turns out to be on a
monitor at another scale, and on `WM_DPICHANGED` when it is dragged to one. The
character grid comes out of that unchanged: the font and the window that holds
`Rows` by `Columns` of it are scaled together, so it is a resize, not a reflow.
Settings from a JVim before this have no `dpi` value, and 96 is the right
reading of them — that is what Windows was virtualising the DPI to.

`linespace`/`charspace` stay in raw pixels on purpose: they are nudges of a
pixel or two typed into a dialog that offers 0 to 10, and scaling them would
make the dialog disagree with itself.

The dialog font in `vim32.rc` had to go the same way. It was Terminal, a raster
font with a strike at a few fixed sizes, so at 125% GDI stretched the nearest
bitmap and the dialog text came out ragged even though everything around it was
now sharp. It is `MS Gothic` at 12 point instead. Fixed pitch was the thing to
keep: a caption of n characters is n × `tmAveCharWidth` wide and a box of 4n
dialog units is exactly the same, at any size, and several of these layouts are
cut that fine — `LTEXT "ASCII"` in 20 units is five characters and not a pixel
more — so a proportional font would mean relaying out all twelve dialogs. 12
point holds the 8 pixel cell the layouts were drawn against; MS Gothic is square
where Terminal 8x12 was tall, so the dialogs come out about a third taller.

To go back to the stretched-but-larger rendering, the exe's Properties >
Compatibility > Change high DPI settings can override the manifest per user.

### Drawing a row of text

Getting an emoji on the screen intact took unpicking four separate faults, three
of them of long standing. What the drawing does now, and why:

**The background of a row goes down before any of its glyphs.**
`PaintWindow()` fills the row's damaged span, in spans of one colour, and only
then walks the runs drawing text. `PrintChar()` paints no background at all.

It used to be one `ExtTextOut` per run with `ETO_OPAQUE` and the background mode
left `OPAQUE`, which paints a cell at a time: `ExtTextOut` fills each glyph's own
advance box immediately before drawing that glyph. A glyph whose ink is wider
than its advance had the overhang painted out by the box of the glyph after it,
in the same call, so redrawing reproduced it exactly rather than repairing it.
Filling once per run was not enough either — the run loop starts a new run
wherever the attribute changes *or* the text crosses between ASCII and a
multi-byte character, so a line of Japanese is chopped into a good many runs and
the blanks after the last word are a run of their own. That is why the last
character of a line lost its right side while a line ending in a full stop,
whose glyph sits well inside its cell, looked perfectly fine.

**A character outside the BMP is drawn by itself.** `lpDx` is documented as one
entry per character of the string, but GDI applies it per *glyph*, and a
surrogate pair is two characters that become one glyph. The entry meant for the
character after the pair was eaten by the pair itself: that character advanced
by the zero belonging to the low surrogate and landed on its neighbour, and
every entry after it was off by one place. Mostly they hold the same number, so
it very nearly looked right — the line simply came out a cell short with its
tail half a character left of the grid. So an astral character gets its own
`ExtTextOut` at a known x with no `lpDx` to misread, and the run picks up again
where the grid says the next cell starts.

**East Asian Ambiguous is two columns.** `utf_ambig[]` in `utf8.c` holds the A
class of `EastAsianWidth.txt`, which was drawn up from the legacy CJK charsets
and so is very nearly the set CP932 encodes as double byte — the set a Japanese
font draws double width. `→` is the everyday case: U+2192 is CP932 0x81A8, and
MS Gothic and Myrica both give it a full width glyph, so calling it one column
put it on top of whatever followed. This is what `'ambiwidth'` set to double
means elsewhere. Left out, though the A class has them, is everything below
U+2000 that is a letter rather than a symbol — Latin-1, Latin Extended, IPA, the
spacing modifiers — because those come out of the Latin half of a mixed font at
half width. Greek and Cyrillic are in: a Japanese font takes those from its CJK
half, CP932 having them in JIS X 0208 rows 6 and 7.

Widening the search meant lowering the fast path in `utf_cpwidth()` that
returned 1 for anything under U+1100, and that cut had been standing in front of
the zero width test: every combining mark below it — the Latin diacriticals,
Hebrew points, Arabic harakat, the Thai marks — was given a column of its own,
which is not what `utf_iszerowidth()` lists them for. The cut is at U+0300 now,
so they get none.

**A text write damages the whole row.** In `mch_write()`, `prefix` counts bytes
and `v_col` counts columns, and since the screen holds UTF-8 the two part
company the moment anything is not ASCII: three bytes for the two columns of a
kana, four for the two of an emoji. The code treats them as the same thing, so
`v_col` runs ahead of where the text really is.

Reckoning it properly turns out to be the wrong repair. The damaged rectangle
came out too wide for the same reason, and too wide is harmless — it is what
kept the display honest while `v_col` drifted. Made exact, it stopped covering
for the drift and lines began to end early. So the rectangle is the row: nothing
is left unpainted, ink that leans out of its cells is inside the clip region
wherever it lands, and `BeginPaint()` is spared the arithmetic. `clreol` needs
its own patch on top, since it blanks to the end of the line straight to the
screen and redraws no text; it asks for the character before the cut back
afterwards, never before, or the `UpdateWindow()` in front of the blanking
services the request and the blanking wipes it again.

Two limits remain. GDI draws no colour emoji at all — `COLR`/`CBDT` layers need
DirectWrite — so what appears is the fallback font's monochrome outline. And an
emoji presentation sequence gets the width of its base character: `⚠️` is U+26A0,
which East Asian Width calls Neutral, followed by a variation selector that
`utf_iszerowidth()` rightly gives no cell of its own, so it is allotted one
column where a font draws two and leans on its neighbour. Fixing that means a
cell that can hold a sequence rather than a single code point, which the screen
planes in `screen.c` do not do yet.

### The GUI's own strings

The manifest settles what the `...A` APIs mean, but the GUI does not lean on it
for its own text: dialogs, menus, fonts and the registry all speak UTF-16
natively, so `winjnt.c` converts UTF-8 to and from wide explicitly
(`SetDlgItemTextU8()`, `GetDlgItemTextU8()`, `AppendMenuU8()`,
`CreateFontIndirectU8()`, `ChooseFontU8()`, `RegGetStringU8()` /
`RegSetStringU8()`), and JVim's dialogs are created with `DialogBoxParamW`.
Otherwise a font name or a history entry depended on the code page twice over,
which is what left them unreadable.

Two related fixes while in there: the font chooser passes `CF_FIXEDPITCHONLY`,
since JVim draws on a character grid and a proportional font cannot work, and
`CF_ANSIONLY` is gone, because it hid every font whose charset is not the ANSI
one — most of the Japanese ones.

One migration note: the `LOGFONT` goes into the registry as a binary blob, so a
face name written by an older build is in CP932. `face_normalise()` converts it
to UTF-8 on the way in, once. A `vim32.ini` written by an older build is a
different matter: Windows reads a BOM-less ini through the process code page, so
its non-ASCII values are misread; re-saving the settings from JVim fixes it.

### jmask has a fourth character

`jmask` is now "key display system **file**". The fourth character is the code a
brand new file is written in. It used to share the system code, which is wrong
now: on Windows the system code has to stay CP932 for the ANSI file APIs and the
IME, while a new file should be UTF-8. The default is `SSST` on Windows and
`EEET` elsewhere, so **new files are UTF-8**. A three character `jmask` (or
`-K SSS`) still works and means "write new files in the system code".

### What is lossy, and what is not

- UTF-8 and UCS-2 files round trip byte for byte, including characters outside
  CP932, characters outside the BMP, and a UTF-8 BOM.
- Saving to EUC-JP, Shift-JIS or ISO-2022-JP can only keep what those encodings
  have; anything else becomes `?`. That is inherent, not a bug.
- Reading a UCS-2 file still pivots through CP932, so a UCS-2 file with
  characters outside CP932 loses them on the way in. Converting it to UTF-8
  first avoids that. (UCS-2 *writing* is direct and lossless.)
- A combining mark takes zero columns and is therefore invisible; the bytes are
  kept and saved.
- `jkanaconv` now defaults to off whichever system code is in use. It rewrites
  halfwidth kana to fullwidth when a file is read, which was a reasonable thing
  to do when a halfwidth kana was one Shift-JIS byte; silently changing the
  user's text is not what a lossless editor should do. `:set jkc` brings it
  back.

### Encoding detection

`judge_jcode()` in `kanji.c` picks the encoding of a file it is given. Two fixes
were needed:

- If the whole buffer parses as UTF-8 and holds at least one multi-byte
  character, it is UTF-8. Shift-JIS and EUC never pass that test on real text:
  their trailing bytes fall outside the UTF-8 continuation range. Without this,
  Japanese UTF-8 was regularly taken for Shift-JIS, because most of its two byte
  prefixes also look like valid Shift-JIS.
- A four byte sequence (a character outside the BMP) now counts as evidence.
  It used to be skipped without counting, so a single emoji tipped a whole file
  over to Shift-JIS and mangled it.

## Bugs found along the way

These were in JVim 2.1b already; they are the kind of thing that makes an editor
feel unstable.

| Where | What |
| --- | --- |
| `judge_jcode()` | Read `ptr[-2]` on a file that starts with a newline. Missing `i >= 2` guards on the UCODE branches. |
| `getcmdline()` | Inserting a multi-byte character at the start of the command line read and wrote `buff[-1]`. |
| `sjis2ucs()` (`s2u.c`) | Indexes its table on the two Shift-JIS bytes with no range check, so an invalid pair reads far out of bounds. Callers validate now. |
| 7 places | `strcpy(p, p + 1)` and friends: the ranges overlap, which is undefined. Replaced with the new `STRMOVE()`. |
| `msg_outtrans()` / `msg_outstr()` | Message and command line output emitted exactly two bytes per multi-byte character, so a three or four byte one had its tail bytes printed separately as `[XX]`. That is what made a file name with an emoji or halfwidth kana unreadable on the `:` line and in completion. |
| `mstrjpchr()` | A regexp character class compared only the first two bytes of its members, so `[あ]` also matched `い` (both start e3 81), and consumed two bytes of a three byte character. Classes and ranges compare code points now, and a range no longer needs both ends to be the same width. |
| `check_abbr()` | Counted a multi-byte character as two bytes when working out how much to erase for an abbreviation. |
| `kanjiconvsfrom()` | Terminal input arrives in chunks of at most `MAXMAPLEN` (50) bytes, so a character can be split across two of them. The UTF-8 path decoded before checking for that, so every byte of a split character became `?` — pasting a line with a character across the boundary produced text like `気難し???指示役`. It checks the length first now and carries the split bytes over in `tail`, which is what that argument is for. |
| `winjnt.c` | `_beginthread()` was called without a declaration. |

The whole test suite and a set of stress runs over real 113-145 KB files in
EUC-JP, Shift-JIS, ISO-2022-JP and UTF-8 pass under AddressSanitizer with no
findings.

## Encoding tests

`scripts/test-encoding.sh` drives a Unix build through a pty and compares bytes.
It covers four things: round tripping files in EUC-JP, Shift-JIS and UTF-8;
editing over multi-byte characters (`x`, `dw`, `cw`, `r`, `J`, yank and put,
visual mode, undo, insert); the screen column arithmetic, via `N|`; input read in
chunks, by typing at the terminal rather than through `-s`, since a script file is
read straight by `vgetorpeek()` and never crosses an `inchar()` boundary; and
regexp character classes, where two characters sharing their first bytes have to
stay apart.

```sh
cd src && cp makjunix.mak makefile   # uncomment your MACHINE/CC/LIBS lines
make jvim3
cd .. && ./scripts/test-encoding.sh
```

All 42 cases pass. Run it against an AddressSanitizer build to check for memory
errors at the same time:

```sh
cd src && cp makjunix.mak makefile
# in the makefile: CC=gcc -O0 -g -std=gnu89 -w -fcommon -fsanitize=address
#                  LIBS = -fsanitize=address
make jvim3
cd .. && ASAN_OPTIONS=detect_leaks=0 ./scripts/test-encoding.sh
```
