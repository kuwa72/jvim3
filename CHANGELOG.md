# Changelog

JVim 3's own version numbers start at 1.0.0. "JVim 3.0-j2.1b" is where this tree
came from, not a version of it: that release is Tsuchida Ken'ichi's, from
December 2002, and it does not change again.

English first in each section, 日本語 after it. One file rather than two, because
a `CHANGELOG.ja.md` beside this would be the highest-churn document pair in the
repository and would drift within three releases.

## Unreleased

### Added

- `scripts/build-unix.sh asan` and `scripts/build-unix.sh ubsan` build with
  AddressSanitizer or UndefinedBehaviorSanitizer and then run the same suites
  the `test` target does, and CI runs both on every push. ASan had only ever
  been used through a line of environment variables typed from memory, and
  UBSan had never been run at all. Both targets point the sanitizer at
  `log_path` and collect the reports afterwards: every suite sends the editor's
  stderr to `/dev/null`, because what they compare is the bytes it writes, so a
  report would otherwise vanish with it — and UBSan carries on after a finding
  and exits 0, which would have made the job green while it was finding things.

### Fixed

- Two pieces of undefined behaviour in `src/memline.c`, found by the first UBSan
  run and reported 244 times over one pass of the test suites, with all 202
  cases passing throughout. `DB_MARKED` shifted a plain `int` into its own sign
  bit (`1 << 31`) on every line fetched for the screen and every line appended;
  it is `(unsigned)1` now, as it is in Vim's own memline.c. And `ml_add_stack()`
  called `memmove()` with a NULL source and a length of zero the first time a
  buffer's stack grew — permitted-looking, undefined in fact, and enough licence
  for a compiler to drop a NULL check elsewhere.

### 日本語

- `scripts/build-unix.sh asan` と `scripts/build-unix.sh ubsan` を追加しました。
  AddressSanitizer / UndefinedBehaviorSanitizer つきでビルドし、`test` と同じ
  スイートを実行します。CI でも push ごとに両方走ります。ASan はこれまで
  記憶を頼りに環境変数を並べて実行するだけのもので、UBSan は一度も走らせて
  いませんでした。どちらもサニタイザの出力を `log_path` でファイルに落として
  後から集めます。各スイートはエディタが書いたバイト列を比較するもので
  stderr は `/dev/null` に捨てているため、そのままではレポートも一緒に消える
  うえ、UBSan は検出しても実行を続けて 0 で終わるので、ジョブが緑のまま何かを
  見つけ続けることになります。
- `src/memline.c` の未定義動作 2 件を修正しました。初回の UBSan 実行で、
  テスト 1 周につき 244 回報告されたものです（その間 202 ケースはすべて
  通っていました）。`DB_MARKED` が `int` を符号ビットまでシフトして
  (`1 << 31`) おり、画面に表示する行の取得と行の追加のたびに踏んでいました。
  Vim 本家の memline.c と同じく `(unsigned)1` にしました。もう 1 件は
  `ml_add_stack()` で、バッファのスタックを最初に伸ばすときに `memmove()` を
  NULL・長さ 0 で呼んでいたものです。問題なさそうに見えて未定義であり、
  コンパイラが別の場所の NULL チェックを削る根拠になります。

## 1.2.0 — 2026-08-28

### Added

- `hi Normal guifg=... guibg=...` in a colour scheme now overrides the Text
  Color / Back Color configured in the Windows GUI/console for as long as the
  scheme keeps setting it, instead of being accepted and silently dropped.
  `hi clear`, or loading a scheme that never sets `Normal`, puts the
  configured colours back — the configured Text/Back Color itself is never
  touched, only which one is drawn with. All eleven bundled schemes now set
  `Normal` to their own base foreground/background, so switching to one of
  them (`dracula`, `nord`, ...) no longer leaves body text painted in
  whatever Text/Back Color happened to be configured while embedded
  markdown-code/HTML regions used the scheme's own (differently intended)
  background — the mismatch that made those regions read as low-contrast.
  Still nothing to override on a plain terminal, where there was never a
  configured base colour to begin with.
- `syntax/` colours the file types tracked in #5, plus a further batch asked
  for directly: OCaml (`.ml`, `.mli`), Standard ML (`.sml`, `.sig`, `.fun`), D
  (`.d`), Scheme (`.scm`, `.ss`), Common Lisp (`.lisp`, `.lsp`, `.cl`),
  Clojure (`.clj`, `.cljs`, `.cljc`), Kotlin (`.kt`, `.kts`), Perl (`.pl`,
  `.pm`, `.t`), Swift (`.swift`), TeX/LaTeX (`.tex`, `.sty`, `.cls`), Haskell
  (`.hs`), Protocol Buffers (`.proto`), Terraform (`.tf`, `.tfvars`), CMake
  (`.cmake`, and the name `CMakeLists.txt`), Vim script (`.vim`), awk
  (`.awk`), and assembler (`.s`, `.S`, `.asm`) each get their own
  `syntax/<name>.jvsyn`. `.pro` — the third most common file type in this
  repository, at 41 files — joins the suffix list `c.jvsyn` already had,
  rather than getting a rule file of its own: they are C prototype headers.
  `.jsonc`/`.jsonl`/`.json5` point at `json.jvsyn`; `.cnf`/`.conf`/`.cfg`/
  `.properties`/`.editorconfig` and the bare names `.gitconfig`/
  `.gitmodules` point at `ini.jvsyn`; `.bashrc`/`.zshrc`/`.profile`/
  `.bash_profile`, the `.env` family, and `.gitignore`/`.dockerignore`/
  `.npmignore` point at `sh.jvsyn`. `scripts/test-syntax.sh` gained a case for
  most of the new files, checked against the real binary's `:syntax dump`
  rather than by eye.

### 日本語

- 配色スキーム中の `hi Normal guifg=... guibg=...` が、Windows の GUI・
  コンソールで設定されている Text Color・Back Color を、そのスキームが
  指定し続けている間だけ上書きするようになりました。これまでは受け付けた
  ふりをして黙って捨てていました。`hi clear`、あるいは `Normal` を
  指定しないスキームに切り替えれば、設定した色に戻ります — 設定そのものが
  書き換えられることはなく、どちらを描画に使うかが変わるだけです。同梱の
  11 テーマすべてに、それぞれの地の前景色・背景色として `Normal` を追加
  しました。これにより `dracula` や `nord` などに切り替えても、地の文が
  設定済みの Text/Back Color のまま残る一方で埋め込みの markdown コード・
  HTML 領域だけがテーマ本来の（異なる意図の）背景色になる、という
  食い違い — 見た目上そこだけコントラストが低く見えていた原因 — が
  なくなります。プレーンな端末では、そもそも上書き対象となる設定色が
  無いため従来どおり何も起きません。
- `syntax/` が Issue #5 に挙がっていたファイル種別と、それに加えて直接
  依頼のあった一群に対応しました: OCaml (`.ml`、`.mli`)、Standard ML
  (`.sml`、`.sig`、`.fun`)、D (`.d`)、Scheme (`.scm`、`.ss`)、Common Lisp
  (`.lisp`、`.lsp`、`.cl`)、Clojure (`.clj`、`.cljs`、`.cljc`)、Kotlin
  (`.kt`、`.kts`)、Perl (`.pl`、`.pm`、`.t`)、Swift (`.swift`)、TeX/LaTeX
  (`.tex`、`.sty`、`.cls`)、Haskell (`.hs`)、Protocol Buffers (`.proto`)、
  Terraform (`.tf`、`.tfvars`)、CMake (`.cmake`、および名前 `CMakeLists.txt`)、
  Vim script (`.vim`)、awk (`.awk`)、アセンブラ (`.s`、`.S`、`.asm`) —
  それぞれに `syntax/<name>.jvsyn` を用意しました。`.pro`（このリポジトリで
  3 番目に多いファイル種別、41 ファイル）は独自のルールファイルではなく、
  `c.jvsyn` が既に持っていたサフィックス一覧に加わっただけです — C の
  プロトタイプヘッダだからです。`.jsonc`/`.jsonl`/`.json5` は `json.jvsyn`
  を、`.cnf`/`.conf`/`.cfg`/`.properties`/`.editorconfig` と裸の名前
  `.gitconfig`/`.gitmodules` は `ini.jvsyn` を、`.bashrc`/`.zshrc`/
  `.profile`/`.bash_profile`、`.env` 系、`.gitignore`/`.dockerignore`/
  `.npmignore` は `sh.jvsyn` を指すようにしました。`scripts/test-syntax.sh`
  に新しいファイルのほとんどに対応するケースを追加し、目視ではなく
  実際のバイナリの `:syntax dump` と突き合わせて確認しています。

## 1.1.0 — 2026-08-24

### Added

