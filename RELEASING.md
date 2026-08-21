# Releasing

One command does the checking; you do the tagging.

```sh
./scripts/release.sh 1.0.1            # check everything, show the notes, stop
./scripts/release.sh 1.0.1 --push     # ... and then tag and push it
```

Pushing the tag is what starts a release: CI builds and tests on all five
systems, cross builds both Windows architectures, and only then publishes.
A broken build cannot become a release.

## The steps

1. Add what changed to the `## Unreleased` section of
   [CHANGELOG.md](CHANGELOG.md), in English and 日本語. `scripts/release.sh`
   refuses a release whose section is missing, empty, or has no 日本語 block.
   `./scripts/changelog-draft.sh` prints a starting point from the commits since
   the last tag, with build- and docs-only commits commented out.
2. Rename that section from `## Unreleased` to `## <version> — <date>`, and put
   a fresh empty `## Unreleased` above it.
3. Edit [VERSION](VERSION) to the new number, and commit both. The commit
   message for this one is conventionally `Release <version>`.
4. `./scripts/release.sh <version>` and read what it prints.
5. `./scripts/release.sh <version> --push`.

## Which number to move

The version is this tree's own, and has nothing to do with the `3.0-j2.1b` it
descends from — that is a fixed point in 2002, not a version of this.

| | |
| --- | --- |
| **Patch** (1.0.**1**) | A fix somebody using the last release would notice. |
| **Minor** (1.**1**.0) | A new capability or a newly supported platform: another encoding, another BSD in CI, the 64 bit Windows build going from "compiles" to "known to run". |
| **Major** (**2**.0.0) | Something that breaks a user's files, their `_jvimrc`, or their stored settings. |

## What does not get a release

CI changes, test work, refactoring, documentation. Those sit on `master` and
accumulate under `## Unreleased`. If `changelog-draft.sh` comes back with every
candidate line commented out as build or docs only, there is nothing to release
yet.

Nine releases went out in three days once, because cutting one was the only way
to get a testable Windows zip to a person. It is not any more:
`scripts/fetch-ci-build.sh` downloads the package CI built for any commit. A
release is for people who are not watching the repository.

## What the version number reaches

`VERSION` is the only place the number is written. From there it reaches:

- the binary — `:version`, `jvim3 -h`, and the crash report, via
  `-DJVIM_VERSION` from `scripts/build-unix.sh` and `src/makefile.mingw`;
- the Windows file properties — `FILEVERSION` and `ProductVersion` in
  `src/vim32.rc`, which `windres` fills in from the same three numbers;
- the package names — `jvim3-<version>-win32.zip`;
- the git tag, which is `v` + `VERSION`, checked by CI before anything is
  published.

`src/jvim.manifest` deliberately keeps a fixed version; the comment in it says
why. Untagged builds add the commit, so a snapshot identifies itself as
`1.0.0 (a1b2c3d4)` and its package as `jvim3-1.0.0-a1b2c3d4-win32.zip`.

## One thing to know about the numbering

Releases up to 2026-08-21 were tagged `v3.0-j2.1b-utf8.1` … `.9`. From 1.0.0 the
tree has its own semantic version. **1.0.0 is newer than
`v3.0-j2.1b-utf8.9`, not older.**

That means git's version sort is wrong about these tags for good:
`git tag --sort=-v:refname | head -1` compares the leading `3` against `1` and
will keep answering `v3.0-j2.1b-utf8.9`. Nothing in this repository may find the
latest release that way. Use `git describe`, `--sort=-creatordate`, or GitHub's
`/releases/latest` — which is what the README badge does, and why it uses
`github/v/release` rather than `github/v/tag`.
