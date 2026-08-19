# Building JVim 3 for Windows with mingw-w64

The original Windows build (`src/makjnt.mak`) needs the MS SDK and `nmake`.
This adds a mingw-w64 build of a minimal configuration: core editor, Win32 GUI,
and kanji/UTF-8 file I/O.

## Build

Cross compile from Linux/WSL:

```sh
sudo apt install mingw-w64          # or: brew install mingw-w64
./scripts/build-mingw.sh both       # jvim32w.exe (GUI) + jvim32.exe (console)
```

Or natively, in the MSYS2 **MINGW32** shell:

```sh
pacman -S mingw-w64-i686-gcc make
./scripts/build-mingw.sh both
```

Output goes to `dist/i686/`, together with `jvim3.hlp` and a sample `_jvimrc`.
Copy that directory to the Windows side and point `%VIM%` at it.

Other targets: `clean`, `warn` (compile with warnings shown), `split` (move the
debug info into `jvim32w.exe.debug` and strip the exe).

### 32 bit or 64 bit

**Use 32 bit.** It runs fine on Windows 11 x64 under WoW64. `ARCH=x86_64` does
compile, but the sources round-trip pointers through `int` in about 37 places,
which 64 bit truncates silently:

```
$ ARCH=i686   ./scripts/build-mingw.sh warn 2>&1 | grep -c warning:   # 49
$ ARCH=x86_64 ./scripts/build-mingw.sh warn 2>&1 | grep -c warning:   # 95
```

The extra 46 are 23 `-Wpointer-to-int-cast`, 14 `-Wint-to-pointer-cast` and 9
more `-Wincompatible-pointer-types`. Fixing those is a separate job; `warn`
lists them.

## What is and is not in this build

Included: `KANJI` `UCODE` (UTF-8 / UCS-2 file I/O) `FEPCTRL` (IME control)
`SYNTAX` `TRACK` `CRMARK` `FEXRC` `NT106KEY` `USE_GREP` `USE_TAGEX` `USE_OPT`
`USE_HISTORY` `WEBB_COMPLETE` `WEBB_KEYWORD_COMPL` `TERMCAP` `XARGS`, and the
full Win32 GUI (`winjnt.c`).

Left out, all re-enablable from `src/makjnt.mak`:

| Feature | Flag / files |
| --- | --- |
| Editing inside LHA/ZIP/CAB/TAR archives and over ftp | `USE_EXFILE`, `src/exfile/` (needs UNLHA32.DLL etc.) |
| MIME / uuencode / base64 decode (the `gu` command) | `USE_MATOME`, `src/exfile/matome.c` |
| BDF font rendering (GPL) | `USE_BDF`, `src/bdf/` |
| `grep.exe`, `clip.exe`, `vim32s.exe` | `src/grep/`, `src/clip/`, `src/vim32s/` |

The in-editor search extensions (`USE_GREP`, in `search.c`) *are* included; only
the standalone `grep.exe` is not.

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
| Pipes and file names | `jmask`'s system code, still CP932 on Windows. |

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
| `winjnt.c` | `_beginthread()` was called without a declaration. |

The whole test suite and a set of stress runs over real 113-145 KB files in
EUC-JP, Shift-JIS, ISO-2022-JP and UTF-8 pass under AddressSanitizer with no
findings.

## Encoding tests

`scripts/test-encoding.sh` drives a Unix build through a pty and compares bytes.
It covers three things: round tripping files in EUC-JP, Shift-JIS and UTF-8;
editing over multi-byte characters (`x`, `dw`, `cw`, `r`, `J`, yank and put,
visual mode, undo, insert); and the screen column arithmetic, via `N|`.

```sh
cd src && cp makjunix.mak makefile   # uncomment your MACHINE/CC/LIBS lines
make jvim3
cd .. && ./scripts/test-encoding.sh
```

All 34 cases pass. Run it against an AddressSanitizer build to check for memory
errors at the same time:

```sh
cd src && cp makjunix.mak makefile
# in the makefile: CC=gcc -O0 -g -std=gnu89 -w -fcommon -fsanitize=address
#                  LIBS = -fsanitize=address
make jvim3
cd .. && ASAN_OPTIONS=detect_leaks=0 ./scripts/test-encoding.sh
```
