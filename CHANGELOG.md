# Changelog

JVim 3's own version numbers start at 1.0.0. "JVim 3.0-j2.1b" is where this tree
came from, not a version of it: that release is Tsuchida Ken'ichi's, from
December 2002, and it does not change again.

English first in each section, 日本語 after it. One file rather than two, because
a `CHANGELOG.ja.md` beside this would be the highest-churn document pair in the
repository and would drift within three releases.

## Unreleased

### Added

- The cursor keys page the `:help` screen: down is SPACE, up is `b`. Only the
  *shifted* arrows did, which is not what anybody reaches for, and the plain
  ones fell through to the fault below.
- Command-line (: mode) completion enhancements for `:colorscheme`, `:highlight`, and `:syntax`.
- Supported internal re-indentation for `=` operator (`==`, `=G`, etc.) when `equalprg` is empty, removing external `indent` dependency on Windows and Unix.
- Bundled standalone lightweight C code formatter tool (`tools/cformat.c` / `cformat.exe`) in Windows packages.
- Added `:macros` command to display recorded keyboard macros only.
- Enhanced tag jump candidate list with filename, kind/type, and tag name display, clipping lines to screen width.
- Added `jvimtutor` / `jvimtutor.bat` runner and `:tutor` / `:Tutor` commands to practice Vim using a safe temporary copy of the tutorial, prioritizing Japanese (`tutor.j`) on Japanese locales.
- Added tests in `scripts/test-editing.sh` for `:macros`, internal re-indentation, tag jump candidates, and `jvimtutor` / `:Tutor`.
  287 cases now.



### Fixed

- **`Q` crashed the editor.** `Qj`, `Q}`, `Q` with any motion at all: a null
  dereference before it had formatted anything, on every platform. `doformat()`
  in `src/ops.c` calls `insertchar()` with no character to insert, which the
  original spells `insertchar(NUL)`; the KANJI build takes a pointer and a byte
  count instead, so `NUL` arrived as a null pointer and `bytes[0]` read it on
  the first line of the function — before the `c == NUL` that means "format
  only, insert nothing" was ever looked at. `Q` has been unusable in JVim's
  KANJI build since the signature changed. The default settings do not include
  `formatprg`, so nothing pushed the command down the external-filter path
  that would have avoided it.
- **A cursor key on the `:help` screen jumped to an arbitrary screen, and then
  crashed.** The down arrow was handed to `isalpha()`, which is undefined for a
  key code: `K_DARROW` is 322, not a character, and the Windows C runtime
  answered yes. `screennr = c - 'b'` then came to 224, and `filepos[]` — 52
  entries on the stack — was read that far past its end. What came back was
  whatever the stack held: usually a plausible-looking file offset, which is why
  it looked like a jump to a random page, and sooner or later a seek to
  somewhere that is not in the help text at all, which is the crash while paging
  back with `b`. Only ASCII letters name a screen now, and `screennr` no longer
  reaches `MAXSCREENS` itself, which was one past the last entry.
- The rest of that family, found by auditing every `ctype` call in the tree.
  `isasciilower()` and the four beside it are in `src/vim.h` now, and everything
  that classifies a value straight from `vgetc()` asks those instead:
  - `isidchar()` and `isabchar()` in `src/charset.c` say no to anything that is
    not a byte. This one was reachable on Linux as well as Windows — a function
    key on the `:` line went to `isalnum(329)`, and whether an abbreviation
    expanded in front of it was then whatever the locale table happened to hold
    past its end.
  - the register name after `"` (`is_yank_buffer()`), and the one after `q`
    (`dorecord()`) — a name that got through indexed `y_buf[]`, 36 entries, at
    277 for the down arrow.
  - the mark name after `m`, `'` and `` ` `` (`setmark()`, `getmark()`) — a
    **write** to `namedfm[]`, 26 entries, at 257.
  - the count after `z`, and the digits of `CTRL-V nnn` and `CTRL-V #nnnn`.

  These four need the key to arrive as a code, which today happens only where
  the terminal codes are translated -- so `:help` is the one that bit. The
  others are the same defect and stop being a question of what a C runtime
  answers for a value it was never given.

### 日本語

- `:help` 画面をカーソルキーでめくれるようになりました。↓ が SPACE、↑ が `b`
  と同じ動きです。これまで反応したのは *Shift* 付きの矢印だけで、普通の矢印は
  下の不具合の入口になっていました。
- **`Q` でエディタが落ちていました。** `Qj`、`Q}`、`Q` に何かモーションを
  続けた場合すべてで、整形を始める前に NULL 参照で落ちます。プラットフォームを
  問いません。`src/ops.c` の `doformat()` は挿入する文字を持たないまま
  `insertchar()` を呼びます。オリジナルの `insertchar(NUL)` に対し KANJI 版は
  引数がバイト列とバイト数なので、`NUL` がヌルポインタとして渡り、関数の
  1 行目の `bytes[0]` がそれを読んでいました。「NUL は挿入せず整形だけ」という
  判定にたどり着く前です。シグネチャが変わって以来、KANJI 版の `Q` は
  使えない状態でした。既定では `formatprg` が空で、設定されていれば通る外部
  フィルタ経路には行かないため、誰も回避できませんでした。
