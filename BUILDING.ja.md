# JVim 3 のビルド

日本語 | 英語版は [BUILDING-unix.md](BUILDING-unix.md) (Unix) と
[BUILDING-mingw.md](BUILDING-mingw.md) (Windows)。手順はこちらに、なぜそうなって
いるかの詳しい話は英語版にあります。

- [Unix (Linux, macOS, BSD)](#unix-linux-macos-bsd)
- [Windows (mingw-w64)](#windows-mingw-w64)
- [テスト](#テスト)
- [Linux から BSD を確認する](#linux-から-bsd-を確認する)
- [CI が見ているもの](#ci-が見ているもの)
- [ビルドに必要なフラグと警告の方針](#ビルドに必要なフラグと警告の方針)

## Unix (Linux, macOS, BSD)

```sh
./scripts/build-unix.sh          # src/jvim3 をビルド
./scripts/build-unix.sh test     # ビルドしてテスト 221 ケースを実行
./scripts/build-unix.sh strict   # CI がエラー扱いする警告つきでビルド
./scripts/build-unix.sh asan     # AddressSanitizer つきでビルドしてテスト
./scripts/build-unix.sh ubsan    # UndefinedBehaviorSanitizer つきでビルドしてテスト
./scripts/build-unix.sh install  # PREFIX (既定: /usr/local) へビルド＆インストール
./scripts/build-unix.sh clean
```

`strict` は CI が強制する `-Werror=` 群でビルドするので、push の前に自分で
確認できます。CI はこれを Linux (gcc) と FreeBSD (clang) の両方で走らせます。
clang は古い形式の関数定義や型の合わないキャストをエラー扱いし、gcc は警告
で済ませることがあるため、gcc で緑でも clang で緑とは限りません。

必要なものは C コンパイラだけです。curses / termcap ライブラリがあれば本物の端末
データベースを使い、なければ JVim 組み込みの端末定義にフォールバックします
(Debian・Ubuntu なら `libncurses-dev`。macOS と BSD には最初から入っています)。

`src/makjunix.mak` は今も、環境に合わせて 3 行を手でコメントアウトすることを前提に
しています。スクリプトは同じ答えをコンパイラに問い合わせて求め、makefile には
コマンドラインから渡します。makefile 自体は元のままです。決定内容は環境変数で
上書きできます。

```sh
CC=clang OPT="-O0 -g" EXTRA_CFLAGS=-I/usr/local/include \
	EXTRA_LIBS=-L/usr/local/lib ./scripts/build-unix.sh
```

POSIX `sh` で書かれていて `make -C` を使わないので、BSD の `/bin/sh` と `bmake`
でも、bash と GNU make でも動きます。

スクリプトが検出するもの:

| | |
| --- | --- |
| `-DUSE_LOCALE` | `setlocale()` があるか。これがあると `LANG` が文字コードの既定値を決めます。`ja_JP.UTF-8` なら `jmask=TTTT`、`ja_JP.eucJP` なら `EEEE`。なければ既定は `EEET` (表示は EUC、新規ファイルは UTF-8)。 |
| `-DTERMCAP` + ライブラリ | `-ltinfo -lncursesw -lncurses -lcurses -ltermlib -ltermcap` のうち `tgetent()` を提供する最初のもの。ただし `<termcap.h>` か `<curses.h>`+`<term.h>` もある場合。どちらもなければ `-DALL_BUILTIN_TCAPS` (組み込みの端末定義) にフォールバックし、何もリンクしません。 |
| `-DUSE_X11` | `<X11/Xlib.h>` と `-lX11` の両方が使えるか。xterm のタイトルの保存と復元にしか使いません。 |
| `-DHAVE_MKSTEMP` | `mkstemp()` がリンクできるか。`:!` とワイルドカード展開の一時ファイルはこれで作ります。なければ名前を決めるだけの `mktemp()` にフォールバックします。 |
| マシン | Linux・BSD・macOS には `-DBSD_UNIX`、BSD と macOS にはさらに `-DBSD4_4` を付けて `unix.c` が `<sgtty.h>` ではなく `<termios.h>` の経路を通るようにします。SunOS と AIX には `-DSYSV_UNIX` 系。 |

### Linux ローカル環境への展開（動作確認・検証用）

ローカルで手軽に動作検証するためにユーザー領域（`~/.local` など）へ展開する場合:

```sh
tools/deploy-local.sh              # ~/.local へビルド＆インストール
tools/deploy-local.sh /path/to/dir # 任意のプレフィックスへ展開
```

`$HOME/.local/bin/jvim3`、シンタックス定義（`$HOME/.local/lib/jvim3/syntax/`）、ヘルプファイル（`$HOME/.local/lib/jvim3.hlp`）等が正しく設定・配置されます。

### システムへのインストール

```sh
sudo ./scripts/build-unix.sh install
```

または手動で配置する場合:

```sh
sudo install -m 755 src/jvim3           /usr/local/bin/
sudo install -m 644 doc.j/vim.hlp       /usr/local/lib/jvim3.hlp   # 英語版は doc/vim.hlp
sudo install -m 644 doc/vim.1           /usr/local/man/man1/jvim3.1
sudo mkdir -p /usr/local/lib/jvim3/syntax
sudo install -m 644 syntax/*.jvsyn      /usr/local/lib/jvim3/syntax/
```

`/usr/local/etc/jvim3rc` があれば、システム共通の設定として何より先に読まれます。

## Windows (mingw-w64)

元の Windows ビルド (`src/makjnt.mak`) は MS SDK と `nmake` を必要とします。ここで
追加したのは mingw-w64 による最小構成のビルドで、コアのエディタ、Win32 GUI、
漢字/UTF-8 のファイル入出力が入っています。

Linux / WSL からクロスビルドする場合:

```sh
sudo apt install gcc-mingw-w64-i686-win32 gcc-mingw-w64-x86-64-win32
./scripts/build-mingw.sh both       # jvim32w.exe (GUI) + jvim32.exe (コンソール)
```

`mingw-w64` メタパッケージや Homebrew の `mingw-w64` ではなくこのパッケージ名です。
**ビルドは msvcrt に対して行う必要があり**、UCRT のビルドはローカルでも CI でも
行いません。mingw-w64 がどちらの C ランタイムを対象にするかはツールチェイン自体を
ビルドした時点で決まる (ヘッダが `_UCRT` を定義するかどうか、`libmingwex` もそれに
合わせてコンパイル済み) ため、コンパイラのフラグでは変更できません。Debian の
`gcc-mingw-w64-*-win32` は msvcrt、Homebrew の `mingw-w64` は UCRT です。

`scripts/build-mingw.sh` は使おうとしているツールチェインが msvcrt かどうかを
確認し、そうでなければ (問い合わせに失敗した場合も) ビルドを拒否します。
`MINGW_BIN` も `CROSS` も未設定なら、`PATH` → `/usr/bin` → MSYS2 のネイティブ
`gcc` の順に試して最初に見つかった msvcrt のものを使うので、`PATH` の先頭に UCRT の
ツールチェインがあっても影響しません。明示する場合:

```sh
MINGW_BIN=/usr/bin ./scripts/build-mingw.sh both                  # ディレクトリ
CROSS=/usr/bin/i686-w64-mingw32- ./scripts/build-mingw.sh both    # 正確な接頭辞
```

両アーキテクチャをビルドする `release` では `MINGW_BIN` を使ってください。`CROSS`
は片方のアーキテクチャを指すもので、`release` が起動する子ビルドにも引き継がれ、
64bit のビルドに 32bit のコンパイラを渡してしまいます。これはパッケージ化せずに
拒否されます。

なぜ msvcrt なのか、`tmpnam()` で実際に何が起きたかは
[BUILDING-mingw.md](BUILDING-mingw.md) の "Which C runtime" にあります。

MSYS2 の **MINGW32** シェルでネイティブにビルドする場合:

```sh
pacman -S mingw-w64-i686-gcc make
./scripts/build-mingw.sh both
```

出力は `dist/i686/` に、`vim.hlp` と `_jvimrc` のサンプルと一緒に置かれます。この
ディレクトリを Windows 側にコピーしてください (`%VIM%` は未設定なら実行ファイルの
ディレクトリになるので、`:help` は隣の `vim.hlp` を見つけます)。

配布用には `release` が両アーキテクチャをビルドして zip にまとめます。

```sh
./scripts/build-mingw.sh release        # release/jvim3-<version>-win{32,64}.zip
VERSION=v3.0-j2.1b-utf8.2 ./scripts/build-mingw.sh release
```

バージョンは `VERSION` の指定がなければ `git describe` から取ります。パッケージ内
の実行ファイル名はアーキテクチャに合わせて付け替えます (64 ビットなら
`jvim64w.exe`)。makefile がアーキテクチャに関わらずターゲットを `jvim32*.exe` と
呼ぶためです。リリース時に CI が実行するのもこのコマンドです。

### Windows ローカルへの展開（動作確認用）

WSL から Windows の実機環境にビルド生成物を直接展開して動作確認する場合:

```sh
tools/deploy-windows.sh              # %USERPROFILE%\Downloads\jvim3-latest に展開
tools/deploy-windows.sh /mnt/c/path  # 展開先を指定
```

32bit (`jvim3-win32`) と 64bit (`jvim3-win64`) の両方をビルド・パッケージングし、Windows 側で実行ファイルがロック（起動中）されていないか確認した上で安全にフォルダ内を更新します（自作の `_vimrc` / `_jvimrc` は保護されます）。


そのほかのターゲット: `clean`、`warn` (警告を表示してコンパイル)、`split`
(デバッグ情報を `jvim32w.exe.debug` に分離して exe を strip)。

`ARCH` の切り替えや `warn` の on/off に `clean` は不要です。`obj-mingw` に
「何向けにビルドしたか」を示すスタンプを置いてあるので、ツールチェインや警告
フラグが変わると自動で全体を再ビルド・再リンクします。

### 32 ビットと 64 ビット

両アーキテクチャとも CI でビルドしてリリースしています。32 ビットは 64 ビット
Windows でも WoW64 で問題なく動きます。ポインタが `int`/`long` を経由して
切られる箇所はどちらのアーキテクチャにもありません。

**Windows の実行時テストはここには存在しません。** コンパイルが通り、ポインタが
切られなくなったことの確認は、同じ移植部分のソースを Unix でテストすることで
代えています。

### この構成に入っているもの

`KANJI` `UCODE` (UTF-8 / UCS-2 の入出力) `FEPCTRL` (IME 制御) `SYNTAX` `TRACK`
`CRMARK` `FEXRC` `NT106KEY` `USE_GREP` `USE_TAGEX` `USE_OPT` `USE_HISTORY`
`WEBB_COMPLETE` `WEBB_KEYWORD_COMPL` `TERMCAP` `XARGS`、および Win32 GUI 全体
(`winjnt.c`)。

`src/makjnt.mak` から戻せるものとして外してあるのは、`grep.exe`、`clip.exe`、
`vim32s.exe` です。エディタ内の検索拡張 (`USE_GREP`) は入っています。

ソースごと削除した機能 (BDF フォント、書庫内編集、MIME デコード) については
[USAGE.ja.md](USAGE.ja.md#削除した機能) を参照してください。

## テスト

```sh
./scripts/build-unix.sh test           # 5 つのスイート
./scripts/test-encoding.sh src/jvim3   # 48 ケース
./scripts/test-editing.sh  src/jvim3   # 74 ケース
./scripts/test-syntax.sh   src/jvim3   # 75 ケース
./scripts/test-sgr.sh      src/jvim3   # 9 ケース
./scripts/test-hostile.sh  src/jvim3   # 15 ケース
```

`test-encoding.sh` は漢字・UTF-8・マルチバイト編集を、`test-editing.sh` は移動、
オペレータ、レジスタ、マーク、undo、ex の範囲指定、`:g`、`:s`、検索、`:!` フィルタ、
ワイルドカード展開を見ます。`scripts/test-syntax.sh` は syntax/ のルールが実際に
何を色付けするかを `:syntax dump` 越しに見ます（`syntax/` の全ファイルに 1 つ
以上）。`scripts/test-sgr.sh` は端末に実際に送られるエスケープを見ます — ルール
ではなく描画側を見る唯一のスイートです。`scripts/test-hostile.sh` は誰も意図して
いない入力を与えます — 1 行 2 MB、あらゆるバイト値、ファイル末尾で切れた
マルチバイト列、再帰的なマッチャを使い切るほど入れ子にした正規表現。合わせて
221 ケースです。

敵性入力スイートは、何もクラッシュしていないのにケースを失敗として報告できる
唯一のスイートです。KNOWN-FAIL が 3 件あり、エディタがそれを終えられない
ことを意味します。「使えないほど遅くなった」をテストが言える唯一の形です。
入力ファイルは awk や dd ではなく `scripts/hostilegen.c` が作ります。5 つの OS で
走るのに、それぞれの awk がゼロバイトの扱いで一致しないからです。

必要なのは bash と C コンパイラです。jvim に端末を与えるために `scripts/ptyrun.c`
をビルドします。以前は `script(1)` を使っていましたが、あれは Linux と NetBSD と
その他の BSD で別のプログラムであり、NetBSD 版は標準入力がファイルのときコマンドの
完了前に終了してしまいます。`ptyrun` は各ケースに 20 秒 (`PTYRUN_TIMEOUT`) の制限
も与えるので、キー入力待ちで止まったケースはスイート全体を固めずに失敗します。

## Linux から BSD を確認する

```sh
./scripts/test-bsd-docker.sh              # FreeBSD: ビルドしてテスト
./scripts/test-bsd-docker.sh freebsd shell   # ゲストを立てたまま触る
./scripts/test-bsd-docker.sh freebsd clean   # 保存したゲストディスクを捨てる
```

**ローカルに置くゲストは FreeBSD だけです。** **clang** でビルドするので gcc が
警告で済ませるところで止まり、push しなくてもコミットしていない変更を含めて
今のツリーを確認できます。`shell` なら失敗した状態のゲストにそのまま入れます。
NetBSD も `./scripts/test-bsd-docker.sh netbsd` でインストールできますが、
NetBSD 固有の失敗を追うときだけで構いません。

Docker で BSD のコンテナは動きません (BSD のバイナリには BSD のカーネルが必要
です)。ここでのコンテナは QEMU を置く場所にすぎず、中の BSD は各 OS 自身のイメージ
から起動した本物の仮想マシンです。必要なものは `/dev/kvm`、ゲストを保存する
1.5 GB のディスク、ゲストを作るあいだの 25 GB の空き、そして各 OS の初回実行時の
ネットワークです。

初回はゲストを ssh で届く状態までインストールし、そのうえで保存するものを小さく
します。インストーラのキャッシュを消し、削除済みファイルが残したブロックを保存する
価値のないものにするために空き領域をゼロで埋め、リリースイメージをゲストディスクに
畳み込んで zstd で圧縮します。残るのは 1 ファイルだけ (FreeBSD で 1.3 GB、以前の
イメージ + オーバーレイでは 7.9 GB) で、ダウンロードは削除されます。2 回目以降は
そのファイルにオーバーレイを重ねるので 20 秒で立ち上がります。以前の版が保存した
ゲストは 2 ファイルのままですが、`./scripts/test-bsd-docker.sh freebsd compact` で
再インストールなしに同じ形にできます。

## CI が見ているもの

push と pull request のたびに、**Linux**、**FreeBSD**、**NetBSD**、
**OpenBSD**、**DragonFly** でビルドしてテストを実行し、mingw-w64 で
Windows 版を 32/64 ビットともクロスビルドします。`v*` のタグは同じことをしたうえで
Windows 版をリリースページに公開するので、壊れたビルドがリリースになることはあり
ません。定義は [.github/workflows/build.yml](.github/workflows/build.yml) です。

BSD は Linux ランナー上の VM で動きます。数分で起動する既成のゲストイメージを使う
点だけが `scripts/test-bsd-docker.sh` と違います。

## ビルドに必要なフラグと警告の方針

必要なフラグは **`-fcommon`** ひとつです。同じグローバル変数が複数のファイルで
定義されていて、gcc 10 からこれがエラーになったためです。

`-std=gnu89` は以前は必要でした。ソースが K&R 形式の関数定義で埋まっていて、C23 が
それを削除したからです。775 か所すべてをプロトタイプ形式にしたので、いまはコンパイラ
既定の方言でビルドできます。C23 が既定の gcc 15 以降でもそのまま通ります。

`-Wall` では警告が 251 件残り、そのうち 236 件は `-Wpointer-sign` です。JVim は
テキストを `char_u` (`unsigned char`) で保持し、それを C ライブラリや自身の
`char *` インタフェースに至る所で渡しています。これは型のノイズであってバグでは
ないため、残しています。

実際に壊れる種類の警告は、CI ではエラーにしています。暗黙の関数宣言
(64 ビットで戻り値のポインタが切られます)、ポインタ型の不一致、暗黙の `int`、
プロトタイプなし、return なし、未初期化変数の使用です。

検証済みの環境と、検証していないことの一覧は
[BUILDING-unix.md](BUILDING-unix.md#what-has-been-verified-and-what-has-not) に
あります。要点は次のとおりです。

- Linux (Ubuntu 24.04 / gcc)、FreeBSD (clang)、NetBSD、OpenBSD、DragonFly で
  221 テスト全件通過。Linux では AddressSanitizer と
  UndefinedBehaviorSanitizer の下でも全件通しており、CI でも毎回走ります
  (`./scripts/build-unix.sh asan` / `ubsan`)。
- macOS は 2026-08 まで CI で通っていましたが、今は対象ではありません。
- 実機・実端末・本物の IME での確認はしていません。すべてシリアルコンソール、
  pty、CI ランナー上です。
- Windows 版に実行時テストはありません。
