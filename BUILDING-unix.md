# Building JVim 3 on Unix

日本語の手順は [BUILDING.ja.md](BUILDING.ja.md) にあります。

```sh
./scripts/build-unix.sh          # build src/jvim3
./scripts/build-unix.sh test     # build, then run the three test suites
./scripts/build-unix.sh strict   # build with the warnings CI refuses
./scripts/build-unix.sh install  # build and install to PREFIX (default: /usr/local)
./scripts/build-unix.sh clean
```

### Deploying to local test environment

For local testing without root privileges:

```sh
tools/deploy-local.sh              # builds and installs to ~/.local
tools/deploy-local.sh /path/to/dir # deploys to custom prefix
```

`strict` is the "warnings that have to stay away" CI job, runnable before the
push instead of after it. It is worth the minute: gcc's default only warns about
an initialiser landing in the wrong field of a struct, and clang, which is what
FreeBSD builds with, stops.

CI runs it twice, on Linux with gcc and on FreeBSD with clang, because green on
gcc does not mean green on clang. It went in on gcc alone at first, and clang
had two things to say the moment it was asked: an old-style function definition
is refused outright rather than warned about, and a cast to the wrong pointer
type is an error. Both were in `unix.c` and `term.c` where nothing had looked.

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

`test` runs all four suites: `scripts/test-encoding.sh` (48 cases — kanji,
UTF-8, multi-byte editing, file names), `scripts/test-editing.sh` (72 cases —
motions, operators, registers, marks, undo, ex ranges, `:g`, `:s`, searching,
the `:!` filter and wildcard expansion), `scripts/test-syntax.sh` (50 cases
— what the rules in `syntax/` actually colour, read back with `:syntax dump`,
one for every file in `syntax/`) and `scripts/test-sgr.sh` (9 cases — the
escapes the terminal is actually sent, which is the only check of the painter
rather than the rules). 179 cases in all.

They need bash and a C compiler: they build `scripts/ptyrun.c` to give jvim a
terminal. That used to be `script(1)`, which is a different program on Linux,
NetBSD and the other BSDs, and NetBSD's quits before the command has run when
its own standard input is a file. `ptyrun` also gives each case 20 seconds
(`PTYRUN_TIMEOUT`) before it kills it, so a case that leaves the editor waiting
for a key fails instead of hanging the suite.

## What CI covers

Every push and pull request builds and runs both suites on **Linux**,
**FreeBSD**, **NetBSD**, **OpenBSD** and **DragonFly**, and cross
builds the Windows executables with mingw-w64, 32 and 64 bit. A tag matching
`v*` does the same and then publishes the Windows build to the release page, so
a broken build cannot become a release. See
[.github/workflows/build.yml](.github/workflows/build.yml).

The BSDs run in a VM on the Linux runner, from a prebuilt guest that boots in a
couple of minutes. `scripts/test-bsd-docker.sh` below does the same thing on
your own machine, from the systems' own images, and lets you get a shell in the
guest to look around.

## Checking the BSDs from a Linux box

```sh
./scripts/test-bsd-docker.sh              # FreeBSD: build and run the tests
./scripts/test-bsd-docker.sh freebsd shell   # leave the guest up to poke at
./scripts/test-bsd-docker.sh freebsd clean   # throw the kept guest disk away
```

**FreeBSD is the guest to keep here.** CI already runs the whole thing on
FreeBSD, NetBSD, OpenBSD and DragonFly, so the coverage is its job, and a
second local guest is a second guest disk for a system CI checks anyway. What a
local guest buys is not the clock — a run here takes about as long as CI's —
but not having to push to find out: it builds the tree as it is, uncommitted
changes and all, and `shell` leaves you in the guest with the failure still
there. FreeBSD is where that pays: it builds with **clang**, which stops where
gcc only warns, and it is the BSD whose differences have actually cost
something. NetBSD is still installable, `./scripts/test-bsd-docker.sh netbsd`,
for a NetBSD-only failure CI has found; it is not part of a routine check.

Docker cannot run a BSD container — a BSD binary needs a BSD kernel — so the
container is only somewhere to keep QEMU, and the BSD inside it is a real
virtual machine booted from the project's own image. It needs `/dev/kvm`,
1.5 GB of disk to keep the guest in, 25 GB free while it builds one, and the
network on the first run of each system.