- A mapping can name the characters it could not hold: `<CR>` `<NL>` `<LF>`
  `<Esc>` `<Tab>` `<Space>` `<BS>` `<Nul>`, in either half, in any case.

      map q ihello<CR>
      map <Space> :w<CR>

  Pressing Enter from a mapping used to mean putting a real carriage return in
  the rc, which made the file's line separator part of what the file meant:
  `dosource()` takes one CR off the end of every line and cannot tell that one
  from a separator, so in a file with Unix endings the mapping quietly lost it.
  **The same rc could not be written for a Unix and for Windows.** A name has
  nothing at the end of a line to lose, so now it can.

  Only characters. `#[UP]`, `#[F01]` and `#1` already name the keys, and a
  second spelling for those would be two tables to keep in step for no new
  ability. A `<` that starts nothing in the list stays a `<`, so a mapping that
  types `<div>` still says so, and `CTRL-V` holds off one that would otherwise
  be read as a name — the expansion and the `CTRL-V` removal are one pass over
  each half, so neither can undo the other. The two halves are told apart
  first, or a `<Space>` among the keys would become the space that ends them.

  With this, the startup warning `Wrong line separator, ^M may be missing` has
  nothing left to warn about and is gone. An rc with Unix line endings now
  starts silently on Windows, which is what a dotfile shared with a Unix looks
  like. The trailing CR of a CRLF file is still taken off, so those still work.
- `:syntax dump <file>` writes what the rules did to the buffer, as text: one
  line per coloured run, with the group and the rule that made it. A rule that
  matches the wrong thing had no other way of saying so — the screen came out a
  colour short and finding the rule meant reading pixels. `scripts/test-syntax.sh`
  is that turned into a suite, so a rule that stops matching fails a test.
- A colour can say what goes behind it: `syntax link Error bolic white on
  maroon`, or the colour itself as `syntax link DiffAdd green on #e6ffe6`, or
  with nothing in front of the `on` to leave the text the colour it would have
  had. The sixteen named colours are all too strong to read a line of text off,
  so the rule files write the tints they want. `Error` and `Todo` are drawn
  this way now — both are groups vim gives a background, and without one they
  had been standing in with `bolic red` and `reverse`. Reverse is the terminal
  swapping two colours it already has, which is not blue on yellow and is not
  the same twice on two terminals. `diff`'s added and removed lines and
  Markdown's fenced blocks have one too.

  A cell's attribute is one byte holding a colour id, and 145 of its 256 values
  are already spoken for, so this is a third plane of the screen array rather
  than more bits. The background is a position in a table of the colours the
  rules asked for, not one of the foreground's letters: it never has to serve
  as a foreground, so it costs nothing to give it a space of its own.

  Both painters draw it — the terminal as `48;2;r;g;b` in the same escape as
  the foreground, or the nearest of sixteen plus forty; the Win32 GUI as the
  rectangle it already fills behind each run. The GUI one can only be checked
  by looking, so `scripts/test-winkeys.sh` takes the window as a bitmap and
  reads the pixels.
- `scripts/test-sgr.sh`, which reads the escapes the terminal is actually sent
  and the text under each of them. `:syntax dump` answers which rule coloured
  which bytes, a question about the rules; this is the other half — whether the
  colour reaches the terminal, and reaches only the cells it was meant for.
  Nothing else in the tree looks at the painter: the Windows suite drives the
  GUI and the dump never draws. It found a bug the first time it was run, in
  the entry below.
- Two encoding cases for a file name of three-byte characters — opening it by
  name and finding it by a wildcard. The Windows suite has had them since
  1.0.0; the part of them that is not about Windows was untested anywhere a CI
  runs, because the Windows suite needs Windows and never does.
- Syntax colouring works on a terminal, not only in the Win32 GUI. The colour a
  rule asks for goes out as an SGR escape — the exact one where `$COLORTERM`
  says the terminal can take it, the nearest of the sixteen otherwise. The
  palette is now in one place, so a terminal and the GUI cannot disagree about
  what "navy" is.
- The rules live in `syntax/`, one file per file type, instead of 1200 lines
  inside `_jvimrc`. An rc reaches all of them with one line, `source
  $VIM/syntax/filetype.jvsyn`, and `syntax/README` says how to add a type.
- `jvimrc.sample`, a short rc that works on both builds, in the Windows package
  and installed into `$VIM` on a Unix. Copy it to `~/.jvimrc` or
  `%HOME%\_jvimrc` — the "j" name, which JVim reads in preference to `.vimrc`
  and nothing else reads at all, so an ordinary vim on the same machine is not
  handed `set fexrc` and a syntax rule set. There was no rc at all that a Unix
  build could use: `doc.j/_jvimrc` stops at `set fepkeys`, which needs FEPCTRL.
- `$VIM` has a default on a Unix — `$PREFIX/lib/jvim3`, where `make install`
  puts the rule files. Windows already set it to the directory of the exe.
- Syntax colouring for Python, JavaScript/TypeScript, Go, Rust, Ruby, shell,
  Markdown, JSON, YAML, TOML, SQL, CSS/SCSS, C#, PHP, Lua, XML, diff, Makefile
  and Dockerfile, in `doc.j/_jvimrc` — which ships as `_jvimrc.sample`. Nothing
  newer than 1998 had rules before. The C rules now also cover `.cc`, `.cxx`,
  `.hpp`, `.hxx`, `.hh` and `.inl`.
- Every file in `syntax/` has a case in `scripts/test-syntax.sh`. It covered
  eighteen of them; the other twelve — HTML, XML, C#, batch, VBScript, plain
  text, Java, INI, `.def`, `.ec`, `.rc` and an rc itself — were shipped and
  unchecked.
- CSS colours the property name, INI the key, and Java its numbers and
  character constants. Each was the one obvious thing its rule file did not do.
- `scripts/build-unix.sh strict` builds with the `-Werror=` set CI refuses to
  build without, so it can be run before the push rather than after it. Several
  of those are warnings under gcc and stop clang, which is what FreeBSD uses.
- The Windows key suite asks whether the rules colour anything at all in the
  GUI build. Colouring is GUI-only on Windows (`SYN_ON()` is `'syntax'` *and*
  `GuiWin`) and the suite that reads colouring back needs a pty, so nothing
  checked that the engine runs there.
- The body of an HTML `<script>` or `<style>` is marked off as a block, on the
  same pale grey a fenced Markdown block gets, with the text left the colour it
  would have had. It used to be drawn as running text: `var` and `function` got
  nothing, and any word of a script that happened to sit between a `<` and a
  `>` got whatever an HTML attribute of that name gets. Colouring the
  JavaScript as JavaScript is not on offer — one rule set has no way to hand a
  range of the buffer to another — but the block now reads as a block. Both
  tags are inside the region, a region being coloured from where its opening
  pattern starts.
