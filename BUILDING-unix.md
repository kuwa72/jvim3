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

The test suite needs bash and a C compiler: it builds `scripts/ptyrun.c` to give
jvim a terminal. It used to call `script(1)` for that, which is a different
program on Linux, NetBSD and the other BSDs, and NetBSD's quits before the
command has run when its own standard input is a file.

## Checking the BSDs from a Linux box

```sh
./scripts/test-bsd-docker.sh              # FreeBSD: build and run the tests
./scripts/test-bsd-docker.sh netbsd
./scripts/test-bsd-docker.sh all
```

Docker cannot run a BSD container — a BSD binary needs a BSD kernel — so the
container is only somewhere to keep QEMU, and the BSD inside it is a real
virtual machine booted from the project's own image. It needs `/dev/kvm`, about
12 GB of disk and, on the first run of each system, the network. That first run
installs the guest far enough to be reachable over ssh and keeps the disk, so
later runs are up in under a minute.

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
| `unix.c` | `sig_winch()` was declared and defined as the 4.3BSD three argument signal handler, `(int, int, struct sigcontext *)`. NetBSD no longer declares `struct sigcontext` in `<signal.h>`, so the prototype and the definition named two different types and the build stopped. Nothing in the handler looks at its arguments, so the modern BSDs and macOS take the plain `(int)` form. |

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

Verified on FreeBSD 14.3-RELEASE-p16, clang 19.1.7, amd64, in the QEMU guest
`scripts/test-bsd-docker.sh` builds:

- All 42 tests
- `-DTERMCAP` against base ncurses, found as `-ltinfo`
- `jmask` following `LANG`: `ja_JP.UTF-8` gives `TTTT`, `ja_JP.eucJP` gives
  `EEEE`, `C` gives `EEET`
- The `BSD4_4` branch in `unix.c` — `<termios.h>` with `TCGETA` mapped to
  `TIOCGETA` — which could not be compiled against Linux headers before.
  `EXTRA_CFLAGS=-DBSD4_4 ./scripts/build-unix.sh` builds and passes all 42
  tests. Without it FreeBSD takes the `<sgtty.h>` branch, which also works.

Verified on NetBSD 10.1, gcc 10.5.0, amd64, the same way:

- All 42 tests
- `-DTERMCAP` against base curses, found as `-lcurses`
- The link warns that `getwd()` and `mktemp()` are used unsafely. Both are old
  interfaces JVim still uses; neither is new here.

**Not** verified:

- OpenBSD, DragonFly and macOS. The `sig_winch()` and `BSD4_4` conditions above
  name them, on the assumption that what NetBSD and FreeBSD need they need too,
  but nobody has run it there.
- Real hardware, a real terminal and a real IME. Everything above is a serial
  console and a pty.
- X11 title saving on a BSD: neither guest has the X headers, so both build with
  `USE_X11` off.

On a BSD you have to hand, `./scripts/build-unix.sh test` is the whole check:
it prints what it detected, builds, and runs the suite. Worth an eye afterwards:
the terminal line of the output (which curses library it found), whether `LANG`
gives you the `jmask` you expect (`:set jm?`), and cursor movement over
double width characters.
