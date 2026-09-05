# JVim 3 と現行 Vim / Neovim の違い

[English](DIFFERENCES.md) | **日本語**

JVim 3 は、Bram Moolenaar 氏による **Vim 3.0**（1994年）を土田健一氏が日本語化した **JVim 3.0-j2.1b**（2002年）をベースに、内部バッファの完全 UTF-8 化や現代の 64-bit OS / Unicode Win32 GUI への対応などを行ったエディタです。

現代の Vim 9.x や Neovim とは 30 年以上の世代差があります。軽量・高速・シンプルである一方、現代の Vim/Neovim に存在する多くの拡張機能はあえて実装されていません。本ドキュメントでは、現代の Vim / Neovim から移行または併用するユーザー向けに「何ができて、何ができないか」を整理しています。

---

## 主な差分・機能対照表

| 分野 | JVim 3 | 現行 Vim (9.x) / Neovim |
|---|---|---|
| **コアサイズ・起動速度** | 実行ファイル約 1MB、瞬時に起動（0ms） | 数十MB〜数百MB（プラグイン構成による） |
| **内部エンコーディング** | **UTF-8 固定**（ファイル入出力時に変換） | `encoding=utf-8`（設定可能） |
| **プラグイン機構** | **なし**（実行ファイル単体完結） | あり（Vim9 script, Lua, RPC, Remote Plugin） |
| **スクリプト言語** | **なし**（条件分岐・ループ・関数定義なし） | Vim script / Vim9 script / Lua |
| **LSP / Tree-sitter** | **なし** | あり（Neovim 標準 / Vim プラグイン） |
| **非同期ジョブ / ターミナル** | **なし**（`:!` による同期的外部実行のみ） | `:terminal`、`job_start()` 等の非同期処理 |
| **ウィンドウ分割** | 水平分割（`:split`）のみ | 水平・垂直分割（`:split`, `:vsplit`） |
| **タブページ** | **なし** | あり（`:tabnew` 等） |
| **Undo 履歴** | 多段 Undo（線形、`undolevels`） | Undo Tree（分岐履歴・永続 Undo） |
| **正規表現** | 基本正規表現（Henry Spencer 実装）+ 拡張クラス | 独自エンジン（NFA/バックトラック）、Perl互換機能 |
| **シンタックスハイライト** | 独自のルールファイル形式（`syntax/*.jvsyn`） | Vim 標準シンタックス / Tree-sitter |
| **カラースキーム** | `:colorscheme` 対応（GUI / 24-bit TrueColor SGR） | 対応 |
| **Quickfix** | あり（`-e`, `:cn`, `:cp`, `:cl`） | あり（エラーリスト複数保持、location-list 等） |
| **キーボードマクロ** | あり（`qa ... q`, `@a`） | あり |
| **レジスタ / マーク** | あり（名前付きレジスタ `a-z`, マーク `'a`） | あり |
| **IME 制御** | コマンドモード連動の IME 制御（Windows GUI） | プラグインまたは外部ツール依存 |

---

## JVim 3 に「ない」もの（できないこと）

1. **プラグインマネージャー・拡張言語**
   - Vim script の関数定義（`function`）、ループ（`for`, `while`）、辞書・リスト型、Vim9 script、Lua は使えません。
   - `.vimrc` は主として `set`, `map`, `hi`, `colo`, `source` などの宣言的設定を記述します。
2. **LSP (Language Server Protocol) および Tree-sitter**
   - 補完はローカルバッファ内の単語補完（`CTRL-N` / `CTRL-P`）および ctags（`:tag`, `CTRL-]`）を用います。
3. **垂直ウィンドウ分割（`:vsplit`）とタブ（`:tabnew`）**
   - ウィンドウ分割は水平分割（`:split`, `CTRL-W s`, `CTRL-W j/k` 等）のみです。
4. **ターミナルエミュレータ（`:terminal`）**
   - シェルコマンドは `:!cmd` またはフィルタ `!motion cmd` で実行します。
5. **ビジュアルモードの一部**
   - 矩形選択（`CTRL-V`）や行選択（`V`）などの現代的ビジュアルモードはなく、オペレータとモーション（`d}`, `y$` など）やマークによる範囲指定、Ex コマンド（`:'<,'>...` ではなく `:10,20...`）を使用します。
6. **複雑な正規表現**
   - 先読み・後読み（lookaround）や非貪欲マッチ（`\{-} `）、非常に複雑なパターンはサポートしていません。

---

## JVim 3 で「できる」もの（強み・特長）

1. **ゼロ構成・極小のフットプリント**
   - 外部依存ライブラリがなく、単一バイナリ（約 1MB）で動作します。設定なしでも即座に快適に動作します。
2. **完全な UTF-8 内部保持**
   - オリジナルの JVim（Shift-JIS ベース）と異なり、バッファ内部をすべて UTF-8 で保持します。絵文字、ハングル、サロゲートペア文字なども破損せずに編集できます。
   - ファイルの入出力は Shift-JIS (CP932)、EUC-JP、ISO-2022-JP、UTF-8 を自動判定し、相互変換が可能です。
3. **最新 Windows への適合（Unicode Win32 GUI）**
   - Per-Monitor DPI 対応、Unicode API 完全移行、IME 自動制御（挿入モードを抜けると自動で IME OFF など）が組み込まれています。
4. **最新の配色テーマ（24-bit TrueColor）**
   - GUI だけでなく現代的な 24-bit TrueColor 端末上でも、Dracula、Gruvbox、Nord、TokyoNight などのモダンなカラースキームが美しく表示されます。
5. **vi / Vim 3.0 の軽快な編集体系**
   - 多段 Undo、水平分割、ctags ジャンプ、Quickfix、キーボードマクロ、コマンドライン補完・履歴など、実用的なテキスト編集に必要な機能は過不足なく揃っています。

---

## .vimrc / 設定ファイルの互換性

現代の Vim/Neovim の複雑な `~/.vimrc` や `init.lua` はそのままでは読み込めません。JVim 3 は専用の `~/.jvimrc`（Windows では `_jvimrc`）を優先して読み込みます。

- 基本的な `:set` オプション（`number`, `autoindent`, `tabstop`, `shiftwidth`, `expandtab`, `ignorecase` 等）は共通して利用可能です。
- キーマッピング（`:map`, `:map!`, `:imap`）は `<CR>`, `<Esc>`, `<Tab>` などの特殊キー表記に対応しています。
- おすすめの設定例は [USAGE.ja.md](USAGE.ja.md#出発点になる-_vimrc) を参照してください。

---

## ドキュメント一覧

- **[README.ja.md](README.ja.md)**: プロジェクト概要と導入手順
- **[USAGE.ja.md](USAGE.ja.md)**: 詳細な使い方、文字コード、配色テーマ、制限事項
- **[doc/difference.doc](doc/difference.doc)** (日本語: [doc.j/differen.doc](doc.j/differen.doc)): オリジナル Vim 3.0 と vi の詳細な差分
- **[doc/reference.doc](doc/reference.doc)**: Vim 3.0 リファレンスマニュアル