- `:colorscheme` (`:colo`) loads a named theme, and `:highlight` (`:hi`) reads
  Vim's own `guifg=` / `guibg=` / `gui=` / `link` / `clear` syntax rather than
  only `syntax link`'s. Eleven themes come bundled in `colors/` — dracula,
  nord, gruvbox, monokai, one-dark, desert, tokyonight, solarized-dark,
  solarized-light, and a default pair that `set background=dark|light`
  switches between. A name is looked for under `~/.jvim/colors/` first, then
  `$VIM/colors/`, so `:colorscheme mine` finds a file dropped in either. A
  real Vim colour scheme is Vimscript; this reads only the directives a
  scheme built the way the bundled ones are needs — `set`, `hi`,
  `let g:colors_name`, `finish`, `if`/`elseif`/`else`/`endif` — and does not
  evaluate an `if`'s condition, so every branch of one runs. See
  [USAGE.md](USAGE.md#colour-schemes).
- The default palette in `syntax/common.jvsyn` was rebuilt for contrast on a
  dark terminal background, replacing colours chosen in 2002 for a light GUI
  window.

### Changed

- The Windows key suite runs out of sight. It drives real editors with real
  windows, fifteen of them, and each used to appear on the screen and take the
  keyboard for a couple of seconds; the machine was unusable for the four
  minutes it takes. The drivers now put themselves on a desktop of their own
  first and give the console build a console with no window, neither of which
  `PostMessage` or `WriteConsoleInput` care about. A desktop alone was not
  enough: `CREATE_NEW_CONSOLE` is handed to whatever is set as the default
  terminal, and Windows Terminal opens it where the person is looking whatever
  desktop asked. `WINDESK_OFF=1` puts it back in sight.
- An unknown mode letter in a syntax rule is refused, with a message naming it,
  instead of being ignored. The 2002 manual says the rest are ignored and they
  were, so a typo in a mode went in as a rule and matched the wrong thing in
  silence. `n`, which is what every rule wanting no mode is written with, is
  now a mode of its own rather than one of the ignored letters.
- Syntax colouring remembers, per line, which multi-line region and which tag
  were open when the line above ended, instead of searching `synlines` lines in
  each direction every time a line is drawn. A comment or a string now keeps
  its colour however long it is, an unterminated one colours the rest of the
  file rather than nothing at all, and typing the token that opens or closes
  one recolours the lines below it straight away. A tag rule (`t`) reads the
  same state, so a tag written over more lines than the old window colours its
  name and its attributes like any other; `<` and `>` a hundred lines apart are
  no different from `<` and `>` on one line. `synlines` is still accepted, so
  an rc that sets it goes on working, but nothing reads it any more.

  It is not slower than the search it replaces: colouring a 1500 line file of
  nested tags takes 0.78–0.85s where the search took 0.88–1.03s, and the two
  produce byte for byte the same colouring on it. Nothing else pays anything —
  a buffer whose rules have no `t` in them never walks a delimiter, and a
  region's two ends are not mistaken for a tag's.
- `:syntax dump` names a tag rule (`t`) by the pattern it matches rather than
  the tag it looks inside. A dozen rules in `html.jvsyn` reported themselves as
  `t/<`, which told them apart from nothing.
- `scripts/test-bsd-docker.sh` runs the three suites in the guest, as CI does
  on the same systems. It ran the encoding suite alone, which is how
  BUILDING-unix.md came to say a guest had passed tests it never saw.
- The GUI key driver waits for the editor's CPU time to stop moving, then types
  one Escape, before a case starts. The window appears while the editor is
  still reading its rc and a key posted then is lost — not delayed — so a case
  read as though its second keystroke were its first. With no rc that window is
  narrow and a fixed sleep covered it nearly always, which is the worst way to
  be wrong: one run in a hundred failed and nothing explained it.
  `WaitForInputIdle` does not help (it reports idle before the rc is read) and
  `SendMessage` in place of `PostMessage` is worse (the editor stops taking
  keys at all).
- The BSD guest to keep on your own machine is FreeBSD's, and it is one file of
  1.3 GB. `scripts/test-bsd-docker.sh` had a NetBSD guest beside it and an
  `all` target that ran both, and the pair wanted about 12 GB of `~/.cache` for
  coverage CI already has — every push runs the suites on FreeBSD, NetBSD,
  OpenBSD and DragonFly. What a local guest is for is not the clock, since a
  run in it takes about as long as CI's, but finding out without pushing: it
  builds the tree as it stands and `shell` leaves you in the guest with the
  failure still there. FreeBSD is where that pays: it builds with clang, which
  stops where gcc only warns. `netbsd` still installs one for a NetBSD-only
  failure CI has found; `all` is gone.
- A kept guest is one compressed file rather than a release image and an
  overlay on it. Both halves had to stay — 3.6 GB and 4.3 GB for FreeBSD — and
  most of the overlay was the first boot patching itself: the patches it
  downloaded, its copy of every file it replaced, and blocks still holding
  files that had been deleted, since nothing rewrites a block on delete and a
  block that is not zero has to be stored. So the caches go, the free space is
  written over with zeros, and the two files are folded into one
  zstd-compressed qcow2: 7.9 GB becomes 1.3 GB, and the download is deleted
  rather than kept for a rebuild that would have to fetch a newer release
  anyway. The tests still run in an overlay on it, the guest is still up in
  twenty seconds, and all 179 pass on it. `freebsd compact` does the same to a
  guest kept by the older version of the script, without reinstalling it, and
  `clean` now clears the downloads too. The zeroing is why the script wants
  25 GB free while it builds or compacts a guest: the qcow2 grows to the whole
  virtual disk before it is compressed back down.
- The FreeBSD CI job runs the strict build after the tests, and the Linux job
  takes the flags from `build-unix.sh strict` rather than spelling them out a
  second time. FreeBSD is the only job here that compiles with clang, so it is
  the only one that can tell whether the set is green on clang — and it was not.
  Two lists of the same flags could have drifted apart as well; now there is
  one, and it is the one anybody can run before pushing.

### Fixed

- The Windows package carries Windows line separators. `dosource()` opens a
  sourced file in binary mode and takes one trailing CR off each line, warning
  `Wrong line separator, ^M may be missing` when there is none — and the
  package was built from a Unix checkout, so it warned three times before it
  had finished starting: once for the rc, once for `filetype.jvsyn`, once for
  the rules it pulls in. Three messages is enough to make the editor stop at
  `Press RETURN or enter command to continue`, so every start needed a keypress.

  The warning is not pedantry, though it is nearly always harmless. A CR at the
  end of a line is the separator and a CR anywhere else is content, so in an LF
  file a command that *ends* in a real CR — `map q ihello^M`, a mapping that
  presses Enter — cannot be told from an ordinary line, and the CR is eaten.
  Measured: the same rc written both ways gives a two-line buffer with CRLF and
  a one-line buffer with LF. Everything that does not end in a CR behaves
  identically, and the rule files colour byte-for-byte the same either way.

  Only a *trailing* CR is touched in the conversion. `doc.j/_jvimrc` line 37 is
  `"map n /^Mz.`, with a real CR in the middle of it, and a first attempt that
  filtered every CR turned it into `/z.` — which is exactly the breakage the
  warning is about, introduced while removing it. The build now checks its own
  output rather than trusting it, because nothing about this shows on a Unix.

  An rc of your own still has to be a CRLF file. Copying the shipped
  `_jvimrc.sample` gives you one.
- A rule file is coloured when you open one. `.jvsyn` had no file type at all,
  which is a poor advertisement for a syntax colouring engine: the thirty files
  the editor ships are the ones most likely to be edited by anyone changing the
  colours, and they came up plain. They are the same language as the `syntax`
  lines of an rc, so they read `jvimrc.jvsyn`, which already draws every group
  name in its own group and every colour name in its own colour — `Error` is red
  there because `Error` is red.

  That file had not been told about the notation added since, either: `white`
  was missing from the colour list, and so were `link`, `clear`, `load`,
  `color`, `dump`, `crchar`, the `on` that introduces a background, and
  `#rrggbb`. `white` is the one name that cannot be drawn in itself on the
  window's own background — it is drawn on grey, which a rule can ask for now.

  A quote on a line of its own is a comment. Only that one: a rule file writes
  `\"` inside its patterns constantly, and a rule that took any quote for the
  start of a comment would paint the pattern of every `String` rule as one. The
  `begin`/`end` block markers keep the quote in front of them so they still win
  the tie against it.
- The shipped `_jvimrc.sample` and `jvimrc.sample` are coloured. They are the
  file most people first see an rc in, and `.sample` is not one of the names an
  rc is read from, so nothing matched them.
- A rule can hold an alternation, and `syntax/README` said it could not. The ex
  command line eats one backslash, so `\|` reaches the regexp as a plain pipe
  and matches a pipe — but `\\|` reaches it as `\|`, which is the alternation
  the engine has always had. Nobody had tried two. Written down now, along with
  what a bare `|` does (ends the command, so `n/aaa|ccc` quietly becomes the
  rule `n/aaa` and a command `ccc`), and pinned by four cases so it cannot go
  back to being folklore.

  Not in a `w` rule: that one wraps its pattern in `\< \>`, so `w/a\\|b`
  compiles as `\<a` or `b\>` and colours the `a` in `ab`. A `w` list is also
  much the faster of the two, which is not the way round it looks — each word
  becomes its own rule, but a word rule is looked up in an index of the line
  built once per line, so a keyword that is not there costs nothing to rule
  out. Rewriting every `w` list in `c.jvsyn` as one alternation — 61 words,
  identical colouring — made a 600-redraw scroll 2.4 times slower.
- `syntax/make.jvsyn` was the same nine rules written out twice. Every Makefile
  walked a doubled list, and could not have coloured anything differently for
  it. `scripts/check-docs.sh` refuses a repeated rule now, and a repeated or
  empty word inside a `w` list with it: `rc.jvsyn` had `CURSOR` and
  `IDI_WINLOGO` twice and a stray `//` that built a rule for the empty word,
  and `java`, `vbs` and `html` each repeated a word the list above already had.
  None of it showed on the screen, which is why it accumulated.
- A batch parameter with modifiers is coloured. `bat.jvsyn` had seventeen rules
  for particular spellings of `%~…`, all of them written *after* the `%VAR%`
  rule — which matches from any `%` to the next one on the line, so it took
  `%~f1 %` out of `%~f1 %~dp2` and left the rest plain. The seventeen never
  coloured anything, and did not list `dpnx` in any case. One rule that takes
  the modifiers in any order, before `%VAR%`, replaces them.
- A cell the cursor stepped over on its way to the next one was retyped in the
  wrong colour. Moving a few columns along a row, jvim types the cells in
  between rather than positioning the cursor, which is faster — but it types
  their characters and nothing else, so they arrive in whatever colour the
  last escape asked for, which is the colour of the cell it is on its way to.
  An unchanged space in front of a coloured word came out inside that word's
  colour. Nobody saw it, because a space has no ink to be the wrong colour.
  The cells are now retyped only when they are already in that colour, and
  skipped over otherwise — they did not change, so there is nothing to draw.
- Syntax rules match Japanese again. Every walk over the text in `syntax.c`
  still stepped two bytes for a multi-byte character, which was Shift-JIS; the
  buffer has held UTF-8 since 1.0.0, where a kanji or a kana is three. A rule
  with a Japanese word in it never found it.
- A word rule (`w/`) that happened to be the first rule in the list kept the
  hash of zero it was allocated with, looked itself up in the wrong bucket, and
  so never matched anything.
- A region whose opening and closing tokens are the same string — Python's
  `"""`, a template literal, a fenced code block — used to close on the very
  characters that opened it when both were on one line, colouring the token and
  leaving the text after it plain. The closing token is now looked for past the
  opening one.
- The test suites no longer read the rc file of whoever runs them. `HOME` points
  at their own temporary directory, so a `_vimrc` — the shipped sample sets
  `textmode`, mappings and a rule set — cannot decide what the editor under test
  does. Installing the sample used to turn 14 passes into 3.
- A `:source` inside an rc no longer stops the per-file-type blocks from
  matching. The suffix and name being matched were pointers into `NameBuff`,
  which the nested `:source` reuses for the name it is expanding, so after the
  first one every `"begin suffixes=` was compared against the wrong string.
- `purple` is `#800080` rather than a second `maroon`.
- `stricmp` links outside the Windows build. It was replaced by `vim_stricmp`
  only where it already happened to be a macro, which is nowhere on glibc.
- The editor no longer aborts under AddressSanitizer while reading the rule
  files. Two `strcpy()` calls in `DoOneCmd()` unescape `\"` and `\%` by copying
  a string over itself one byte to the left; the ranges overlap, which is
  undefined. Nothing reached them often enough to notice until every startup
  sourced files full of `\"`. `STRMOVE()` now, as in the seven found before.
- The Windows build works again on a compiler that means it. `defcolor`, the
  entry `is_syntax()` uses to record that nothing matches the rest of a line, is
  initialised positionally, and a field added to the struct above `.color` moved
  its `'A'` into a pointer. gcc warns; clang, which FreeBSD builds with, stops.
  `scripts/build-unix.sh strict` catches it now.
- `scripts/build-unix.sh clean` no longer deletes `src/cmdtab.h`. It is
  generated but committed, and `makefile.mingw` has no rule to make one, so a
  clean left the Windows cross build unable to compile `cmdline.c`.
- `scripts/check-docs.sh` sees a case count that a paragraph wrapped between the
  number and the word. It reads one line at a time, and "All 100\ntests on each"
  sat in BUILDING-unix.md through two suites growing.
- `scripts/fetch-ci-build.sh` unpacks the package with its directories. It used
  `unzip -j`, which junks paths — that was how the version-named top directory
  was stripped, and it was harmless while the package was flat. Once the rules
  were split out of the rc it poured `syntax/`'s thirty files in beside the exe,
  and the editor said `can't open file …\syntax\filetype.jvsyn` on startup. The
  exe lands in the right place either way, which is the only thing anyone checks
  after unpacking, so nothing else gave a sign. It also says so itself now if the
  rules are missing afterwards, rather than leaving the editor to say it later.
- A group name that only one rule file uses is a colour in a `.jvsyn` too.
  `DiffAdd`, `MdHead`, `Url`, `Value`, `Arg` and the rest were defined in the
  file that used them, which works for every file but a rule file: a `.jvsyn` is
  opened with `common.jvsyn` and `jvimrc.jvsyn` and nothing else, so `DiffAdd`
  was not a name the editor knew while `diff.jvsyn` was on screen, and the very
  line that gives `DiffAdd` its colour came out plain. Twelve names, all in
  `common.jvsyn` now, where every other group name already was.

  Two of them had been given a colour twice. `Value` was grey in both files, and
  `Url` was one thing in `html.jvsyn` and another in `text.jvsyn`; it is
  `text.jvsyn`'s now — a URL in a page is no longer the same colour as the
  strings around it, and `Url` and `E-Mail` go together as they do in a mail.
- Installing the NetBSD guest gets its packages from where they actually are.
  `pkg_add` was pointed at `pkgsrc/packages/NetBSD/amd64/10.1/All/`, which the
  CDN now answers with a redirect to `x86_64/10.0_2026Q2`; the guest's
  `fetch(3)` does not follow that, so `pkg_add bash` sat there until something
  killed it, with nothing said. The directory is resolved with curl on the
  Linux side, which does follow it, and handed over — and if the redirect ever
  goes away that resolves to the address it started from.
- A first run of `scripts/test-bsd-docker.sh` that fails in the middle no
  longer leaves a guest disk behind for later runs to trust. Preparing a guest
  writes `prepared-<os>.qcow2` as it goes, and the run above stopped with a
  half-installed system in it; every run after that found the file, skipped
  preparing, and booted the wreck. The failure is caught now and the file is
  removed.
- `scripts/build-unix.sh strict` gets through with clang, not only with gcc. The
  `-Werror=` set had only ever been checked on Linux, and clang — which FreeBSD
  builds with — stopped in two places the first time it was asked. Three fixes,
  none of which change what the editor does:

  `unix.c`'s local `extern void getlinecol();` is a declaration without a
  prototype. clang refuses that under `-Werror=strict-prototypes`; gcc lets it
  pass, because a prototype for the same function is already in scope. It is
  `__ARGS((void))` now, which is what the same declaration in `dos_v.c` has
  always been.

  The `remove()` that stands in for a libc without one was still a K&R
  definition, with `const` switched into the middle of it by the preprocessor.
  clang does not warn about the form, it refuses it — `-Wdeprecated-non-prototype`,
  and C23 does not have it at all. Both shapes are written out as prototypes now.

  `term.c`'s `TGETSTR` cast the area argument to `char *` for everything except
  Linux and MSDOS, while the `tgetstr()` declaration a dozen lines above it in
  the same file says `char **`. A cast to the wrong type is the one thing a cast
  must not be, and there were 19 of them in `set_term()`. It is one macro now,
  casting to `char **`, which is what Linux's `<termcap.h>` and the BSDs' curses
  both declare. This is also what the 58 warnings BUILDING-unix.md recorded for
  NetBSD were, and it said they were not worth 58 casts to be rid of: they were
  worth one. The NetBSD guest now builds with no warnings at all.
- The `ansi` terminal built into JVim — used when there is no termcap/terminfo
  library to fall back on, or `$TERM` names it directly — had `t_el` (clear to
  end of line) clear the whole line instead: `ANSI_TCAP` wrote `\033[2K` where
  `\033[K` was meant. Every redraw that only means to erase ahead of the
  cursor — the tail of a line that got shorter, a status line, retyping a word
  with fewer characters — also erased everything already drawn to its left,
  undoing whatever the screen had just written there.
- `set_init()` recognised only the two exact spellings `en_US.UTF-8` and
  `ja_JP.UTF-8` as UTF-8; every other UTF-8 locale — `C.UTF-8`, `ja_JP.utf8`,
  whatever WSL or a modern distribution actually sets — fell through to the
  Unix default `jmask=EEET`, which reads the terminal as EUC-JP and mojibakes
  every Japanese character on a real UTF-8 terminal. Matching is a
  case-insensitive substring now (`UTF-8` / `utf8` anywhere in the name), an
  unset `$LC_CTYPE` falls back to `$LC_ALL` before `$LANG`, and the older
  JIS/EUC/Shift-JIS names are matched the same substring way rather than
  against a fixed list of exact spellings — in both `set_init()` and the
  bundled `grep` helper, which had the identical exact-match list.

### 日本語

- マッピングが、そのままでは持てなかった文字を名前で書けるようになりました。
  `<CR>` `<NL>` `<LF>` `<Esc>` `<Tab>` `<Space>` `<BS>` `<Nul>` の 8 つで、
  キー側・引数側の両方、大文字小文字を問いません。

      map q ihello<CR>
      map <Space> :w<CR>

  マッピングから Enter を押すには、これまで rc に生の復帰文字を置くしか
  ありませんでした。そのため**ファイルの改行コードがファイルの意味の一部**に
  なっていました。`dosource()` は各行末の CR を 1 つ取り除きますが、それが
  区切りなのか内容なのか区別できないので、Unix 改行のファイルではマッピングが
  黙って CR を失います。**同じ rc を Unix と Windows の両方では書けませんでした。**
  名前なら行末に失うものが何もありません。

  対象は文字だけです。キーは `#[UP]`・`#[F01]`・`#1` が既に名前を持っており、
  綴りを 2 通りにしても新しくできることは無く、同期を取る表が 2 つに増えるだけ
  です。一覧に無い名前で始まる `<` はそのまま `<` なので、`<div>` と打つ
  マッピングはそのまま動きます。名前として読ませたくない場合は `CTRL-V` で
  抑えられます — 展開と `CTRL-V` の除去は各半分につき 1 パスで行うので、
  片方がもう片方を打ち消すことはありません。キーと引数は展開前に分離します。
  さもないとキー側の `<Space>` がキーの終わりを示す空白になってしまいます。

  これにより起動時の警告 `Wrong line separator, ^M may be missing` は警告する
  対象が無くなったので削除しました。Unix 改行の rc が Windows でも静かに起動
  します（Unix と共有している dotfile はまさにこの形です）。CRLF ファイルの
  行末 CR は従来どおり取り除くので、そちらも変わらず動きます。
- `:syntax dump <file>` を追加しました。ルールがバッファに何をしたかをテキストで
  書き出します (色の付いた範囲ごとに1行、グループ名と該当ルール付き)。これまで
  ルールの間違いは「画面の色が足りない」以外に現れず、原因のルールを特定するには
  ピクセルを読むしかありませんでした。`scripts/test-syntax.sh` はこれをスイートに
  したもので、ルールが一致しなくなればテストが落ちます。
- 色が「後ろに何を敷くか」を言えるようになりました。`syntax link Error bolic white
  on maroon`、色そのものを書く `syntax link DiffAdd green on #e6ffe6`、`on` の前に
  何も書かず文字の色をそのままにする形の 3 通りです。名前付きの 16 色はどれも 1 行
  まるごと敷いて文字を読むには濃すぎるので、ルールファイルは欲しい淡色を直接
  書いています。`Error` と `Todo` はこの方式に変わりました — どちらも vim が背景色を
  与えている群で、背景が無いあいだは `bolic red` と `reverse` で代用されていました。
  reverse は「端末が持っている 2 色を入れ替えろ」という意味なので、青地に黄でも
  なければ、端末が違えば同じ見た目にもなりません。diff の追加行・削除行と、
  Markdown のコードフェンスにも背景が付きました。

  1 セルの属性は色 id を持つ 1 バイトで、256 のうち 145 が既に埋まっています。
  なのでビットを増やすのではなく、画面配列に 3 枚目の面を足しました。背景は前景の
  letter ではなく「ルールが求めた色の表の位置」です。背景が前景として使われることは
  絶対にないので、専用の空間を与えても何も失いません。

  描画側は両方が対応します。端末へは前景と同じエスケープの中に `48;2;r;g;b`
  (または 16 色フォールバックの 40 番台) を、Win32 GUI へは run ごとに既に塗って
  いる矩形の色として。GUI のほうは見るしか確かめようがないので、
  `scripts/test-winkeys.sh` が窓をビットマップに取ってピクセルを読みます。
- `scripts/test-sgr.sh` を追加しました。端末に実際に送られるエスケープと、その下に
  書かれた文字を読みます。`:syntax dump` は「どのルールがどのバイトを塗ったか」=
  ルールについての問いに答えますが、これはもう半分 —「その色が端末に届いているか、
  そして届くべきセルにだけ届いているか」です。描画側を見るものは他にありません
  (Windows スイートは GUI を動かし、dump は一切描画しません)。初回実行でいきなり
  バグを 1 つ見つけました。下の「修正」を参照。
- 3 バイト文字のファイル名を扱う文字コードのケースを 2 つ追加しました (名前で開く、
  ワイルドカードで見つける)。Windows 側には 1.0.0 からありましたが、Windows 固有
  でない部分は CI の走る環境でまったくテストされていませんでした。Windows スイートは
  Windows を必要とし、CI では動かないためです。
- シンタックスカラーが Win32 GUI だけでなく端末でも動くようになりました。ルールが
  指定した色を SGR エスケープとして出します。`$COLORTERM` が対応を示していれば
  その色そのもの、そうでなければ 16 色のうち最も近いものです。パレットを 1 箇所に
  まとめたので、端末と GUI で「navy」の意味が食い違うことはありません。
- ルールを `_jvimrc` の中の 1200 行から、種別ごと 1 ファイルの `syntax/` に移しました。
  rc からは `source $VIM/syntax/filetype.jvsyn` の 1 行で全部に届きます。種別の
  足し方は `syntax/README` にあります。
- 両方のビルドで使える短い rc `jvimrc.sample` を追加し、Windows パッケージに同梱、
  Unix では `$VIM` にインストールするようにしました。`~/.jvimrc` か
  `%HOME%\_jvimrc` にコピーして使います。この "j" の付く名前は JVim が `.vimrc`
  より優先して読み、ほかのエディタは読まないので、同じマシンの普通の vim に
  `set fexrc` やシンタックス定義を渡さずに済みます。これまで Unix ビルドで使える
  rc は同梱されていませんでした (`doc.j/_jvimrc` は FEPCTRL を要する
  `set fepkeys` で止まります)。
- Unix でも `$VIM` に既定値が入るようになりました (`$PREFIX/lib/jvim3`、
  `make install` がルールファイルを置く場所)。Windows では以前から exe の
  ディレクトリが入っていました。
- Python、JavaScript/TypeScript、Go、Rust、Ruby、シェル、Markdown、JSON、YAML、
  TOML、SQL、CSS/SCSS、C#、PHP、Lua、XML、diff、Makefile、Dockerfile のシンタックス
  カラー定義を `doc.j/_jvimrc` (配布物の `_jvimrc.sample`) に追加しました。これまで
  1998 年より新しい言語の定義はひとつもありませんでした。C の定義は `.cc` `.cxx`
  `.hpp` `.hxx` `.hh` `.inl` にも効くようになりました。
- Windows のキー入力スイートが画面に出なくなりました。実際のエディタを実際の
  ウィンドウで 14 回起動するため、そのつど画面に現れてキーボードを奪い、4 分間
  マシンが使えませんでした。ドライバが起動前に専用のデスクトップへ移るようにした
  うえで、コンソール版にはウィンドウを持たないコンソールを与えるようにしました。
  `PostMessage` も `WriteConsoleInput` もどちらも気にしません。デスクトップだけでは
  不十分でした。`CREATE_NEW_CONSOLE` は既定の端末に引き渡され、Windows Terminal は
  どのデスクトップから要求されても人が見ている側に開くためです。`WINDESK_OFF=1` で
  元に戻せます。
- シンタックスルールの未知のモード文字を、無視せずエラーにするようになりました
  (どの文字かをメッセージに出します)。2002 年の説明書は「その他は無視します」と
  していて実際そうだったため、モードの打ち間違いがそのままルールとして登録され、
  黙って違うものに一致していました。モードなしを表す `n` は、無視される文字では
  なく正式なモードにしました。
- シンタックスカラーが、行をまたぐ領域とタグの状態を行ごとに覚えるようになりました。
  これまでは 1 行描くたびに前後 `synlines` 行を探していたため、コメントや文字列
  がその範囲を超えると色が落ちていました。長さに関わらず色が保たれ、閉じていない
  コメントは以降すべてが色付き (従来は無色) になり、開始・終了の記号を打った時点
  で下の行の色がすぐ変わります。タグルール (`t`) も同じ状態を読むため、その範囲を
  超える長さで書かれたタグでもタグ名や属性に色が付きます。`<` と `>` が 100 行
  離れていても 1 行に並んでいるのと変わりません。`synlines` は今も受け付けるので
  設定してある rc はそのまま動きますが、もう誰も読んでいません。

  置き換え前の探索より遅くはなりません。入れ子タグ 1500 行の色付けは 0.78〜0.85 秒
  で、探索方式は 0.88〜1.03 秒でした (両者の色付け結果はバイト単位で同一)。`t` を
  使わないルールしか持たないバッファは区切りを 1 つも走査しませんし、領域の両端が
  タグと取り違えられることもありません。
- Windows パッケージが Windows の改行を持つようになりました。`dosource()` は
  ソースするファイルをバイナリモードで開き、各行末の CR を 1 つ取り除きます。CR が
  無ければ `Wrong line separator, ^M may be missing` と警告します。パッケージは
  Unix のチェックアウトから作っていたため、起動を終える前に 3 回警告が出ていました
  — rc で 1 回、`filetype.jvsyn` で 1 回、そこから読むルールで 1 回。3 つ出ると
  `Press RETURN or enter command to continue` で止まるので、**起動のたびにキーを
  1 回押す必要がありました**。

  この警告は些事ではありません（ただしほぼ常に無害です）。行末の CR は区切りで、
  それ以外の位置の CR は内容です。したがって LF のファイルでは、**内容が CR で
  終わるコマンド** — `map q ihello^M` のような Enter を押すマッピング — を
  普通の行と区別できず、CR が食われます。実測: 同じ rc を両方の改行で書くと、
  CRLF ではバッファが 2 行、LF では 1 行になります。CR で終わらないものはすべて
  同一に動き、ルールファイルの色付け結果もバイト単位で同一です。

  変換で触るのは**行末の CR だけ**です。`doc.j/_jvimrc` の 37 行目は
  `"map n /^Mz.` で、行の途中に本物の CR があります。最初に書いた「CR を全部
  落とす」版はこれを `/z.` にしてしまいました — 警告が言っているまさにその破壊を、
  警告を消す作業で持ち込んだわけです。ビルドは自分の出力を信用せず検査するように
  しました。Unix 側からはこの手の破損が一切見えないためです。

  自分の rc は自分で CRLF にする必要があります。同梱の `_jvimrc.sample` を
  コピーすればそうなります。
- ルールファイルを開くと色が付きます。`.jvsyn` にはファイル種別が一切割り当てられて
  いませんでした。シンタックスカラーのエンジンとしては具合の悪い話で、同梱の 30
  ファイルは配色を変えたい人が最も触るものなのに、真っ白で出ていました。中身は rc の
  `syntax` 行と同じ言語なので `jvimrc.jvsyn` を読ませます。あちらは既に群名を
  その群の色で、色名をその色そのもので描いています — `Error` が赤いのは `Error` が
  赤だからです。

  その `jvimrc.jvsyn` にも、以降に増えた記法が入っていませんでした。色名の一覧に
  `white` が無く、`link`・`clear`・`load`・`color`・`dump`・`crchar`、背景を導く
  `on`、`#rrggbb` も同様です。`white` は窓の地色の上では唯一「自分自身の色では
  描けない」名前なので、灰色を敷いて描いています — ルールが背景を言えるように
  なったので。

  行頭の `"` はコメントとして色が付きます。**行頭のものだけ**です。ルールファイルは
  パターンの中に `\"` を常時書くので、どの `"` でもコメント開始とみなすルールにすると
  `String` ルールのパターンが軒並みコメント色になってしまいます。`begin`/`end` の
  ブロック行は `"` を own の一部として取り込んであるので、同じ 0 桁目で競合しても
  そちらが勝ちます。
- 同梱の `_jvimrc.sample` と `jvimrc.sample` に色が付きます。多くの人が rc を最初に
  目にするのはこのファイルですが、`.sample` は rc として読まれる名前ではないため、
  どの種別にも一致していませんでした。
- ルールに選択肢 (alternation) を書けます。`syntax/README` は「書けない」と
  断言していましたが、間違いでした。ex のコマンド行はバックスラッシュを 1 つ食べる
  ので、`\|` は正規表現にはただのパイプとして届いてパイプに一致します。しかし
  `\\|` なら `\|` として届き、これは正規表現エンジンが最初から持っている選択肢です。
  誰も 2 つ書いて試していませんでした。素の `|` が何をするか (コマンドがそこで
  終わるので、`n/aaa|ccc` は黙ってルール `n/aaa` とコマンド `ccc` になる) と
  合わせて文書化し、ケース 4 本で固定したので、もう口伝には戻りません。

  ただし `w` ルールの中では使えません。`w` はパターン全体を `\< \>` で包むので、
  `w/a\\|b` は `\<a` または `b\>` になり、`ab` の `a` に色が付きます。そして
  `w` のリストは選択肢よりずっと高速です — 見た目に反しますが、1 単語が 1 ルールに
  なる代わりに、単語ルールは行ごとに 1 回作る索引で引かれるので、その行に無い
  キーワードを外すコストがゼロだからです。`c.jvsyn` の `w` リスト全部 (61 語) を
  1 本の選択肢に書き換えたところ、色付け結果は完全に同一のまま、600 回再描画の
  スクロールが 2.4 倍遅くなりました。
- `syntax/make.jvsyn` は同じ 9 ルールを 2 回書いたファイルでした。Makefile を開く
  たびに倍の長さのリストを走査していて、そのぶん色が変わることはありません。
  `scripts/check-docs.sh` がルールの重複を弾くようになり、`w` リスト内の重複語と
  空語も一緒に見ます: `rc.jvsyn` には `CURSOR` と `IDI_WINLOGO` が 2 回ずつと、
  空語のルールを作る `//` があり、`java`・`vbs`・`html` にもそれぞれ上の行と重複した
  語がありました。どれも画面には出ないので溜まっていたものです。
- バッチの修飾子付きパラメータに色が付きます。`bat.jvsyn` には `%~…` の個別の綴りに
  対するルールが 17 本ありましたが、すべて `%VAR%` のルールより**後ろ**にありました。
  `%VAR%` は行内の任意の `%` から次の `%` までに一致するので、`%~f1 %~dp2` からは
  `%~f1 %` が取られ、残りは無色でした。17 本は何も塗っておらず、そもそも `dpnx` は
  どれにも載っていません。修飾子を任意の順で取る 1 本を `%VAR%` の前に置きました。
- カーソルが通り過ぎるセルが間違った色で打ち直されていました。同じ行を数桁動くとき、
  jvim はカーソルを位置決めせず間のセルを打ち直します (そのほうが速い)。ところが
  打ち直すのは文字だけで、色は直前のエスケープが指定したまま — つまり「これから
  描くセルの色」です。色の付いた語の手前にある無関係な空白が、その語の色の中で
  打ち直されていました。空白にはインクが無いので誰も気付きませんでした。今は
  「既にその色になっているセル」のときだけ打ち直し、そうでなければ飛ばします
  (変わっていないセルなので、そもそも描く必要がありません)。
- 日本語のシンタックスルールが再び一致するようになりました。`syntax.c` の文字送り
  がすべて Shift-JIS 時代の 2 バイト前提のままで、1.0.0 でバッファが UTF-8
  (漢字・かなは 3 バイト) になって以降、日本語を含むルールは何にも一致していません
  でした。
- 単語ルール (`w/`) がリストの先頭に来た場合、ハッシュが 0 のままになり、誤った
  バケットを引いて一致しませんでした。
- 開始と終了が同じ文字列の領域 (Python の `"""`、テンプレートリテラル、コードフェンス)
  が 1 行に収まっている場合、開始トークン自身で閉じてしまい、記号だけが色付いて後ろの
  文字列が無色になっていました。終了トークンを開始トークンより後ろから探すようにしました。
- テストスイートが実行者の rc ファイルを読まなくなりました。`HOME` をスイート自身の
  一時ディレクトリに向けているので、`_vimrc` (同梱サンプルは textmode・マッピング・
  ルール定義を設定します) がテスト対象の挙動を変えることはありません。サンプルを
  導入すると 14 pass が 3 pass になっていました。
- rc の中で `:source` すると、以降ファイル種別ごとのブロックが一致しなくなる問題を
  修正しました。判定に使う拡張子とファイル名が `NameBuff` へのポインタで、入れ子の
  `:source` が同じバッファを展開に使うため、1 回目以降はすべて誤った文字列と比較して
  いました。
- `purple` が maroon と同じ色だったのを `#800080` にしました。
- Windows 以外のビルドで `stricmp` がリンクできない問題を修正しました。`vim_stricmp`
  への置き換えが「すでにマクロだった場合」にしか効かず、glibc では効きませんでした。
- `syntax/` の全ファイルに `scripts/test-syntax.sh` のケースを用意しました。これまで
  30 ファイル中 18 だけで、HTML、XML、C#、バッチ、VBScript、プレーンテキスト、Java、
  INI、`.def`、`.ec`、`.rc`、rc 自身の 12 個は配布しているのに未検証でした。
- CSS でプロパティ名、INI でキー、Java で数値と文字定数に色が付くようになりました。
  それぞれ、その定義ファイルに欠けていた最も基本的なものです。
- `scripts/build-unix.sh strict` を追加しました。CI がエラー扱いする `-Werror=` 群
  つきでビルドするので、push の後ではなく前に確認できます。この中には gcc なら警告
  で済み、FreeBSD が使う clang では止まるものがあります。
- Windows のキー入力スイートに、GUI ビルドでシンタックスカラーが動いているかを見る
  ケースを追加しました。Windows では色付けは GUI 版だけの機能で (`SYN_ON()` は
  `'syntax'` かつ `GuiWin`)、色を読み戻すスイートは pty を必要とするため、そこで
  エンジンが動いているかを確かめるものがありませんでした。
- `:syntax dump` が、タグルール (`t`) を「内側を見るタグ」ではなく「実際に一致する
  パターン」で表示するようになりました。`html.jvsyn` の十数個のルールが揃って
  `t/<` と名乗っており、区別が付きませんでした。
- `scripts/test-bsd-docker.sh` がゲスト内で 3 つのスイートすべてを実行します
  (CI が同じ OS でしていることと同じ)。文字コードのスイートしか動かしていなかった
  ため、BUILDING-unix.md がゲストで走っていないテストの結果を書いていました。
- GUI のキードライバが、ケース開始前に Escape を 1 回打つようにしました。ウィンドウが
  出るのは rc を読み終わる前で、その間に送ったキーは遅れるのではなく失われるため、
  ケースの 2 打目が 1 打目として扱われていました。rc がなければこの隙間は狭く、固定の
  sleep でほぼ隠れていましたが、それが一番たちの悪い状態です。100 回に 1 回落ちて、
  理由が何も残りません。
- 手元に置く BSD のゲストを FreeBSD だけにしました。1.3 GB の 1 ファイルです。
  `scripts/test-bsd-docker.sh` には NetBSD のゲストと、両方を走らせる `all` が
  ありましたが、2 つで `~/.cache` を 12 GB ほど使っていました。しかもその網羅は
  すでに CI にあります (push ごとに FreeBSD・NetBSD・OpenBSD・DragonFly でスイートを
  実行しています)。ローカルのゲストの価値は時間ではなく (1 回にかかる時間は CI と
  だいたい同じです)、push せずに分かることです。今のツリーをそのままビルドし、
  `shell` なら失敗した状態のゲストに入れます。それが効くのは FreeBSD です。clang で
  ビルドするので、gcc が警告で済ませるところで止まります。NetBSD は CI が NetBSD 固有の失敗を見つけたときのために `netbsd` で
  入れられます。`all` は廃止しました。
- 保存するゲストが、リリースイメージとその上のオーバーレイの 2 つではなく、圧縮した
  1 ファイルになりました。以前は両方を残す必要があり、FreeBSD では 3.6 GB と
  4.3 GB でした。しかもオーバーレイのほとんどは初回起動の自己パッチです。
  ダウンロードしたパッチ、置き換えた全ファイルの控え、そして削除済みファイルが
  まだ入っているブロック (削除でブロックは書き換わらず、ゼロでないブロックは保存
  しなければなりません)。そこでキャッシュを消し、空き領域をゼロで埋め、2 つの
  ファイルを zstd 圧縮の qcow2 1 つに畳み込みます。7.9 GB が 1.3 GB になり、
  ダウンロードは削除します (作り直すときは、どうせ新しいリリースを取り直すことに
  なります)。テストは今もその上のオーバーレイで走り、ゲストは 20 秒で立ち上がり、
  179 ケースすべて通ります。古い版が保存したゲストは `freebsd compact` で
  再インストールなしに同じ形にできます。`clean` はダウンロードも消します。ゲストを
  作るとき・縮めるときに 25 GB の空きが必要になったのはゼロ埋めのためです。圧縮して
  縮む前に、qcow2 が仮想ディスク全体まで膨らみます。
- CI の FreeBSD ジョブが、テストのあとに strict ビルドも実行します。Linux ジョブは
  フラグ一覧を書き並べるのをやめ、`build-unix.sh strict` から取ります。ここで clang
  でコンパイルするジョブは FreeBSD だけなので、この警告セットが clang で緑かどうかを
  言えるのも FreeBSD だけです。そして緑ではありませんでした。同じフラグの一覧が
  2 つあれば離れていく恐れもあります。今は 1 つで、push 前に誰でも走らせられる
  ものです。
- AddressSanitizer 下でルールファイルを読むと異常終了する問題を修正しました。
  `DoOneCmd()` の 2 か所が `\"` と `\%` の解除に、文字列を 1 バイト左へ自分自身の上に
  `strcpy()` していました。範囲が重なるので未定義動作です。起動のたびに `\"` だらけの
  ファイルを読むようになるまで、気づくほど通る場所ではありませんでした。既出の 7 か所
  と同じく `STRMOVE()` にしました。
- 本気のコンパイラで Windows 版が再びビルドできるようになりました。`is_syntax()` が
  「この行の残りは何にも一致しない」と記録するために使う `defcolor` は位置指定の
  初期化子で、`.color` より前に構造体メンバを足したことで `'A'` がポインタに入って
  いました。gcc は警告、FreeBSD が使う clang は停止します。`scripts/build-unix.sh
  strict` で検出できます。
- `scripts/build-unix.sh clean` が `src/cmdtab.h` を消さないようにしました。生成物
  ではありますがコミットされており、`makefile.mingw` には作り直す規則がないため、
  clean すると Windows クロスビルドが `cmdline.c` をコンパイルできなくなっていました。
- `scripts/check-docs.sh` が、数字と単語の間で折り返されたケース数を見落とさなくなり
  ました。1 行ずつ読んでいたため、"All 100\ntests on each" が BUILDING-unix.md に
  スイート 2 回分の増加をまたいで残っていました。
- `scripts/fetch-ci-build.sh` がパッケージをディレクトリ構造ごと展開するように
  なりました。`unzip -j` を使っていたためパスが潰れていました。バージョン名の
  トップディレクトリを剥がす手段としてはそれでよく、パッケージが平坦なあいだは
  無害でしたが、ルールを rc から分離して以降は `syntax/` の 30 ファイルが exe と
  同じ場所にばら撒かれ、起動時に `can't open file …\syntax\filetype.jvsyn` と
  出るようになっていました。どちらにせよ exe は正しい位置に落ちる — 展開後に
  確認されるのはそれだけ — ので、他に兆候がありませんでした。展開後にルールが
  無ければスクリプト自身がその場で言うようにもしました。
- HTML の `<script>` `<style>` の中身を 1 つの塊として区切るようにしました。地色は
  Markdown のフェンス付きブロックと同じ淡い灰色で、文字色はそのままです。これまでは
  地の文として描かれていたので、`var` や `function` には何も付かず、逆に `<` と `>`
  の間にたまたま入った語には同名の HTML 属性の色が付いていました。JavaScript を
  JavaScript として色付けすることはできません — あるルール集合がバッファの範囲を
  別のルール集合に渡す手段が無いからです — が、塊としては読めるようになりました。
  開始タグと終了タグは領域の内側です。領域は最初のパターンが始まる位置から
  色が付くためです。
- 1 つのルールファイルでしか使わない群名も、`.jvsyn` の中で色が付くようになりました。
  `DiffAdd`・`MdHead`・`Url`・`Value`・`Arg` などは、それを使うファイルの中で定義して
  いました。ルールファイル以外なら問題はありませんが、`.jvsyn` を開いたときに読まれる
  のは `common.jvsyn` と `jvimrc.jvsyn` だけです。つまり `diff.jvsyn` を画面に出して
  いるあいだ `DiffAdd` はエディタの知らない名前で、`DiffAdd` に色を与えている当の行が
  無色で表示されていました。12 個の名前を、他のすべての群名と同じ `common.jvsyn` に
  移しました。

  そのうち 2 つは二重に定義されていました。`Value` はどちらのファイルでも gray で
  したが、`Url` は `html.jvsyn` と `text.jvsyn` で別の色でした。`text.jvsyn` の側に
  揃えています — ページ中の URL がまわりの文字列と同じ色ではなくなり、`Url` と
  `E-Mail` はメールでの組み合わせのまま揃います。
- `:colorscheme` (`:colo`) でテーマを名前で読み込めるようになり、`:highlight`
  (`:hi`) は `syntax link` だけでなく Vim 本来の `guifg=` / `guibg=` / `gui=` /
  `link` / `clear` 構文も読むようになりました。`colors/` に dracula、nord、
  gruvbox、monokai、one-dark、desert、tokyonight、solarized-dark、
  solarized-light と、`set background=dark|light` で切り替わる既定の 2 つを
  同梱しています。テーマ名はまず `~/.jvim/colors/`、次に `$VIM/colors/` から
  探すので、`:colorscheme mine` はどちらに置いたファイルも見つけます。本物の
  Vim のカラースキームは Vimscript ですが、ここで読むのは同梱のテーマが使って
  いる範囲の命令だけです — `set`、`hi`、`let g:colors_name`、`finish`、
  `if`/`elseif`/`else`/`endif`。`if` の条件は評価されないため、両方の分岐が
  実行されます。詳細は [USAGE.ja.md](USAGE.ja.md#配色テーマ)。
- `syntax/common.jvsyn` の既定パレットを、暗い背景の端末でも読みやすい配色に
  作り直しました。2002 年当時のものは明るい GUI ウィンドウ向けの配色でした。
- NetBSD ゲストのインストールが、パッケージを実際にある場所から取るようになりました。
  `pkg_add` に渡していた `pkgsrc/packages/NetBSD/amd64/10.1/All/` は、現在 CDN が
  `x86_64/10.0_2026Q2` へのリダイレクトを返します。ゲストの `fetch(3)` はこれを
  追わないため、`pkg_add bash` は何も言わずに、誰かが止めるまで待ち続けていました。
  リダイレクトを追える Linux 側の curl でディレクトリを解決して渡します。将来
  リダイレクトがなくなれば、元のアドレスがそのまま解決されます。
- `scripts/test-bsd-docker.sh` の初回実行が途中で失敗したときに、あとの実行が
  信用してしまうゲストディスクを残さなくなりました。ゲストの用意は
  `prepared-<os>.qcow2` に書きながら進むため、上記の失敗では中途半端に
  インストールされた状態のディスクが残り、以降の実行はそれを見つけて用意を飛ばし、
  その残骸を起動していました。失敗を捕まえてファイルを削除します。
- `scripts/build-unix.sh strict` が gcc だけでなく clang でも通るようになりました。
  この `-Werror=` セットはこれまで Linux でしか確かめておらず、FreeBSD がビルドに
  使う clang に初めて訊いた時点で 2 か所で止まりました。修正は 3 つ、いずれも
  エディタの動作は変えません。

  `unix.c` の関数内の `extern void getlinecol();` はプロトタイプのない宣言です。
  clang は `-Werror=strict-prototypes` でこれを拒否します。gcc は同じ関数の
  プロトタイプがすでにスコープにあるため通します。`dos_v.c` の同じ宣言が昔から
  そうであるように、`__ARGS((void))` にしました。

  libc に `remove()` がない環境向けの代替定義が K&R 形式のままで、しかも途中に
  プリプロセッサで `const` を差し込む形でした。clang はこの形を警告ではなく拒否
  します (`-Wdeprecated-non-prototype`。C23 にはこの形自体がありません)。両方の形を
  プロトタイプで書き出しました。

  `term.c` の `TGETSTR` は、Linux と MSDOS 以外では area 引数を `char *` に
  キャストしていました。同じファイルの十数行上にある `tgetstr()` の宣言は
  `char **` です。誤った型へのキャストはキャストが唯一やってはいけないことで、
  それが `set_term()` に 19 個ありました。今は 1 つのマクロで `char **` に
  キャストします。Linux の `<termcap.h>` も BSD の curses もそう宣言しています。
  BUILDING-unix.md が NetBSD について記録していた 58 個の警告もこれでした。
  「58 個のキャストを書くほどの価値はない」と書いていましたが、必要だったのは
  1 個でした。NetBSD ゲストのビルドは警告 0 件になりました。
- JVim 組み込みの `ansi` 端末定義 — termcap/terminfo ライブラリが無いときの
  フォールバック、または `$TERM` が直接それを指す場合に使われます — で、
  `t_el`（カーソルから行末までのクリア）が行全体を消していました。
  `ANSI_TCAP` が `\033[K` と書くべきところを `\033[2K` にしていたためです。
  カーソルより先だけを消すつもりの再描画 — 短くなった行の末尾、ステータス行、
  文字数の少ない単語への打ち直しなど — のたびに、カーソルより左に
  すでに描いてあったものまで消え、画面がそこに書いたばかりの内容を
  台無しにしていました。
- `set_init()` が UTF-8 と認識するのは `en_US.UTF-8` と `ja_JP.UTF-8` という
  完全一致の 2 つだけで、それ以外の UTF-8 系ロケール — `C.UTF-8`、
  `ja_JP.utf8`、WSL や最近のディストリビューションが実際に設定してくる
  もの全般 — は Unix の既定値 `jmask=EEET` に落ちていました。これは端末表示を
  EUC-JP として読むため、実際は UTF-8 の端末で日本語がすべて文字化けします。
  判定は大文字小文字を区別しない部分一致になりました（名前のどこかに
  `UTF-8` / `utf8` があればよい）。`$LC_CTYPE` が未設定のときは `$LANG` の前に
  `$LC_ALL` も見るようになり、JIS・EUC・Shift-JIS の従来の判定も、固定の
  完全一致リストではなく同じ部分一致方式にしました — `set_init()` と、
  同じ完全一致リストを持っていた同梱の `grep` ヘルパーの両方です。

## 1.0.0 — 2026-08-22

The tree starts numbering itself. Everything in this section already shipped,
across the nine tags `v3.0-j2.1b-utf8.1` … `.9`, between 2026-08-19 and
2026-08-21; 1.0.0 is where those become one release with notes worth reading.

### Added

- The buffer holds UTF-8 instead of Shift-JIS, so hangul, accented Latin, emoji
  and anything outside the BMP survive being read, edited and saved.
- A Unicode Win32 GUI: `RegisterClassW`, `ExtTextOutW`, `CF_UNICODETEXT`
  clipboard, UTF-16 `WM_CHAR` joined into UTF-8.
- File names outside CP932 work — the manifest asks for UTF-8 as the process
  code page, so `🍣.txt` opens.
- Per-monitor DPI awareness, with the stored font and window sizes restated for
  the DPI in front of them.
- `scripts/build-unix.sh` asks the compiler what the machine has instead of
  asking you to uncomment three lines in a makefile. `scripts/build-mingw.sh`
  cross builds the Windows executables with mingw-w64.
- 110 tests — 46 encoding cases and 64 editing cases — driven through a real
  pty, run in CI on Linux, FreeBSD, NetBSD, OpenBSD and DragonFly.
- The binary now says which release it is: `:version`, `jvim3 -h`, the Windows
  file properties and the crash report all carry it.

### Fixed

- `[あ]` in a regexp no longer also matches `い`.
- A command line no longer reads `buff[-1]`.
- Encoding detection no longer tips a whole file over to Shift-JIS because of
  one emoji.
- Terminal input no longer mangles a character split across two reads.
- Cursor keys move instead of inserting stray characters: a special key is no
  longer put through the code conversion.
- A Japanese file name makes it from the directory scan through to the screen.
- `:!` and `:r !cmd` write their temp file somewhere it can actually be written,
  whatever `TEMP` is called, and keep all of the command's output.
- `:help` finds the help file under the name it is actually shipped as.
- Emoji and the end of a line are drawn whole, and a run's background is filled
  once.
- 64 bit Windows no longer loses half a pointer. It compiles clean; it has still
  never been run.

### Removed

- BDF font rendering and editing inside LHA/ZIP/TAR archives, sources and all,
  because their terms made the tree awkward to redistribute.
- macOS from CI: nobody here has one to try, so a failure there was a puzzle
  nobody was in a position to solve. The `__APPLE__` paths stay, shared with the
  BSDs, but nothing checks them.

### 日本語

- バッファが Shift-JIS ではなく UTF-8 を保持するようになりました。ハングル、
  アクセント付きラテン文字、絵文字、BMP 外の文字が、読み書き・編集を通して
  失われません。
- Win32 GUI を Unicode 化しました (`RegisterClassW`、`ExtTextOutW`、
  `CF_UNICODETEXT`、UTF-16 の `WM_CHAR` を UTF-8 に結合)。
- CP932 にない文字を含むファイル名が開けます。マニフェストでプロセスのコード
  ページを UTF-8 にしているためです。
- モニタごとの DPI に対応し、保存したフォントとウィンドウのサイズを目の前の DPI
  で読み直します。
- `scripts/build-unix.sh` が makefile の 3 行をコメントアウトさせる代わりに、
  コンパイラに環境を尋ねます。`scripts/build-mingw.sh` は mingw-w64 で Windows
  版をクロスビルドします。
- テストを 110 ケース (文字コード 46、編集 64) 用意し、実際の pty 越しに Linux、
  FreeBSD、NetBSD、OpenBSD、DragonFly の CI で実行しています。
- バイナリが自分のリリース番号を名乗るようになりました。`:version`、`jvim3 -h`、
  Windows のファイルのプロパティ、クラッシュレポートのすべてに出ます。
- 正規表現の `[あ]` が `い` にも一致する問題を修正しました。
- コマンドラインが `buff[-1]` を読む問題を修正しました。
- 絵文字 1 文字でファイル全体が Shift-JIS と判定される問題を修正しました。
- 2 回の read に分割された文字が壊れる問題を修正しました。
- 特殊キーをコード変換に通さないようにし、カーソルキーが動くようにしました。
- 日本語のファイル名が、ディレクトリ走査から画面表示まで通るようになりました。
- `:!` と `:r !cmd` が、`TEMP` の名前に関わらず書き込める場所に一時ファイルを
  作り、コマンドの出力を取りこぼさなくなりました。
- `:help` が、実際に同梱している名前でヘルプファイルを見つけます。
- 絵文字と行末を欠けずに描画し、1 行の背景を一度で塗ります。
- 64 ビット Windows でポインタが半分失われる問題を修正しました。コンパイルは
  通りますが、まだ誰も実行していません。
- BDF フォント描画と LHA/ZIP/TAR 内の編集を、ソースごと削除しました。再配布の
  条件が扱いにくかったためです。
- macOS を CI から外しました。試せる人がここにいないため、失敗しても誰も解決
  できません。BSD と共有している `__APPLE__` の分岐は残していますが、確認する
  ものはありません。

## Before 1.0.0 — `v3.0-j2.1b-utf8.1` … `.9` (2026-08-19 … 08-21)

Nine tags in three days, every one of them published with the same release
notes. They were checkpoints in one continuous run of work rather than nine
separate products, so rather than invent per-tag notes after the fact, what that
run did is written up under 1.0.0 above. The tags and their release pages stay
where they are.

日本語: 3 日間で 9 個のタグを打ちましたが、リリースノートはすべて同じ内容でした。
これらは 9 個の別々の成果物ではなく、ひと続きの作業の途中の記録です。後から
タグごとのノートを捏造するより、その期間に何をしたかを上の 1.0.0 にまとめて
あります。タグとリリースページはそのまま残します。

[Unreleased]: https://github.com/kuwa72/jvim3/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/kuwa72/jvim3/compare/v3.0-j2.1b-utf8.9...v1.0.0
