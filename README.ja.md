# JVim 3 — 日本語化 Vim 3.0 を UTF-8 で

[English](README.md) | **日本語**

JVim 3 は、Bram Moolenaar の Vim 3.0 を土田健一さんが日本語化したエディタです。
最終版 3.0-j2.1b は 2002 年 12 月のものです。このリポジトリはそれを現代のシステ
ムで動くようにしたもので、内部にひとつ大きな変更を入れています。**バッファは
UTF-8 を保持する**ようになりました。Shift-JIS ではありません。CP932 に席のない文字 —
ハングル、アクセント付きラテン文字、絵文字、BMP 外の文字 — が、読み込み・編集・
保存を通って壊れずに残ります。

中身は 1994 年のエディタです。1 MB 程度の実行ファイル 1 個、プラグインも
スクリプト言語も起動待ちもなし。vi のキー操作に、Vim 3.0 が足したぶんだけ —
多段 undo、複数ウィンドウ、コマンドライン履歴と補完、シンタックスカラー、
そしてコマンドモードを理解している IME 制御。

[![最新リリース](https://img.shields.io/github/v/release/kuwa72/jvim3?label=%E6%9C%80%E6%96%B0%E3%83%AA%E3%83%AA%E3%83%BC%E3%82%B9)](https://github.com/kuwa72/jvim3/releases/latest)
JVim 3.0-j2.1b (2002 Dec 24) 由来

```
対応環境       Windows 10/11 (Win32 GUI + コンソール)、Linux、
               FreeBSD、NetBSD、OpenBSD、DragonFly
テスト         269 ケース。上記すべてで CI が実行。Windows のキー入力は
               scripts/test-winkeys.sh の 16 ケース (WSL から実機で実行)。
               Windows 版の実行そのものは CI で 6 ケース
ライセンス     パブリックドメイン — LICENSE を参照。付随する寄付のお願いは
               uganda.txt にあります
```

## このリポジトリで変わったこと

| | |
| --- | --- |
| 内部 UTF-8 | バッファは UTF-8。変換は端 (ファイル、端末、GUI、キーボード、クリップボード、パイプ) だけで行います。1 文字は 1〜4 バイト、幅は 1〜2 桁。「必ず 2 バイト 2 桁」という前提はなくなりました。 |
| Unicode の Win32 GUI | `RegisterClassW` のウィンドウ、`ExtTextOutW` による描画、`CF_UNICODETEXT` のクリップボード、`SetWindowTextW` のタイトル、UTF-16 の `WM_CHAR` を UTF-8 に組み直す入力。ダイアログ・メニュー・フォント名・レジストリも明示的に UTF-16 で扱います。 |
| CP932 外のファイル名 | マニフェストでプロセスのコードページを UTF-8 に指定しているので、`...A` 系のファイル API が UTF-8 を受け取り、`🍣.txt` が開けます。 |
| 画面スケーリング | プロセスをモニタ単位 DPI 対応にし、保存済みのフォントサイズ・ウィンドウサイズを目の前の DPI に読み替えます。125%・150% でも文字がぼやけず、別倍率のモニタへ移動しても保たれます。 |
| Unix 系ならビルドできる | `scripts/build-unix.sh` が、makefile の 3 行をコメントアウトさせる代わりに、コンパイラに環境を問い合わせます。`scripts/build-mingw.sh` は mingw-w64 で Windows 版をクロスビルドします。 |
| 269 個のテスト | エンコーディング 51 ケース、編集 103 ケース、シンタックスカラー 88 ケース、端末に実際に送られるエスケープを見る 9 ケース、そして誰も意図していない入力や終わり方を与える 18 ケース（1 行 2 MB、あらゆるバイト値、ファイル末尾で切れたマルチバイト列、編集中にセッションが切れる）。いずれも本物の pty 越しです。push ごとに 5 つの OS で実行され、さらに AddressSanitizer と UndefinedBehaviorSanitizer の下でも走ります。 |

| 長年のバグを修正 | 15 件。一覧は [BUILDING-mingw.md](BUILDING-mingw.md#bugs-found-along-the-way) にあります。正規表現の `[あ]` が `い` にもマッチする、コマンドラインが `buff[-1]` を読む、絵文字 1 個でファイル全体が Shift-JIS と誤判定される、端末入力で 2 回の読み込みにまたがった文字が化ける、など。 |
| 配色テーマ | `:colorscheme` と Vim 互換の `:highlight`、同梱の 16 テーマ。GUI でも端末の SGR でも使えます。詳細は [USAGE.ja.md](USAGE.ja.md#配色テーマ)。 |
| 削除した機能 3 つ | BDF フォント描画、書庫 (LHA/ZIP/TAR) 内のファイル編集、MIME / uuencode / base64 デコードをソースごと削除しました。再配布の条件が扱いにくかったためです。[後述](#ライセンス)。 |

## 入手する

### Windows

[リリースページ](https://github.com/kuwa72/jvim3/releases)から zip を取って、好きな
場所に展開してください。インストール作業はありません。`%VIM%` が未設定なら実行
ファイルのあるディレクトリを `%VIM%` として扱うので、exe の隣に置いたヘルプファイル
や `_vimrc` はそのまま見つかります。

| パッケージ | 中身 | |
| --- | --- | --- |
| `jvim3-*-win32.zip` | `jvim32w.exe`, `jvim32.exe` | 32 ビット。64 ビット Windows でも WoW64 で動きます。 |
| `jvim3-*-win64.zip` | `jvim64w.exe`, `jvim64.exe` | ネイティブ 64 ビット。64 ビットとわかっているマシン向けです。 |

`jvim32w.exe` が GUI 版、`jvim32.exe` がコンソールから起動する版です。後者に
`-nw` を付けるとウィンドウを開かずコンソールで動きます。

パッケージには `vim.hlp` (JVim の日本語ヘルプ。そのままの名前で `:help` が見つけ
ます)、シンタックスルールの `syntax/`、そして出発点になる rc が 2 つ入っています。
`jvimrc.sample` は短く Unix ビルドでもそのまま使えるもの、`_jvimrc.sample` は
従来からある Windows 用のものです。初期設定は
[USAGE.ja.md](USAGE.ja.md#windows-での最初の設定) にあります。

### Linux、FreeBSD、NetBSD、OpenBSD、DragonFly

```sh
git clone https://github.com/kuwa72/jvim3
cd jvim3
./scripts/build-unix.sh test        # src/jvim3 をビルドして 269 個のテストを実行
```

必要なのは C コンパイラと、本物の端末データベースを使うための curses / termcap
ライブラリです (Debian・Ubuntu なら
`libncurses-dev`、BSD には最初から入っています)。スクリプトは何を見つけ
たかを表示します。

macOS でもビルドは通りますが、未検証です。
CI では扱っていません ([BUILDING-unix.md](BUILDING-unix.md) の未検証の項を参照)。

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

手でインストールする場合:

```sh
sudo install -m 755 src/jvim3           /usr/local/bin/
sudo install -m 644 doc.j/vim.hlp       /usr/local/lib/jvim3.hlp   # 英語版は doc/vim.hlp
sudo install -m 644 doc/vim.1           /usr/local/man/man1/jvim3.1
```

これがビルド時に埋め込まれるパスです。ディストリビューションのパッケージはまだ
ありません。作った方は知らせてください。ここからリンクします。

### Windows 版を自分でビルドする

```sh
sudo apt install gcc-mingw-w64-i686-win32   # Linux または WSL からクロスビルド
./scripts/build-mingw.sh both               # dist/i686/jvim32w.exe + jvim32.exe
```

MSYS2 の **MINGW32** シェルなら `pacman -S mingw-w64-i686-gcc make` を入れて同じ
スクリプトを実行します。このパッケージを使うのは、ビルドが msvcrt に対して行われる
必要があるためです。`mingw-w64` メタパッケージは UCRT 対象なので使えません。スクリプトはそうでない
ツールチェインを拒否します。詳細は [BUILDING.ja.md](BUILDING.ja.md) と
[BUILDING-mingw.md](BUILDING-mingw.md)。

## ドキュメント

このリポジトリのドキュメント:

| | |
| --- | --- |
| [USAGE.ja.md](USAGE.ja.md) / [USAGE.md](USAGE.md) | 起動方法、設定ファイルの場所、文字コードの扱い、IME、画面表示、トラブルシューティング。**まずここから。** |
| [DIFFERENCES.ja.md](DIFFERENCES.ja.md) / [DIFFERENCES.md](DIFFERENCES.md) | 現行 Vim / Neovim との差分ガイド。残されている機能と削られている機能。 |
| [BUILDING.ja.md](BUILDING.ja.md) | Unix / Windows 両方のビルド手順 (日本語)。 |
| [BUILDING-unix.md](BUILDING-unix.md) | Linux・BSD でのビルド。スクリプトが何を検出するか、CI が何を見ているか、何が検証済みで何がそうでないか (英語)。 |
| [BUILDING-mingw.md](BUILDING-mingw.md) | Windows 版のビルドと、UTF-8 化・Unicode GUI・DPI 対応・1 行の描画が実際にどう動いているかの詳しい話 (英語)。 |

JVim 自身のマニュアル (`doc.j/`、UTF-8):

| | |
| --- | --- |
| [doc.j/readme.doc](doc.j/readme.doc) | JVim 3.0-j2.1b の説明書。増えているオプション・コマンド、syntax の設定、tips。 |
| [doc.j/differen.doc](doc.j/differen.doc) | vi との違い (difference.doc の日本語版)。 |
| [doc.j/fepctrl.doc](doc.j/fepctrl.doc) | FEP/IME 制御について。 |
| [doc.j/vim.hlp](doc.j/vim.hlp) | `:help` の日本語版。 |

Vim 3.0 のマニュアル (`doc/`、1994 年、英語):

| | |
| --- | --- |
| [doc/reference.doc](doc/reference.doc) | 全コマンド・全オプション。リファレンス。 |
| [doc/difference.doc](doc/difference.doc) | vi に対して Vim が足したもの、違うところ。 |
| [doc/windows.doc](doc/windows.doc) | 複数ウィンドウとバッファ。 |
| [doc/index](doc/index) | コマンドのアルファベット順一覧。 |
| [tutor/tutor](tutor/tutor) | vi が初めての人向けの 1 時間コース。 |
| [README](README) | Vim 3.0 自身の README (1994 年)。そのまま残しています。ここに書かれたビルド手順は、このリポジトリが置き換えたものです。 |

`doc.j/` の中で MS-DOS、Windows 95、BOW、書庫内編集、BDF フォントについて書いて
ある箇所は、もう手順ではありません。あくまで歴史です。

## 現状と限界

CI で Windows 版にキーを打ち込むものはありません。実行自体はするようになり
ましたが、スクリプト入力経由のみで、カーソルキーには到達できません。実際に
キーを打つスイートは Windows 実機を必要とし、手動で実行しています。Windows の
コンソールモードでの日本語入力は不安定、GDI はカラー絵文字を描かず、一部の
文字コード変換は一方向です。全項目とそれぞれの実際の影響は
[USAGE.ja.md](USAGE.ja.md#既知の制限) にまとめてあります。

## テスト

```sh
./scripts/build-unix.sh test           # ビルドして 5 つのスイートを実行
./scripts/test-encoding.sh src/jvim3   # 51 ケース: 文字コード、マルチバイト編集
./scripts/test-editing.sh  src/jvim3   # 103 ケース: 移動、オペレータ、レジスタ、

                                       #   マーク、undo、ex の範囲指定、:g、:s、:!
./scripts/test-syntax.sh   src/jvim3   # 88 ケース: syntax/ が実際に何を色付けするか
./scripts/test-sgr.sh      src/jvim3   # 9 ケース: 端末に実際に送られるエスケープ
./scripts/test-hostile.sh  src/jvim3   # 18 ケース: 誰も意図していない入力
```

いずれも本物の pty (`scripts/ptyrun.c`) 越しにエディタを動かしてバイト列を比較
するので、人が打つのと同じ入力経路を通ります。各ケースには 20 秒の制限があり、
キー入力待ちで止まったケースはスイートを固めずに失敗します。これは、何かが
終わらないほど遅くなったことを敵性入力スイートが検出する仕組みでもあります。

push と pull request のたびに、Linux・FreeBSD・NetBSD・OpenBSD・DragonFly の 5
環境で 269 ケースすべてを実行し、Windows 版を両アーキテクチャでクロスビルド


します。`v*` のタグはそれに加えて Windows の zip を公開するので、壊れたビルドが
リリースになることはありません
([.github/workflows/build.yml](.github/workflows/build.yml))。

## 開発に参加する

Issue と pull request は <https://github.com/kuwa72/jvim3> へ。日本語で構いません。

取り込みやすい変更の条件は 2 つです。`./scripts/build-unix.sh test` が通ること、
そして CI がエラー扱いにしている警告を増やさないこと。

特に助かるのは、64 ビット Windows 版を実際に動かしてみること、実機の IME で使って
みて壊れたところを報告してくれることです。

残りは [CONTRIBUTING.ja.md](CONTRIBUTING.ja.md)
([English](CONTRIBUTING.md)) にあります。警告の一覧、テストケースの追加方法、
コミットの書き方、そして非公開のセキュリティ窓口を置いていない理由です。

## ライセンス

Vim 3.0 は**パブリックドメイン**です。そのうえで Bram Moolenaar は、気に入ったら
ウガンダの Kibaale Children's Centre に寄付してほしいと書きました
(charityware)。本人の文章が [uganda.txt](uganda.txt)、日本語訳が
[doc.j/uganda.jp](doc.j/uganda.jp) です。この呼びかけは今も有効で、現在の寄付方法
は [Vim の ICCF のページ](https://www.vim.org/iccf/)にあります。

土田健一さんは日本語化部分の**著作権を放棄**しています (`doc.j/readme.doc` §12)。
希望として挙げられているのは、ソースを使ったら公開してほしいということだけで、
一切の保証はしないと明記されています。ここでも同じです。**このソフトウェアには
いかなる保証もありません。**

上記の条件に含まれていなかった 2 つのディレクトリは、このリポジトリから削除しま
した。`src/bdf/` (GPL での配布が必要だが、ファイル自身にライセンス表記がない) と
`src/exfile/` (使用時に著者への連絡が必要、つまり自由なライセンスではない) です。
残っているのは、Vim 3.0 のパブリックドメインと、著者が権利を放棄した日本語化部分
だけです。削除したのは作業ツリーからで、git の履歴からではありません。

[LICENSE](LICENSE) が以上を機械可読にしたものです。Moolenaar と土田さんが実際に
書いたことに最も近い標準テキストであり、かつ GitHub が認識できるものとして
Unlicense を置いています。上に書いた寄付の呼びかけはお願いであって、利用条件では
ありません。

## クレジット

Vim 3.0 は **Bram Moolenaar** 作。元は Tim Thompson、Tony Andrews、G. R. Walter の
Stevie です。全一覧は [credits.txt](credits.txt) にあります。

日本語化は **土田健一**さん。**小笠原博之**さんの Vim 3.0 用日本語化パッチと
**中村敦司**さんの Vim 2.0 用パッチをベースに、Onew メーリングリストの方々の協力
のもとで作られました。IME 制御には**太田純**さんの FEPCTRL ライブラリを使ってい
ます。

このリポジトリでの UTF-8 化、Unicode GUI、ビルドとテストのスクリプトは
[kuwa72](https://github.com/kuwa72) によるものです。
