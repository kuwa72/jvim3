# Building JVim 3 on Unix

```sh
./scripts/build-unix.sh          # build src/jvim3
./scripts/build-unix.sh test     # build, then run the encoding tests
./scripts/build-unix.sh clean
```

`src/makjunix.mak` still expects three lines to be uncommented by hand for your
machine. The script works out the same answers by asking the compiler and hands
them to that makefile on the command line, so the makefile itself stays as it
was. Override anything it decides through the environment:

```sh
CC=clang OPT="-O0 -g" EXTRA_CFLAGS=-I/usr/local/include \
	EXTRA_LIBS=-L/usr/local/lib ./scripts/build-unix.sh
```

It is POSIX `sh` and avoids `make -C`, so it works with the BSDs' `/bin/sh` and
`bmake` as well as with bash and GNU make.

## Two flags the build now needs

- **`-std=gnu89`** (or `gnu17`). The sources use K&R function definitions
  throughout, which C23 removed. A compiler that defaults to C23 — gcc 15 and
  later — fails on every file with "number of arguments doesn't match
  prototype". The script probes for a dialect that still accepts them.
- **`-fcommon`**. Several globals are defined in more than one file, which
  became an error in gcc 10.

## What the script detects

| | |
| --- | --- |
| `-DUSE_LOCALE` | `setlocale()` exists. This is what makes `LANG` pick the kanji codes: `ja_JP.UTF-8` gives `jmask=TTTT`, `ja_JP.eucJP` gives `EEEE`. Without it the default is `EEET` — EUC display, UTF-8 for new files. |
| `-DTERMCAP` + a library | The first of `-ltinfo -lncursesw -lncurses -lcurses -ltermlib -ltermcap` that provides `tgetent()`, if `<termcap.h>` or `<curses.h>`+`<term.h>` is also present. Otherwise it falls back to `-DALL_BUILTIN_TCAPS`, JVim's own compiled-in terminal entries, and links nothing. |
| `-DUSE_X11` | `<X11/Xlib.h>` and `-lX11` are both usable; only used for saving and restoring the xterm title. |
| machine | `-DBSD_UNIX` for Linux, the BSDs and macOS; `-DSYSV_UNIX` variants for SunOS and AIX. |

## Portability fixes this needed

| Where | What |
| --- | --- |
| `unix.c` | The local `extern int errno;` and `sys_errlist[]` do not build against a current libc: `errno` is thread local now and `sys_errlist[]` is gone. Uses `strerror()`. |
| `main.c`, `message.c`, `getchar.c` | Five `fprintf(stderr, (char *)s)` calls with a non-literal format. Debian, Ubuntu and Fedora build with `-Werror=format-security` by default, which refuses them, so the build failed before reaching the link. |
| `term.c`, `termlib.c` | `outchar()` was `void(unsigned)` but was handed to `tputs()`, which declares its third argument `int (*)(int)`. Calling through a mismatched function pointer is undefined; it happened to work. `outchar()` is `int(int)` now and JVim's own `tputs()` agrees. |
| `termlib.c` | Six functions relied on the implicit `int` return type, which C23 also removed. |
| 7 places | `strcpy(p, p + 1)` and friends: the ranges overlap, which is undefined. `STRMOVE()` now. |

## What has been verified, and what has not

Verified here, on Ubuntu 24.04 / gcc 13.3, x86-64:

- `-DTERMCAP` with ncurses, and the `-DALL_BUILTIN_TCAPS` fallback
- `-DUSE_LOCALE`, and `jmask` following `LANG`
- `-DUSE_X11`, `-DFEPCTRL` with `fepseq.o`
- `-std=gnu89`, `gnu99`, `gnu17`; `-O0` through `-O2`
- Distribution hardening: `-D_FORTIFY_SOURCE=2 -Werror=format-security
  -fstack-protector-strong`
- `/bin/sh` being dash
- All 42 encoding and editing tests, and the same again under AddressSanitizer
- 64 bit: no `-Wpointer-to-int-cast` anywhere in the portable sources. The three
  `-Wint-to-pointer-cast` left are `long` values passed to `emsg2()` for a `%ld`,
  which keep their value on LP64. (The Windows build is a different story, see
  BUILDING-mingw.md.)

**Not** verified, because there is no BSD or macOS to hand:

- Actually running on FreeBSD, NetBSD, OpenBSD or macOS
- clang, which is what the BSDs use. Only gcc was available here.
- The `BSD4_4` branch in `unix.c` — `<termios.h>` with `TCGETA` mapped to
  `TIOCGETA` — which is FreeBSD-specific and cannot be compiled against Linux
  headers. It syntax-checks with those two constants supplied by hand, and it is
  untouched by this work.

If you can run it on a BSD, `./scripts/build-unix.sh test` is the whole check:
it prints what it detected, builds, and runs the suite. Worth an eye afterwards:
the terminal line of the output (which curses library it found), whether `LANG`
gives you the `jmask` you expect (`:set jm?`), and cursor movement over
double width characters.