That first run installs the guest far enough to be reachable over ssh, and then
makes what it keeps small: the installer's caches go, the free space is written
over with zeros so that the blocks deleted files left behind stop being worth
storing, and the release image is folded into the guest disk and compressed
with zstd. One file is left — 1.3 GB for FreeBSD, against 7.9 GB for the image
and the overlay it used to be — and the download is deleted. Later runs overlay
that file and are up in twenty seconds. A guest kept by an earlier version of
this script is the two files; `./scripts/test-bsd-docker.sh freebsd compact`
does the same to it, without reinstalling.

## The one flag the build still needs

**`-fcommon`**. Several globals are defined in more than one file, which became
an error in gcc 10.

`-std=gnu89` used to be needed as well, because the sources were full of K&R
function definitions and C23 removed them. All 775 of them are prototypes now,
so the compiler's own default is used and gcc 15 and later, which default to
C23, build this as they are.

Turning the prototypes on had not been done before either: `proto.h` and every
`proto/*.pro` file declare their functions through `__PARMS()`, which was only
defined for Aztec, SAS, DICE, Turbo C and Borland — everything else fell through
to a `()` fallback in `vim.h`, so the declarations were empty parameter lists
and were never checked against a definition. They are now.

## Warnings

`-Wall` leaves 251 warnings, of which 236 are `-Wpointer-sign`: JVim keeps its
text in `char_u` (`unsigned char`) and hands it to the C library and to its own
`char *` interfaces all over the place. Those are type noise, not bugs, and
changing 236 sites by hand would be a big diff over code the tests only
partly reach, so they stay.

The classes that do break things are errors in CI instead: an implicit
declaration (which truncates a returned pointer on a 64 bit machine -- that is
exactly how `tgoto()` broke every test on OpenBSD), a mismatched pointer type,
an implicit `int`, a missing prototype, a missing return, a use of an
uninitialised variable. The remaining 15 warnings are five ambiguous `else`
branches inside `#ifdef KANJI`, three `-Wint-to-pointer-cast` in `buffer.c`
described above, and a handful of unused variables and ignored return values.

## What the script detects

| | |
| --- | --- |
| `-DUSE_LOCALE` | `setlocale()` exists. This is what makes `LANG` pick the kanji codes: `ja_JP.UTF-8` gives `jmask=TTTT`, `ja_JP.eucJP` gives `EEEE`. Without it the default is `EEET` — EUC display, UTF-8 for new files. |
| `-DTERMCAP` + a library | The first of `-ltinfo -lncursesw -lncurses -lcurses -ltermlib -ltermcap` that provides `tgetent()`, if `<termcap.h>` or `<curses.h>`+`<term.h>` is also present. Otherwise it falls back to `-DALL_BUILTIN_TCAPS`, JVim's own compiled-in terminal entries, and links nothing. |
| `-DUSE_X11` | `<X11/Xlib.h>` and `-lX11` are both usable; only used for saving and restoring the xterm title. |
| `-DHAVE_MKSTEMP` | `mkstemp()` links. The temp files for `:!` and for wildcard expansion are made with it; without it they fall back to `mktemp()`, which only picks a name. |
| machine | `-DBSD_UNIX` for Linux, the BSDs and macOS, plus `-DBSD4_4` on the BSDs and macOS so that `unix.c` takes its `<termios.h>` path rather than `<sgtty.h>`; `-DSYSV_UNIX` variants for SunOS and AIX. |

## Portability fixes this needed