- **`:help` でカーソルキーを押すと無関係な画面に飛び、その後落ちていました。**
  ↓ が `isalpha()` に渡っていました。キーコードに対しては未定義で、`K_DARROW`
  は文字ではなく 322 です。Windows の C ランタイムはこれを「英字」と答えます。
  すると `screennr = c - 'b'` が 224 になり、スタック上の 52 要素の配列
  `filepos[]` をそこまではみ出して読みます。読めた値はスタックの残骸で、
  たいていはそれらしいファイル位置に見えるため「でたらめなページに飛ぶ」
  ように見え、いずれヘルプ本文の外を指したところで落ちます。`b` で戻っている
  最中に突然落ちるのはこれです。画面を指定できるのは ASCII の英字だけにし、
  `screennr` が最後の要素の 1 つ先である `MAXSCREENS` に達しないようにしました。
- 同じ種類の問題を、ツリー内の `ctype` 呼び出しを全数監査して洗い出しました。
  `isasciilower()` ほか 4 つを `src/vim.h` に置き、`vgetc()` から来た値を
  分類している箇所はすべてそちらを使います。
  - `src/charset.c` の `isidchar()` と `isabchar()` は、バイトでない値には
    FALSE を返します。ここは Windows だけでなく Linux でも到達しました。
    `:` 行でファンクションキーを押すと `isalnum(329)` に渡り、その手前の
    アブリビエーションが展開されるかどうかが、ロケールのテーブルの
    はみ出した先に何があるか次第になっていました。
  - `"` の後のレジスタ名（`is_yank_buffer()`）と `q` の後のレジスタ名
    （`dorecord()`）。通ってしまうと 36 要素の `y_buf[]` を、↓ なら 277 番目で
    参照します。
  - `m`・`'`・`` ` `` の後のマーク名（`setmark()`、`getmark()`）。26 要素の
    `namedfm[]` の 257 番目への**書き込み**です。
  - `z` の後のカウントと、`CTRL-V nnn`・`CTRL-V #nnnn` の数字。

  この 4 つはキーがコードとして届く必要があり、現状それが起きるのは端末の
  キーコードを変換する経路だけです。実害が出ていたのは `:help` でした。
  残りも欠陥としては同じで、C ランタイムが「渡されるはずのない値」に
  どう答えるか次第、という状態ではなくなりました。

## 1.2.1 — 2026-09-01

### Added

- **CI runs the Windows executables now**, which nothing automatic had ever
  done: they were compiled, and the same portable sources were tested on a Unix.
  Everything Windows-only — the console it starts in, the shell `:r !` calls, the
  ANSI file APIs it opens names through — was covered by nothing, and `:r !cmd`
  shipped broken twice. `scripts/test-winrun.sh` drives the editor through
  script input: the ordinary operators, `:r !echo`, a UTF-8 round trip and a
  Japanese file name, 6 cases, against both architectures of the package the
  `windows` job built — not something the runner compiles, which would be the
  other C runtime. It runs by hand as well, from WSL or from Git Bash on
  Windows, and gates a release. What it cannot reach is keys: script input comes
  back from `inchar()` before the keyboard conversion, so
  `scripts/test-winkeys.sh` and its 16 typed cases still need a Windows machine.
- **A Unix build now catches the signals it cannot carry on through**, and does
  what Windows has done since 1.0.0 through `src/w32crash.c`: says which signal
  it was, writes the swap files out and names them, and points at `jvim3 -r`.
  SIGHUP, SIGQUIT, SIGILL, SIGTRAP, SIGABRT, SIGFPE, SIGBUS, SIGSEGV and
  SIGTERM. Before this, `grep SIGSEGV src/*.c` found nothing: a fault, or an ssh
  session closing — SIGHUP, which is not a crash at all — took whatever had not
  reached the swap file with it, left the terminal in raw mode, and said nothing
  about `-r`. The terminal is put back first, the swap files are preserved and
  **not** deleted, and a second signal during that exits at once rather than
  going round again. SIGSEGV and SIGBUS are left to AddressSanitizer under
  `build-unix.sh asan`, whose report is worth more than the message.
- `scripts/test-hostile.sh`, a fifth suite: 18 cases that hand the editor input
  nobody intended, or a hostile end, where the other four hand it input it is
  meant to accept.
  2 MB on one line, every possible byte value, invalid and truncated UTF-8, a
  1015 character pattern to `:syntax` (`pattern[1024]` on the stack is as close
  as the command line can get to it), 60 nested groups in `:s`, a tabstop of two
  billion, 200k lines through `:g`, a colour scheme whose every token is
  longer than the command line can hold, 17 kB of tags on one line with the
  colouring rules loaded, and the session dropping mid-edit.
  Seventeen of them pass; one is KNOWN-FAIL and is the issue below. Input files
  come from `scripts/hostilegen.c` rather than from awk or dd, because five
  operating systems run this and their awks disagree about a zero byte.
- Two cases in `scripts/test-editing.sh` for a `:s` whose replacement holds a
  carriage return, which breaks the line there, and one held off by a CTRL-V,
  which puts a real CR in the text. Both halves of that branch in `dosub()` were
  rewritten below and neither had ever been run by a test.
- Two cases in `scripts/test-syntax.sh` for a pair rule and for the same rule
  with `w` as well, which is the bug below.
- Two more in `scripts/test-editing.sh` for an indent wider than a tabstop —
  tabs and then the remainder in spaces, or spaces throughout under
  `expandtab` — which is what `set_indent()` builds, rewritten below.
- `PTYRUN_SIGNAL` in `scripts/ptyrun.c`: the signal to send when the timeout
  expires, instead of the SIGKILL it has always sent, with the copying carried
  on afterwards so that what the command prints on its way out and the status it
  exits with are both collected. It is the only way to test what the editor does
  when the session goes away — a command started in the background cannot have
  the pty as its controlling terminal, and the editor will not start without
  one.
- Two cases in `scripts/test-encoding.sh` for autodetection where the file is
  not perfectly one thing: a UTF-8 file with one byte that is not, and a
  Shift-JIS file that has to stay Shift-JIS. They pin what happens today rather
  than fixing anything; see the note below. 235 cases in all.
