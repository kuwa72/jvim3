# Changelog

JVim 3's own version numbers start at 1.0.0. "JVim 3.0-j2.1b" is where this tree
came from, not a version of it: that release is Tsuchida Ken'ichi's, from
December 2002, and it does not change again.

English first in each section, 日本語 after it. One file rather than two, because
a `CHANGELOG.ja.md` beside this would be the highest-churn document pair in the
repository and would drift within three releases.

## Unreleased

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