| Where | What |
| --- | --- |
| `unix.c` | The local `extern int errno;` and `sys_errlist[]` do not build against a current libc: `errno` is thread local now and `sys_errlist[]` is gone. Uses `strerror()`. |
| `main.c`, `message.c`, `getchar.c` | Five `fprintf(stderr, (char *)s)` calls with a non-literal format. Debian, Ubuntu and Fedora build with `-Werror=format-security` by default, which refuses them, so the build failed before reaching the link. |
| `term.c`, `termlib.c` | `outchar()` was `void(unsigned)` but was handed to `tputs()`, which declares its third argument `int (*)(int)`. Calling through a mismatched function pointer is undefined; it happened to work. `outchar()` is `int(int)` now and JVim's own `tputs()` agrees. |
| `termlib.c` | Six functions relied on the implicit `int` return type, which C23 also removed. |
| 9 places | `strcpy(p, p + 1)` and friends: the ranges overlap, which is undefined. `STRMOVE()` now. The last two were the `\"` and `\%` unescaping in `DoOneCmd()`, which nothing reached often enough to notice until the rule files in `syntax/` — full of `\"` — were sourced at every startup, and AddressSanitizer stopped the editor before it drew a screen. |
| `unix.h`, `unix.c` | Declarations for libcs that predate C89 — `bcopy()`, `bzero()`, `ioctl()`, `fork()`, `execvp()`, and `memmove()`/`memset()` mapped onto the b* functions — were skipped through a chain of `!defined()` per block, and macOS was in none of them. On macOS these are errors, not warnings, because its own `bcopy()` and `execvp()` have different prototypes; the build stopped in `alloc.c`. `MODERN_LIBC` in `unix.h` names those systems once. |
| `cmdcmds.c`, `unix.c`, `misccmds.c` | `mktemp()` for the `:!` filter and wildcard expansion only picks a name, leaving a gap for somebody to drop a symlink into; the linkers on the BSDs warn about every use. `vim_mktemp()` uses `mkstemp()` where the build finds it. `getwd()` in three places was handed no length at all and wrote up to `PATH_MAX` into the caller's buffer; `getcwd()` now. |
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
- All 179 tests, and the same again under AddressSanitizer
  (`OPT="-O1 -g -fsanitize=address" EXTRA_LIBS=-fsanitize=address`)
- `./scripts/build-unix.sh strict`, the `-Werror=` set CI refuses to build without
- 64 bit: no `-Wpointer-to-int-cast` anywhere in the portable sources. The three
  `-Wint-to-pointer-cast` left are `long` values passed to `emsg2()` for a `%ld`,
  which keep their value on LP64. (The Windows build is a different story, see
  BUILDING-mingw.md.)

Verified on FreeBSD 14.3-RELEASE-p16, clang 19.1.7, amd64, in the QEMU guest
`scripts/test-bsd-docker.sh` builds:

- All 179 tests
- `./scripts/build-unix.sh strict` with clang 19, which is what the FreeBSD CI
  job now runs after the tests. It took two fixes to get there, both of them
  things gcc had only warned about; see the CHANGELOG for the day.
- `-DTERMCAP` against base ncurses, found as `-ltinfo`
- `jmask` following `LANG`: `ja_JP.UTF-8` gives `TTTT`, `ja_JP.eucJP` gives
  `EEEE`, `C` gives `EEET`
- Both tty paths: `-DBSD4_4` (`<termios.h>`, what the script picks now) and the
  `<sgtty.h>` branch it used to take. Each passes the whole suite. The `BSD4_4`
  branch had never been compiled before — it cannot be, against Linux headers.

Verified on NetBSD 10.1, gcc 10.5.0, amd64, the same way:

- All 179 tests
- `-DTERMCAP` against base curses, found as `-lcurses`
- No warnings at all for the whole build. There were 58, every one of them the
  second argument of `tgetstr()`, which NetBSD's curses declares `char **` while
  `term.c` cast it to `char *`. That looked like 58 casts nobody wanted to write
  over a declaration that differs between systems; it was one wrong cast in one
  macro, and fixing that took all 58 with it.

Verified in CI, on whatever release the VM images carry — FreeBSD 15.1,
NetBSD 11.0 and OpenBSD 7.9 at the time of writing, plus DragonFly. The whole
suite on each, whatever it holds that day — no number here, because a count
written down in two places is a count that goes stale in one of them, and this
one did: it said 100 for months. OpenBSD had never been built at all before
that; `-lncursesw` is what it finds. DragonFly is CI only:
`scripts/test-bsd-docker.sh` has no guest for it, so it has never been looked at
interactively.

**Not** verified:

- macOS. It was in CI until 2026-08 and passed, but it is not a target anybody
  here can try by hand, and a failure there is a puzzle nobody is in a position
  to solve. One had turned up by then: an escape sequence arriving as the very
  first thing typed was not recognised, which no other system does. The
  `__APPLE__` paths in `unix.h` and `unix.c` stay, because the BSDs share them.
- Real hardware, a real terminal and a real IME. Everything above is a serial
  console, a pty or a CI runner.
- X11 title saving anywhere but Linux: no BSD guest and no CI runner has the X
  headers, so they all build with `USE_X11` off.

On a BSD you have to hand, `./scripts/build-unix.sh test` is the whole check:
it prints what it detected, builds, and runs the suite. Worth an eye afterwards:
the terminal line of the output (which curses library it found), whether `LANG`
gives you the `jmask` you expect (`:set jm?`), and cursor movement over
double width characters.