- `scripts/build-unix.sh asan` and `scripts/build-unix.sh ubsan` build with
  AddressSanitizer or UndefinedBehaviorSanitizer and then run the same suites
  the `test` target does, and CI runs both on every push. ASan had only ever
  been used through a line of environment variables typed from memory, and
  UBSan had never been run at all. Both targets point the sanitizer at
  `log_path` and collect the reports afterwards: every suite sends the editor's
  stderr to `/dev/null`, because what they compare is the bytes it writes, so a
  report would otherwise vanish with it — and UBSan carries on after a finding
  and exits 0, which would have made the job green while it was finding things.

### Changed

- **Every Japanese document in the tree is UTF-8 now**, including
  `doc.j/vim.hlp`, which `:help` reads and which the Windows package ships
  beside the exe. They were ISO-2022-JP, from 2002, and unreadable without
  telling an editor or a browser what they were. `judge_jcode()` detects UTF-8
  the same way it detected JIS, so `:help` and opening any of them work as
  before, and the help file now needs no conversion at all on the way in.
  Four files are deliberately left in their old encoding, because there the
  bytes are the content and not the text: `src/jptab.c` decides which way to
  convert by testing its own string literals for raw Shift-JIS bytes and walks
  them two at a time, and it generates the committed `jptab.h` that `kanji.c`
  and `track.c` compile in; `doc.j/vim32.ini` and `src/vim32s/vim32s.ini` are
  read with `GetPrivateProfileStringA`, where `fontname=ＭＳ ゴシック` has to
  be in the ANSI code page or the font is not found; and `src/vim32s/vim32s.rc`
  is a resource script compiled in that same code page. `doc/digraph.doc` keeps
  its bytes too — it is two tables of byte values in two different code pages,
  and says so itself. `src/inc/` is vendored and untouched.

### Fixed

- **`:help` displayed an empty screen, and leaving it ended the editor.** On the
  Windows GUI that is what choosing Help from the menu did: nothing appeared,
  and RETURN — which the help screen itself offers as the way out — took the
  process with it, unwritten buffers and no question asked. `kopen()` in
  `src/help.c` gave the conversion a destination the size of the help file, so
  every `helpfile` that was not already UTF-8 failed to convert: the shipped one
  was ISO-2022-JP, and turning its 32241 bytes into the internal UTF-8 needs
  33784, so `kanjiconvsfrom()` returned -1 every time. The -1 was stored as the
  text's length, which both emptied the screen and wrote a NUL one byte in front
  of the buffer, and the `free()` on the way out then fell over the damaged
  heap. The buffer is sized for what the conversion can produce now, and a
  conversion that fails says the help file could not be read instead of guessing.
  Every build was affected — the console one drew the same empty screen — but
  only the GUI, where the crash lands in the message loop, ended the session.
- The Windows GUI drew the help screen a line at a time and dropped the last
  byte of each line on the way, so the final character of every line was lost
  and a Japanese one came out as half of itself. On an empty line it wrote that
  NUL in front of `IObuff` and displayed the previous line again in place of the
  blank. Both are the same `IObuff[--col]`, which was there to drop a CR: it
  drops one only when one is present.
