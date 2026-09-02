# JVim 3 — Vi IMproved 3.0, Japanized, holding UTF-8

**English** | [日本語](README.ja.md)

JVim 3 is Tsuchida Ken'ichi's Japanized build of Bram Moolenaar's Vim 3.0. Its
last release, 3.0-j2.1b, is from December 2002. This tree is that editor brought
to current systems, with one substantial change inside it: **the buffer holds
UTF-8 instead of Shift-JIS**, so text that CP932 has no room for — hangul,
accented Latin, emoji, anything outside the BMP — survives being read, edited
and saved.

It is a 1994 editor: a single executable of about a megabyte, no plugins, no
scripting language, no startup delay, vi's own key map plus the handful of
things Vim 3.0 added — multi level undo, several windows, command line
history and completion, syntax colouring, IME control that knows about
command mode.

[![Latest release](https://img.shields.io/github/v/release/kuwa72/jvim3?label=latest%20release)](https://github.com/kuwa72/jvim3/releases/latest)
derived from JVim 3.0-j2.1b (2002 Dec 24)

```
Platforms        Windows 10/11 (Win32 GUI + console), Linux,
                 FreeBSD, NetBSD, OpenBSD, DragonFly
Tests            252 cases, run on all of the above in CI, plus 6 that run
                 the Windows executables there. The Windows keyboard has 16
                 more in scripts/test-winkeys.sh, typed on the real thing
                 from WSL
Licence          Public domain — see LICENSE, and uganda.txt for the
                 charity request that comes with it
```

## What this tree changed

| | |
| --- | --- |
| UTF-8 inside | Buffer text is UTF-8; conversion happens only at the edges (files, terminal, GUI, keyboard, clipboard, pipes). A character is one to four bytes and one or two columns wide, rather than always two of each. |
| Unicode Win32 GUI | A `RegisterClassW` window, `ExtTextOutW` drawing, `CF_UNICODETEXT` clipboard, `SetWindowTextW` title, UTF-16 `WM_CHAR` joined into UTF-8. Dialogs, menus, font names and the registry all speak UTF-16 explicitly. |
| File names outside CP932 | The manifest asks for UTF-8 as the process code page, so the `...A` file APIs take UTF-8 and a file called `🍣.txt` opens. |
| Display scaling | The process is per-monitor DPI aware, and the stored font and window sizes are restated for the DPI in front of them, so text is sharp at 125% and 150% and stays that way when the window is dragged between monitors. |
| Builds anywhere Unix-ish | `scripts/build-unix.sh` asks the compiler what the machine has instead of asking you to uncomment three lines in a makefile. `scripts/build-mingw.sh` cross builds the Windows executables with mingw-w64. |
| 252 tests | 51 encoding cases, 86 editing cases, 88 syntax colouring cases, 9 that read the escapes the terminal is actually sent, and 18 that hand it input nobody intended, or a hostile end — 2 MB on one line, every byte value there is, a multi-byte sequence cut in half by the end of the file, the session dropping mid-edit. All driven through a real pty. Every push runs them on five operating systems, and again under AddressSanitizer and UndefinedBehaviorSanitizer. |
| Long standing bugs fixed | Fifteen of them, listed in [BUILDING-mingw.md](BUILDING-mingw.md#bugs-found-along-the-way) — `[あ]` in a regexp also matching `い`, a command line reading `buff[-1]`, encoding detection tipping a whole file over to Shift-JIS because of one emoji, terminal input mangling a character split across two reads. |
| Colour schemes | `:colorscheme` and a Vim-compatible `:highlight`, eleven bundled themes, on the GUI and over a terminal's SGR alike. [USAGE.md](USAGE.md#colour-schemes) has the reference. |
| Two features removed | BDF font rendering and editing inside LHA/ZIP/TAR archives are gone, sources and all, because their terms made the tree awkward to redistribute. See [below](#licence). |

## Get it

### Windows

Download a zip from the [releases page](https://github.com/kuwa72/jvim3/releases)
and unpack it anywhere. There is nothing to install: with `%VIM%` unset the
editor takes its own directory as `%VIM%`, so the help file and a `_vimrc` beside
the exe are found as they are.

| Package | Holds | |
| --- | --- | --- |
| `jvim3-*-win32.zip` | `jvim32w.exe`, `jvim32.exe` | 32 bit; runs on 64 bit Windows under WoW64. |
| `jvim3-*-win64.zip` | `jvim64w.exe`, `jvim64.exe` | Native 64 bit, for a machine you know is 64 bit. |

`jvim32w.exe` is the GUI. `jvim32.exe` is the same editor from a console window;
give it `-nw` to stay in the console instead of opening a window.

Each package also has `vim.hlp`, JVim's Japanese help file, which `:help` finds
where it is, the syntax rules in `syntax/`, and two rcs to start from:
`jvimrc.sample`, which is short and works on a Unix build too, and
`_jvimrc.sample`, the long-standing Windows one.
[USAGE.md](USAGE.md#first-run-on-windows) has the ten minutes of setup worth
doing.

### Linux, FreeBSD, NetBSD, OpenBSD, DragonFly

```sh
git clone https://github.com/kuwa72/jvim3
cd jvim3
./scripts/build-unix.sh test        # build src/jvim3, then run the 252 tests
```

You need a C compiler and, for the real terminal database rather than the
entries compiled in, a curses or termcap library (`libncurses-dev` on Debian and
Ubuntu; already there on the BSDs). The script prints what it found:

```
configuring for Linux 6.18.33, cc
  dialect                (compiler default)
  tentative globals      -fcommon
  machine                -DBSD_UNIX
  setlocale              yes
  mkstemp                yes
  terminal               -DTERMCAP -DSOME_BUILTIN_TCAPS -ltinfo
  X11 title              yes
```

macOS builds too, but it is not verified and not in CI. See the unverified
list in [BUILDING-unix.md](BUILDING-unix.md).

To install it by hand:

```sh
sudo install -m 755 src/jvim3           /usr/local/bin/
sudo install -m 644 doc.j/vim.hlp       /usr/local/lib/jvim3.hlp   # or doc/vim.hlp for English
sudo install -m 644 doc/vim.1           /usr/local/man/man1/jvim3.1
```

Those are the paths the build compiles in. There is no package in any
distribution yet; if you make one, say so and it will be linked here.

### Windows, built yourself

```sh
sudo apt install gcc-mingw-w64-i686-win32   # cross build from Linux or WSL
./scripts/build-mingw.sh both               # dist/i686/jvim32w.exe + jvim32.exe
```

or `pacman -S mingw-w64-i686-gcc make` in the MSYS2 **MINGW32** shell, and the
same script. That package and not the `mingw-w64` metapackage: the build has to
be against msvcrt, and the script refuses a toolchain that is not.
[BUILDING-mingw.md](BUILDING-mingw.md) has the rest.

## Documentation

This tree's own docs:

| | |
| --- | --- |
| [USAGE.md](USAGE.md) / [USAGE.ja.md](USAGE.ja.md) | Running it, where settings live, the encoding model, IME, display, troubleshooting. **Start here.** |
| [BUILDING-unix.md](BUILDING-unix.md) | Building on Linux and the BSDs; what the script detects; what CI covers; what is verified and what is not. |
| [BUILDING-mingw.md](BUILDING-mingw.md) | Building for Windows; and the long version of how UTF-8, the Unicode GUI, DPI awareness and the drawing of a row of text actually work. |
| [BUILDING.ja.md](BUILDING.ja.md) | ビルド手順の日本語版 (both platforms). |

Vim 3.0's own manuals, from 1994, in `doc/`:

| | |
| --- | --- |
| [doc/reference.doc](doc/reference.doc) | Every command and option. The reference. |
| [doc/difference.doc](doc/difference.doc) | What Vim adds to vi, and the few places it differs. |
| [doc/windows.doc](doc/windows.doc) | Multiple windows and buffers. |
| [doc/index](doc/index) | Alphabetical list of commands. |
| [tutor/tutor](tutor/tutor) | An hour's training course for someone new to vi. |
| [doc/vim.hlp](doc/vim.hlp) | The `:help` file, English. |
| [README](README) | Vim 3.0's own README, from 1994, kept as it was. Its build instructions are the ones this tree replaced. |

JVim's own manuals, in Japanese, in `doc.j/` (UTF-8):

| | |
| --- | --- |
| [doc.j/readme.doc](doc.j/readme.doc) | JVim 3.0-j2.1b の説明書。増えているオプション、コマンド、syntax の設定、tips。 |
| [doc.j/differen.doc](doc.j/differen.doc) | difference.doc の日本語版。 |
| [doc.j/fepctrl.doc](doc.j/fepctrl.doc) | FEP/IME 制御について。 |
| [doc.j/vim.hlp](doc.j/vim.hlp) | `:help` の日本語版。 |

Anything in `doc.j/` describing MS-DOS, Windows 95, BOW, archive editing or BDF
fonts is history now, not instructions.

## Where it stands

Nothing types at the Windows builds in CI: both are run there now, but only
through script input, which cannot reach a cursor key — the suite that types
for real needs a Windows machine and is run by hand. Console mode Japanese
input on Windows is unreliable, GDI draws no colour emoji, and a couple of
encoding conversions are one-way.
[USAGE.md](USAGE.md#known-limits) has the full list, with what each one means
for actual use.

## Tests

```sh
./scripts/build-unix.sh test           # build and run all five suites
./scripts/test-encoding.sh src/jvim3   # 51 cases: encodings, multi-byte editing
./scripts/test-editing.sh  src/jvim3   # 86 cases: motions, operators, registers,
                                       #   marks, undo, ex ranges, :g, :s, :!
./scripts/test-syntax.sh   src/jvim3   # 88 cases: what syntax/ actually colours
./scripts/test-sgr.sh      src/jvim3   # 9 cases: the escapes a terminal is sent
./scripts/test-hostile.sh  src/jvim3   # 18 cases: input nobody intended
```

They all drive the editor through a real pty (`scripts/ptyrun.c`) and compare
bytes, so they exercise the same input path a person does. Each case is given 20
seconds before it is killed, so a case that leaves the editor waiting for a key
fails rather than hanging the suite — which is also how the hostile-input suite
notices that something has become too slow to finish at all.

Every push and pull request builds and runs all 230 on Linux, FreeBSD, NetBSD,
OpenBSD and DragonFly, and cross builds both Windows architectures. A
tag matching `v*` does the same and then publishes the Windows zips, so a broken
build cannot become a release. See
[.github/workflows/build.yml](.github/workflows/build.yml).

## Contributing

Issues and pull requests: <https://github.com/kuwa72/jvim3>. Japanese is as
welcome as English.

Two things make a change easy to take: `./scripts/build-unix.sh test` passes,
and the build stays clean of the warnings CI treats as errors.

Especially useful: running the 64 bit Windows build, or using any of this with a
real IME on real hardware, and saying what broke.

[CONTRIBUTING.md](CONTRIBUTING.md) ([日本語](CONTRIBUTING.ja.md)) has the rest —
the warning list, how to add a test case, commit style, and why there is no
private security channel.

## Licence

Vim 3.0 is **public domain**, and Bram Moolenaar asked that if you like it you
send money not to him but to the Kibaale Children's Centre in Uganda —
"charityware". [uganda.txt](uganda.txt) is his own account of it, and
[doc.j/uganda.jp](doc.j/uganda.jp) is the Japanese translation. The request
stands; Vim's [own page for it](https://www.vim.org/iccf/) is where to look for
the current way to give.

Tsuchida Ken'ichi **abandoned copyright** in the Japanization
(`doc.j/readme.doc` §12), asking only that changed sources be published in turn,
and disclaiming all warranty. Both of those apply here as well: this comes with
no warranty of any kind.

Two directories that were *not* under those terms are gone from this tree —
`src/bdf/` (required GPL distribution, with no licence header in the files) and
`src/exfile/` (needed the author's permission, which is not a free licence).
What is left is Vim 3.0's public domain plus a Japanization whose author gave up
his claim. They are gone from the working tree, not from the git history.

[LICENSE](LICENSE) is the machine-readable form of all this — the Unlicense,
which is the closest standard text to what Moolenaar and Tsuchida each actually
said, and one GitHub can recognise. The charity request above is a request, not
a condition of use.

## Credits

Vim 3.0 by **Bram Moolenaar**, from Stevie by Tim Thompson, Tony Andrews and
G. R. Walter; [credits.txt](credits.txt) has the full list.

The Japanization is **Tsuchida Ken'ichi**'s (土田健一), built on
**Ogasawara Hiroyuki**'s (小笠原博之) patches for Vim 3.0 and
**Nakamura Atsushi**'s (中村敦司) for Vim 2.0, with the help of the Onew mailing
list, and **Ohta Jun**'s (太田純) FEPCTRL library for IME control.

The UTF-8 work, the Unicode GUI and the build and test scripts in this tree are
by [kuwa72](https://github.com/kuwa72).
