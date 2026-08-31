# Contributing to JVim 3

**English** | [日本語](CONTRIBUTING.ja.md)

Issues and pull requests: <https://github.com/kuwa72/jvim3>. Japanese is as
welcome as English, in issues, pull requests and commit messages.

## What makes a change easy to take

Three things:

1. `./scripts/build-unix.sh test` passes — 202 cases, and they run in about a
   minute.
2. `./scripts/build-unix.sh strict` passes. That is the same `-Werror=` set CI
   uses — an implicit declaration, a mismatched pointer type, a missing
   prototype, a missing return, an implicit `int`, an uninitialised variable —
   run before the push rather than after it. Several of those are only warnings
   under gcc and stop clang, which is what the FreeBSD job builds with.
3. `./scripts/build-unix.sh asan` and `./scripts/build-unix.sh ubsan` pass. They
   run the same cases against a build that stops on the errors a test cannot
   see: a read one byte past a buffer changes nothing about what the editor
   writes, so every case passes and the bug ships. Both are CI jobs, and both
   collect their reports from `log_path` rather than from stderr, which the
   suites discard.

`-Wpointer-sign` warnings are expected and stay — the sources mix `char` and
`unsigned char` deliberately (`char_u`), and there are 236 of them.
[BUILDING-unix.md](BUILDING-unix.md#warnings) says why.

CI runs the whole thing on Linux, FreeBSD, NetBSD, OpenBSD and DragonFly, and
cross builds both Windows architectures, on every push and every pull request.
A pull request from a fork gets a read-only token, so it builds and tests but
cannot publish anything.

## What is most wanted

- **Running the 64 bit Windows build.** CI only compiles it; there is no
  automated runtime test for either Windows architecture. Say what happens.
- **Using any of this with a real IME on real hardware.** CI is runners, ptys
  and serial consoles. Nothing here has been tried at length with a real IME.
- **A package for a distribution.** There is none anywhere yet. If you make one,
  say so and it will be linked from the README.

You do not need a mingw toolchain to test the exact binary a release ships:
`scripts/fetch-ci-build.sh` downloads the package CI built for any commit. If it
crashes, `scripts/resolve-crash.sh` turns the report in
`%LOCALAPPDATA%\jvim3\report.log` back into source lines.

## Where things are

| | |
| --- | --- |
| [BUILDING-unix.md](BUILDING-unix.md) | Building on Linux and the BSDs; what the script detects; what CI covers; what is verified and what is not. |
| [BUILDING-mingw.md](BUILDING-mingw.md) | Building for Windows, and the long version of how UTF-8, the Unicode GUI, DPI awareness and the drawing of a row of text work. |
| [USAGE.md](USAGE.md) | Running it, settings, the encoding model, IME, troubleshooting. |

## Tests

Add a case to whichever suite fits — `scripts/test-encoding.sh` for encodings
and multi-byte editing, `scripts/test-editing.sh` for motions, operators,
registers, marks, undo and ex commands. Both drive the editor through a real pty
(`scripts/ptyrun.c`) and compare bytes, so they exercise the same input path a
person does.

To land a case for a bug you cannot fix yet, mark it `knownfail` instead of
`ok`. The suite reports it as a known failure rather than a failure, and tells
you if it starts passing.

## Commit messages

An English or Japanese sentence saying what the change does, in the imperative,
with no `fix:` or `feat:` prefix — see `git log`. Around 50 characters. Put a
reason worth knowing in the body.

There is no CLA and no sign-off. This is public domain (see
[LICENSE](LICENSE)); by sending a patch you put it in the public domain too.

## Branches and releases

`master` is expected to build and pass on all five systems. Work that needs CI
on several platforms before it is fit for `master` goes on a topic branch named
for the workstream, and comes back by rebase rather than a merge commit.

Releases are cut from a tag, by CI, only after the build and tests pass
everywhere — so a broken build cannot become a release. They are for changes
that matter to someone who is not watching the repository; CI, test and
documentation work sits on `master` until there is something worth releasing.
[RELEASING.md](RELEASING.md) is the procedure.

## Security

There is no private disclosure channel. This is a local text editor with no
network features, maintained by one person. Please open a normal public
issue.