- **`:s///g` on a long line is no longer quadratic.** Two million substitutions
  on one line now take half a second where they took about twenty minutes; 400k
  characters went from 53 seconds to 0.11. `dosub()` allocated exactly what the
  next match needed and copied everything substituted so far into it, once per
  match, and measured the length of that with `STRLEN()` twice more. It now
  keeps the length and the allocated size and doubles the buffer, so the copying
  happens a handful of times instead of once per match. A minified `.js` or a
  one-line JSON dump is an ordinary file to be handed, which is what made this
  worth doing. The two `STRCPY()` calls in the same function that were given
  overlapping strings — undefined, and the sort of thing AddressSanitizer stops
  on — are `memmove()` now. (#22)
- The release notes said "the build and 129 tests passed on ." — a count from
  when the encoding and editing suites were the only two, and no platform at all.
  `release-notes.sh` adds up every suite the `test` target runs now, and reads
  the `needs:` of the release job rather than the first one in the workflow:
  `windows-runtime` is written above it and needs only `[windows]`, so the list
  it was given held no operating system. The header of that script says it exists
  because the notes used to be a heredoc that drifted exactly this way.
- **Syntax colouring made a long line quadratic, which is a minified page.**
  Opening a 31 kB `.html` with 17 kB of it on one line took 2.0 seconds; sending
  the cursor to the far end of that line took 19.5; dumping the colouring of the
  whole file took 89. They are 0.09, 0.32 and 1.8 now. A file of 800 tags on one
  line went from 73 seconds to one, and the Windows build opened the same 31 kB
  page and walked that line in 39 seconds where it now takes 1.6. Drawing asks `is_syntax()` for the colour
  of a byte, which asks every rule where its next match is — and asked again at
  every match in the line, so each of the thousands of matches on a minified line
  was another search of the whole of it by all eighty of `html.jvsyn`'s rules.
  Each rule now keeps the answer it last gave and searches again only once the
  drawing has passed it, which is one pass over the line per rule however many
  matches are in it, and the answers are dropped when the line changes or another
  line is drawn. A "no match" is kept only as far as the search that produced it
  really looked: a word rule matched as a string stops at the first place its
  letters appear and gives up if that one is inside a longer word, so keeping
  that as "nothing further on this line" lost every later `var` that was a word
  of its own. What is coloured has not changed — `:syntax dump` over the file
  this was found on is byte for byte what it was.
- **Every string rule in the tree ended a string at the wrong quote, so an empty
  `""` — or one ending in a backslash — drew the rest of the line as string.**
  The 50 rules were written `\".*[^\\]\"`: a quote, anything, one character that
  is not a backslash, a quote. That reads as "ends at a quote that is not
  escaped", and is not one. An empty `""` has no character between its quotes and
  `"C:\\"` has a backslash there, so neither could end where it does: the match
  ran on to the next quote on the line, and everything in between — the commas,
  the keywords, the numbers — came out coloured as string. `textmode ? "" :
  "[notextmode] "` in this tree's own `src/fileio.c` was one run from the first
  quote to the last; `if (s == "" || t == "y")` is the shape of it in any
  language. 35 of the 49 rule files carried it — 36 rules, and 14 more through
  the single quoted twin `'.*[^\\]'` — and a probe of every file type caught 32 of
  them drawing the rest of the line as string: C, Python, JavaScript, Go, Rust,
  Ruby, JSON, YAML, TOML, CSS, C#, PHP, Lua, Java, Kotlin, Swift, Perl, D, OCaml,
  SML, Scheme, Lisp, Clojure, Haskell, proto, Terraform, CMake, vim, awk and asm
  among them.
  They are `\"\([^\"\\]\\|\\.\)*\"` now — a quote, then any number of (one
  character that is neither a quote nor a backslash, or a backslash and whatever
  follows it), then a quote — which cannot reach past a closing quote, because
  neither branch matches one. A string holding an escaped quote, which is what
  the old form was written for, still works: `test-syntax.sh` now covers all
  three. The new form is also **8 times faster** on a 33 kB
  minified `.js` held on one line, since it gives up at the first character that
  cannot be in a string rather than scanning to the end of the line from every
  quote. `syntax/README` has the pattern to copy and says not to simplify it.
- **An HTML attribute value was found by counting quotes from the left, so one
  value that did not take part spoiled every value after it.** On a page built
  the way pages are built now — `<div class="flex items-center ...">` — the class
  list came out drawn as HTML, with `border`, `var` and `center` in it coloured
  as tag and attribute names, while the `>`, the `<div` and the `class` between
  two values were drawn as nothing at all. `syntax/html.jvsyn` matched a value as
  `\"[^#].*\"`, which starts at whichever quote the search reaches: that is the
  opening quote only while the quotes on the line pair up from the left, and a
  value the rule does not match — `href="#"`, an empty `""`, a value a tag broke
  a line inside of — puts every quote after it one out. The rule then matched
  from the *end* of one value to the *start* of the next. It is anchored at the
  `=` that introduces the value now, which cannot be a closing quote; the `=` is
  coloured with the value, there being no way to match text without colouring it.
  A tag broken across lines keeps this from spreading: the attributes after the
  break are coloured again, though the value itself is still not coloured over
  the break — that needs a region that opens only inside a tag, which the rule
  language cannot say, and it is in USAGE.md's known limits with the `<script>`
  body it is the same missing idea as.
- The rule for a single quoted HTML value was `\'.*\'` and was not confined to a
  tag, so it also matched from any apostrophe to the next: "It's fine, isn't it"
  came out coloured from the first apostrophe to the second. It is anchored at
  the `=` and confined to a tag now, like the double quoted one. A quote outside
  a tag means nothing in HTML, so nothing out there awaited it.
- **A regexp on a long line asked which byte of its character it was on the slow
  way.** `ISkanjiPointer()` was a wrapper on the offset form, which walks the
  string from the start to reach the byte, so a `*` or `\+` backing off over a
  long match paid the length of the line at every step. It has the byte, so it
  looks at that byte and at the three before it now — the search backwards for
  the start of a character stops after `UTF8_MAXLEN - 1` bytes, which also keeps
  a run of stray trailing bytes in a `-b` buffer from being walked to its
  beginning. This was most of those 20 seconds: 3.9 million calls of it, and
  19.85 of the 21.7 seconds a profile could account for.
- **`>>` with a large `shiftwidth` no longer locks the editor up.** `:set
  sw=10000000` then `>>` did not finish and could not be interrupted; it takes
  about a tenth of a second now. `set_indent()` inserted the indent one
  character at a time with `inschar()`, and deleted the old one a character at a
  time with `delchar()`, each of which rewrites the whole line — so the work was
  quadratic in the size of the indent, and neither loop called `breakcheck()`,
  which is what made a mistyped `:set sw=` unrecoverable rather than slow. The
  indent is built in one piece and put in place with one `ml_replace()` now.
  Ordinary `>>` was never affected: the work is quadratic in the indent, not in
  the file. (#29)
- Seven `malloc()` calls whose result was used without being checked now go
  through `alloc()` and handle failure: five in `src/syntax.c`, four of them
  followed immediately by `memset()` on what would have been NULL, and two in
  `src/tag.c` written through the same way. Out of memory is a message now
  rather than a crash, and only the rule or the tag being added is lost. The
  32 bit Windows build is where a long-lived editor can genuinely run out of
  address space. (#24)
- A syntax rule that asks for both `w` and a pair — `syntax Group wp/BEG/END` —
  built its end pattern from the pointer past the pattern rather than from the
  pattern, so it compiled as `\<\>`, the region never closed, and everything
  after the opening token was coloured to the end of the file. One word. No rule
  file in `syntax/` combines the two, so nothing bundled was affected. (#23)
- Two pieces of undefined behaviour in `src/memline.c`, found by the first UBSan
  run and reported 244 times over one pass of the test suites, with every case
  passing throughout. `DB_MARKED` shifted a plain `int` into its own sign
  bit (`1 << 31`) on every line fetched for the screen and every line appended;
  it is `(unsigned)1` now, as it is in Vim's own memline.c. And `ml_add_stack()`
  called `memmove()` with a NULL source and a length of zero the first time a
  buffer's stack grew — permitted-looking, undefined in fact, and enough licence
  for a compiler to drop a NULL check elsewhere.

### Known, and now written down

The hostile suite arrived with three cases the editor does not pass. Two of them
are the `:s` and `>>` fixes above. The third turned out not to be a bug, and is
written into the known limits instead (#30):

- **A byte that is not valid text is replaced by `?` on the way in, and saving
  writes the `?`.** The buffer holds only valid UTF-8, and every width
  calculation, cursor motion and character length in the editor is built on
  that, so bytes that are not characters cannot be carried through it. In a file
  that is mostly text with one bad byte, that byte is all that is lost — there
  is a case for that in `test-encoding.sh` now. In a file that is mostly not
  text, the character before a bad byte can go with it, and a sequence cut off
  by the end of the file comes back as an ASCII letter. `-b` round trips any
  file byte for byte and is the answer when the bytes matter more than the text;
  the hostile suite's KNOWN-FAIL and its passing `-b` twin are the two halves of
  that, and the KNOWN-FAIL stays.

### 日本語

- **CI が Windows の実行ファイルを実行するようになりました。** これまで自動で
  実行するものは一つもなく、コンパイルと、同じ移植部分のソースを Unix で
  テストするだけでした。起動するコンソール、`:r !` が呼ぶシェル、名前を開く
  ANSI ファイル API といった Windows 固有の部分は何にも守られておらず、
  `:r !cmd` は 2 回壊れたまま出荷されました。`scripts/test-winrun.sh` は
  スクリプト入力でエディタを動かします。通常のオペレータ、`:r !echo`、UTF-8 の
  往復、日本語のファイル名の 6 ケースで、対象は `windows` ジョブが作った
  パッケージの両アーキテクチャです（ランナー側でコンパイルしたものではなく。
  それは C ランタイムが別物になります）。手元でも WSL や Windows の Git Bash
  から実行でき、リリースのゲートにもなっています。届かないのはキー入力です。
  スクリプト入力はキーボードの符号変換より前に `inchar()` から戻るため、
  `scripts/test-winkeys.sh` の 16 ケース（実際にキーを打つ）は今も Windows 実機
  が必要です。
- **Unix 版が、そのまま続行できないシグナルを捕まえるようになりました。**
  Windows が 1.0.0 から `src/w32crash.c` でしていたのと同じことをします。
  どのシグナルだったかを表示し、スワップファイルを書き出して場所を示し、
  `jvim3 -r` を案内します。対象は SIGHUP・SIGQUIT・SIGILL・SIGTRAP・SIGABRT・
  SIGFPE・SIGBUS・SIGSEGV・SIGTERM です。これまでは `grep SIGSEGV src/*.c` が
  1 件も返さない状態で、異常終了や ssh セッションの切断（SIGHUP。これは
  クラッシュではありません）が、スワップファイルに届いていなかった分を
  持ち去り、端末を raw モードのまま残し、`-r` について何も言いませんでした。
  端末を先に戻し、スワップファイルは保全して**削除しません**。処理中に
  2 度目のシグナルが来たら、やり直さずその場で終了します。SIGSEGV と SIGBUS は
  `build-unix.sh asan` では AddressSanitizer に譲ります（そのレポートのほうが
  このメッセージより価値があります）。
- `scripts/test-hostile.sh` を追加しました。5 つ目のスイートで、誰も意図して
  いない入力や終わり方を与える 18 ケースです（他の 4 つは、受け付けるべき入力を
  与えます）。
  1 行 2 MB、あらゆるバイト値、不正な・途中で切れた UTF-8、`:syntax` への
  1015 文字のパターン（スタック上の `pattern[1024]` にコマンドラインから
  最も近づける長さ）、`:s` の 60 重ネストしたグループ、タブ幅 20 億、
  20 万行への `:g`、全トークンがコマンドラインに収まらない長さの配色スキーム、
  カラールールを読み込んだ状態で 1 行に 17 kB のタグ、編集中のセッション切断。
  17 件が通り、KNOWN-FAIL は 1 件で、下記の issue に対応します。
  入力ファイルは awk や dd ではなく `scripts/hostilegen.c` が作ります。5 つの
  OS で走るのに、それぞれの awk がゼロバイトの扱いで一致しないからです。
- `scripts/test-editing.sh` にケースを 2 件追加しました。置換文字列に改行を含む
  `:s`（そこで行が分かれる）と、CTRL-V で押さえた改行（本物の CR が本文に
  入る）です。下記で `dosub()` のその分岐を書き換えましたが、どちらも
  テストが一度も通っていませんでした。
- `scripts/test-syntax.sh` にケースを 2 件追加しました。ペアのルールと、それに
  `w` を足したルール（下記のバグ）です。
- `scripts/test-editing.sh` にもう 2 件。タブ幅より広いインデント（タブを並べ、
  残りをスペースで埋める。`expandtab` ならすべてスペース）を見ます。下記で
  書き換えた `set_indent()` が作るものです。
- `scripts/ptyrun.c` に `PTYRUN_SIGNAL` を追加しました。タイムアウト時に
  これまでの SIGKILL ではなく指定のシグナルを送り、そのあとも出力のコピーを
  続けるので、コマンドが終わる際に出力した内容と終了ステータスの両方が
  取れます。セッションが切れたときのエディタの挙動を試す唯一の方法です
  （バックグラウンドで起動したコマンドは pty を制御端末にできず、エディタは
  制御端末なしでは起動しません）。
- `scripts/test-encoding.sh` にケースを 2 件追加しました。ファイルが完全に
  1 つの符号ではない場合の自動判別で、不正なバイトが 1 個ある UTF-8 ファイルと、
  Shift-JIS のままであるべき Shift-JIS ファイルです。何かを直したのではなく、
  現在の挙動を固定するものです（下記の注記を参照）。合わせて 235 ケースに
  なりました。
- 敵性入力スイートは、エディタが通らない 3 件を持って入りました。うち 2 件は
  下記の `:s` と `>>` の修正です。残る 1 件は調べた結果バグではなく、既知の
  制限として書き下しました (#30)。**テキストとして正しくないバイトは読み込み時に
  `?` に置き換えられ、保存するとその `?` が書かれます。** バッファが持つのは
  妥当な UTF-8 だけで、幅の計算・カーソル移動・文字長のすべてがその前提の上に
  あるため、文字ではないバイトをそのまま通すことはできません。ほぼテキストの
  ファイルに不正なバイトが 1 個ある場合、失われるのはそのバイトだけです
  （`test-encoding.sh` にケースを追加しました）。ほぼテキストでないファイルでは、
  直前の文字まで一緒に失われることがあり、ファイル末尾で切れたシーケンスは
  ASCII 文字になります。バイトのほうが重要なときの答えは `-b` で、どんな
  ファイルでもバイト単位で往復します。敵性入力スイートの KNOWN-FAIL と、
  それに対応して通っている `-b` 版のケースがその両面で、KNOWN-FAIL は
  残します。
- `scripts/build-unix.sh asan` と `scripts/build-unix.sh ubsan` を追加しました。
  AddressSanitizer / UndefinedBehaviorSanitizer つきでビルドし、`test` と同じ
  スイートを実行します。CI でも push ごとに両方走ります。ASan はこれまで
  記憶を頼りに環境変数を並べて実行するだけのもので、UBSan は一度も走らせて
  いませんでした。どちらもサニタイザの出力を `log_path` でファイルに落として
  後から集めます。各スイートはエディタが書いたバイト列を比較するもので
  stderr は `/dev/null` に捨てているため、そのままではレポートも一緒に消える
  うえ、UBSan は検出しても実行を続けて 0 で終わるので、ジョブが緑のまま何かを
  見つけ続けることになります。
- **リポジトリ内の日本語ドキュメントをすべて UTF-8 にしました。** `:help` が
  読み、Windows パッケージが exe と並べて同梱する `doc.j/vim.hlp` も含みます。
  これまでは 2002 年当時の ISO-2022-JP で、エディタやブラウザに符号を教えない
  限り読めないものでした。`judge_jcode()` は JIS と同じように UTF-8 を判別する
  ので、`:help` も各ファイルを開く動作もこれまで通りで、ヘルプファイルに
  至っては読み込み時の変換自体が不要になります。
  次の 4 ファイルは意図的に元の符号のまま残しています。そこではバイト列が
  テキストではなく内容そのものだからです。`src/jptab.c` は自身の文字列リテラルを
  生の Shift-JIS バイトで判定して変換方向を決め、2 バイトずつ走査します
  （生成物の `jptab.h` はコミット済みで、`kanji.c` と `track.c` が取り込みます）。
  `doc.j/vim32.ini` と `src/vim32s/vim32s.ini` は `GetPrivateProfileStringA` で
  読まれ、`fontname=ＭＳ ゴシック` が ANSI コードページでないとフォントが
  見つかりません。`src/vim32s/vim32s.rc` は同じコードページでコンパイルされる
  リソーススクリプトです。`doc/digraph.doc` もバイトのまま残します。異なる
  2 つのコードページによるバイト値の表が 2 つ入っており、その旨がファイル自身に
  書かれています。`src/inc/` は他所から持ち込んだものなので触っていません。
- **`:help` が何も表示せず、抜けるとエディタごと終了していました。** Windows の
  GUI でメニューからヘルプを選ぶとまさにこれで、画面には何も出ず、ヘルプ自身が
  「抜けるにはこれ」と案内している RETURN でプロセスが落ちました。未保存の
  バッファがあっても確認は出ません。`src/help.c` の `kopen()` が符号変換の
  出力先をヘルプファイルと同じ大きさで渡していたためで、UTF-8 でない
  `helpfile` はすべて変換に失敗していました。同梱のものは ISO-2022-JP で、
  32241 バイトを内部の UTF-8 にすると 33784 バイトになるので、
  `kanjiconvsfrom()` は毎回 -1 を返していました。その -1 がテキストの長さとして
  保存され、画面が空になると同時にバッファの 1 バイト手前へ NUL を書き、
  終了時の `free()` が壊れたヒープの上で転んでいた、という筋です。変換が
  生成しうる大きさで確保するようにし、変換に失敗した場合は当て推量をせず
  ヘルプファイルを読めなかったと言うようにしました。影響は全ビルドに及び
  （コンソール版も同じ空の画面を描いていました）、セッションが終わるのは
  クラッシュがメッセージループに落ちる GUI だけです。
- Windows GUI はヘルプ画面を 1 行ずつ描く際に各行の最後の 1 バイトを捨てて
  いたため、行末の文字が失われ、日本語なら半分だけになっていました。空行では
  その NUL を `IObuff` の手前に書き、空行の代わりに前の行をもう一度表示して
  いました。どちらも CR を落とすための `IObuff[--col]` が原因で、CR がある
  ときだけ落とすようにしました。
- **長い 1 行に対する `:s///g` が二次オーダーではなくなりました。** 1 行
  200 万文字の全置換が、およそ 20 分から 0.5 秒になりました。40 万文字では
  53 秒から 0.11 秒です。`dosub()` は次のマッチに必要な分だけを確保して、
  それまでに置換した全体をそこへコピーする、というのをマッチごとに行い、
  さらにその長さを `STRLEN()` で 2 回測っていました。長さと確保済みサイズを
  持ち回り、バッファを倍々に伸ばすようにしたので、コピーはマッチごとではなく
  数回で済みます。minified な `.js` や 1 行の JSON ダンプは普通に開く
  ファイルであり、それがこの修正の理由です。同じ関数で重なった文字列を
  渡していた `STRCPY()` 2 箇所（未定義動作で、AddressSanitizer が止まる類の
  もの）も `memmove()` にしました。(#22)
- リリースノートが「the build and 129 tests passed on .」と書いていました。129 は
  エンコーディングと編集の 2 スイートしかなかった頃の数で、プラットフォーム名は
  空でした。`release-notes.sh` は `test` ターゲットが走らせる全スイートを合計し、
  ワークフローの最初の `needs:` ではなく release ジョブの `needs:` を読むように
  しました。`windows-runtime` がその上に書かれていて `[windows]` しか必要と
  しないため、渡されていたリストには OS が 1 つも入っていませんでした。この
  スクリプトの冒頭には、ノートがまさにこうしてずれていく heredoc だったから
  これがある、と書かれています。
- **シンタックスカラーが長い 1 行に対して二次オーダーで、それは minified な
  ページそのものでした。** 17 kB が 1 行に入った 31 kB の `.html` を開くのに
  2.0 秒、その行の末尾へカーソルを送るのに 19.5 秒、ファイル全体の色を
  ダンプするのに 89 秒かかっていました。それぞれ 0.09 秒、0.32 秒、1.8 秒に
  なりました。1 行に 800 個のタグが並ぶファイルは 73 秒から 1 秒になり、
  Windows 版が同じ 31 kB のページを開いてその行を歩くのにかかる時間は
  39 秒から 1.6 秒になりました。
  描画は 1 バイトの色を `is_syntax()` に尋ね、それが全ルールに「次のマッチは
  どこか」を尋ねます。これを行内のマッチごとに毎回尋ねていたので、minified な
  1 行にある数千のマッチのそれぞれで、`html.jvsyn` の 80 個のルールが行全体を
  もう一度探し直していました。各ルールは直前に返した答えを覚えておき、描画が
  そこを通り過ぎてから探し直すようにしたので、マッチが何個あってもルールごとに
  行を 1 回なめるだけになります。行が書き換わったとき、別の行を描き始めたときに
  答えは破棄します。「マッチなし」は、その探索が実際に見た範囲までしか
  覚えません。文字列として照合される `w` ルールは、字面が最初に現れた場所で
  止まり、そこが長い単語の一部なら諦めるからで、それを「この行にはもうない」
  として覚えると、それより後ろにある単独の `var` がすべて失われます。何が
  色づくかは変わっていません。これが見つかったファイルに対する
  `:syntax dump` は、以前とバイト単位で同一です。
- **リポジトリ内のすべての文字列ルールが文字列の終わりを誤った引用符で判定して
  いたため、空の `""`、あるいはバックスラッシュで終わる文字列があると、その行の
  残りが文字列として色付けされていました。** 該当する 50 個のルールは
  `\".*[^\\]\"` と書かれていました。引用符、任意の文字列、バックスラッシュ
  でない 1 文字、引用符です。これは「エスケープされていない引用符で終わる」と
  読めますが、そうではありません。空の `""` には引用符の間に文字がなく、
  `"C:\\"` はそこがバックスラッシュなので、どちらも本来の位置で終われず、
  マッチはその行の次の引用符まで伸びて、間にあるカンマ・キーワード・数値が
  すべて文字列の色になっていました。このリポジトリ自身の `src/fileio.c` にある
  `textmode ? "" : "[notextmode] "` は最初の引用符から最後の引用符までが
  1 つの区間になっていました。どの言語でも `if (s == "" || t == "y")` がその形
  です。49 個のルールファイルのうち 35 個がこれを持っていました（36 ルール。
  シングルクォート版の `'.*[^\\]'` でさらに 14 ルール）。全ファイルタイプを
  当たったところ、そのうち 32 個で行の残りが文字列として描かれていました。
  C、Python、JavaScript、Go、Rust、Ruby、JSON、YAML、TOML、CSS、C#、PHP、
  Lua、Java、Kotlin、Swift、Perl、D、OCaml、SML、Scheme、Lisp、Clojure、
  Haskell、proto、Terraform、CMake、vim、awk、asm などです。
  現在は `\"\([^\"\\]\\|\\.\)*\"` です。引用符、そのあと「引用符でも
  バックスラッシュでもない 1 文字、またはバックスラッシュとそれに続く 1 文字」
  の任意回の繰り返し、引用符。どちらの分岐も閉じ引用符にマッチしないので、
  閉じ引用符を越えることができません。古い形が想定していたエスケープされた
  引用符を含む文字列も従来どおり動きます。3 つとも `test-syntax.sh` に
  ケースがあります。新しい形は 1 行に収まった 33 kB の minified な `.js` で
  **8 倍高速**でもあります。引用符ごとに行末まで走査するのではなく、文字列に
  入りえない文字で打ち切るからです。コピーすべきパターンと「簡略化しないこと」は
  `syntax/README` に書きました。
- **HTML の属性値を左から引用符の数を数えて見つけていたため、数に入らない値が
  1 つあるとそれ以降の値がすべてずれていました。** いまふうに書かれたページ
  — `<div class="flex items-center ...">` — では、クラスの並びが HTML として
  描かれ、その中の `border`・`var`・`center` がタグ名や属性名の色になり、
  値と値のあいだにある `>`・`<div`・`class` には何の色も付きませんでした。
  `syntax/html.jvsyn` は値を `\"[^#].*\"` で照合していて、これは探索が到達した
  引用符から始まります。それが開き引用符であるのは、その行の引用符が左から
  対になっているあいだだけです。このルールが照合しない値 — `href="#"`、空の
  `""`、タグが途中で改行された値 — が 1 つあると、以降の引用符がすべて 1 つ
  ずれ、ルールはある値の*終わり*から次の値の*始まり*までを照合していました。
  値を導く `=` に錨を打つようにしました。`=` が閉じ引用符であることはあり得ま
  せん。色を付けずに照合する方法がないので、`=` は値と同じ色になります。
  タグが行をまたいでいてもここから崩れることはなくなり、改行の後ろの属性には
  また色が付きます。ただし値そのものは改行をまたいで色が付きません。それには
  「タグの内側でだけ開く領域」が必要で、ルール言語にその言い方はありません。
  `<script>` の中身と同じ欠けている概念として USAGE.md の既知の制限にあります。
- HTML のシングルクォート値のルールは `\'.*\'` で、タグの内側に限定されて
  いなかったため、任意のアポストロフィから次のアポストロフィまでも照合して
  いました。"It's fine, isn't it" が 1 つ目から 2 つ目まで色付きになる、という
  ことです。ダブルクォートのものと同じく `=` に錨を打ち、タグの内側に限定
  しました。HTML ではタグの外の引用符に意味はないので、外に探すものは
  ありません。
- **長い 1 行に対する正規表現が、いま何文字目のどのバイトかを遅い方法で
  尋ねていました。** `ISkanjiPointer()` はオフセット版の薄い包みで、その
  オフセット版はバイトに到達するために文字列の先頭から歩きます。そのため
  `*` や `\+` が長いマッチを 1 文字ずつ戻すとき、毎回行の長さぶんを払って
  いました。ポインタは手元にあるのですから、そのバイトと直前の 3 バイトだけを
  見るようにしました（文字の先頭を探す後方走査は `UTF8_MAXLEN - 1` バイトで
  打ち切るので、`-b` バッファにありうる後続バイトの連なりを先頭まで
  歩くこともなくなります）。上の 20 秒の大半はこれでした。呼び出し回数
  390 万回、プロファイルが説明できた 21.7 秒のうち 19.85 秒です。
- **大きな `shiftwidth` での `>>` がエディタを固めなくなりました。**
  `:set sw=10000000` のあと `>>` すると終わらず、中断もできませんでしたが、
  0.1 秒ほどで終わります。`set_indent()` は新しいインデントを `inschar()` で
  1 文字ずつ入れ、古いインデントを `delchar()` で 1 文字ずつ消していました。
  どちらも行全体を書き直すので、インデントの大きさに対して二次オーダーでした。
  さらにどちらのループにも `breakcheck()` がなく、`:set sw=` の打ち間違いが
  「遅い」ではなく「戻れない」になっていました。インデントを一度に組み立てて
  `ml_replace()` 1 回で置くようにしました。通常の `>>` は元から影響ありません
  （二次になるのはインデントの大きさに対してで、ファイルの大きさではない）。
  (#29)
- 戻り値を確認せずに使っていた `malloc()` 7 箇所を `alloc()` 経由にして、失敗を
  処理するようにしました。`src/syntax.c` に 5 箇所（うち 4 箇所は NULL に対して
  そのまま `memset()`）、`src/tag.c` に同じ書き方の 2 箇所です。メモリ不足が
  クラッシュではなくメッセージになり、失われるのは追加しようとしたルールか
  タグだけになります。長時間動かしたときにアドレス空間を本当に使い切りうるのは
  32bit の Windows 版です。(#24)
- `w` とペアを同時に指定したシンタックスルール（`syntax Group wp/BEG/END`）が、
  終了パターンをパターン自身ではなくその先を指すポインタから作っていました。
  `\<\>` としてコンパイルされるため領域が閉じず、開始トークン以降がファイル末尾
  まで色付けされていました。修正は 1 語です。`syntax/` の同梱ルールにこの
  組み合わせはないので、同梱分への影響はありません。(#23)
- `src/memline.c` の未定義動作 2 件を修正しました。初回の UBSan 実行で、
  テスト 1 周につき 244 回報告されたものです（その間、全ケースが
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
  Still nothing to override on a plain terminal, which never had a
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
  handed `set fexrc` and a syntax rule set. No rc existed that a Unix
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
  Two lists of the same flags could have drifted apart as well; now they are
  one list, the one anybody can run before pushing.

### Fixed

- The Windows package carries Windows line separators. `dosource()` opens a
  sourced file in binary mode and takes one trailing CR off each line, warning
  `Wrong line separator, ^M may be missing` when none is present — and the
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
  skipped over otherwise — they did not change, so nothing needs drawing.
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
  must not be, and `set_term()` held 19 of them. It is one macro now,
  casting to `char **`, which is what Linux's `<termcap.h>` and the BSDs' curses
  both declare. This is also what the 58 warnings BUILDING-unix.md recorded for
  NetBSD were, and it said they were not worth 58 casts to be rid of: they were
  worth one. The NetBSD guest now builds with no warnings at all.
- The `ansi` terminal built into JVim — used without a termcap/terminfo
  library to fall back on, or when `$TERM` names it directly — had `t_el` (clear to
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
- macOS from CI: nobody here has one to try, so a failure there became a puzzle
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
