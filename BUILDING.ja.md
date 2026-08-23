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
./scripts/build-unix.sh test     # ビルドしてテスト 148 ケースを実行
./scripts/build-unix.sh strict   # CI がエラー扱いする警告つきでビルド
./scripts/build-unix.sh clean
```

`strict` は CI の「出てはいけない警告」ジョブと同じもので、push の前に手元で
走らせられます。1 分の価値はあります。構造体の初期化子が別のメンバに落ちても
gcc は警告で済ませますが、FreeBSD がビルドに使う clang は止まります。

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

インストールは手作業です。ビルド時に埋め込まれるパスに合わせてください。

```sh
sudo install -m 755 src/jvim3           /usr/local/bin/
sudo install -m 644 doc.j/vim.hlp       /usr/local/lib/jvim3.hlp   # 英語版は doc/vim.hlp
sudo install -m 644 doc/vim.1           /usr/local/man/man1/jvim3.1
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

そのほかのターゲット: `clean`、`warn` (警告を表示してコンパイル)、`split`
(デバッグ情報を `jvim32w.exe.debug` に分離して exe を strip)。

`ARCH` の切り替えや `warn` の on/off に `clean` は不要です。オブジェクトディレクトリ
と exe の名前は両アーキテクチャで共通なので、`obj-mingw` に「何向けにビルドしたか」
を示すスタンプを置き、ツールチェインや警告フラグが変わったら全体を再ビルド・再リンク
します (同じ条件での再実行では何もコンパイルしません)。これがないと、32 ビットの
オブジェクトに 64 ビットのリンカを当てて `file format not recognized` で止まり、逆
方向はさらに厄介で、オブジェクトが最新のままなので make は何もせず、別アーキテクチャ
でリンクされた exe が残っていました。

### 32 ビットと 64 ビット

**リリースしているのは 32 ビット**で、Windows 11 x64 でも WoW64 で問題なく動きます。

`ARCH=x86_64` は、以前はポインタが `int` や `long` を経由して半分失われる箇所が
37 か所、ハードエラーが 9 か所ありました。それらは解消済みです。両方とも CI で
ビルドしています。

```sh
ARCH=x86_64 ./scripts/build-mingw.sh warn
```

**ただし 64 ビット版は一度も実行されていません。** コンパイルが通り、ポインタが
切られなくなったことは、動くことと同義ではありません。Windows の実行時テストは
ここには存在しません。誰かが実際に動かすまで、リリースは 32 ビットのままにします。

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
./scripts/build-unix.sh test           # 3 つのスイート
./scripts/test-encoding.sh src/jvim3   # 48 ケース
./scripts/test-editing.sh  src/jvim3   # 64 ケース
./scripts/test-syntax.sh   src/jvim3   # 36 ケース
```

`test-encoding.sh` は漢字・UTF-8・マルチバイト編集を、`test-editing.sh` は移動、
オペレータ、レジスタ、マーク、undo、ex の範囲指定、`:g`、`:s`、検索、`:!` フィルタ、
ワイルドカード展開を見ます。`scripts/test-syntax.sh` は syntax/ のルールが実際に
何を色付けするかを `:syntax dump` 越しに見ます（`syntax/` の全ファイルに 1 つ
以上）。合わせて 148 ケースです。

必要なのは bash と C コンパイラです。jvim に端末を与えるために `scripts/ptyrun.c`
をビルドします。以前は `script(1)` を使っていましたが、あれは Linux と NetBSD と
その他の BSD で別のプログラムであり、NetBSD 版は標準入力がファイルのときコマンドの
完了前に終了してしまいます。`ptyrun` は各ケースに 20 秒 (`PTYRUN_TIMEOUT`) の制限
も与えるので、キー入力待ちで止まったケースはスイート全体を固めずに失敗します。

## Linux から BSD を確認する

```sh
./scripts/test-bsd-docker.sh              # FreeBSD: ビルドしてテスト
./scripts/test-bsd-docker.sh netbsd
./scripts/test-bsd-docker.sh all
```

Docker で BSD のコンテナは動きません (BSD のバイナリには BSD のカーネルが必要
です)。ここでのコンテナは QEMU を置く場所にすぎず、中の BSD は各 OS 自身のイメージ
から起動した本物の仮想マシンです。`/dev/kvm`、12 GB 程度のディスク、そして各 OS の
初回実行時にはネットワークが必要です。初回はゲストを ssh で届く状態までインストール
してディスクを保存するので、2 回目以降は 1 分以内に立ち上がります。

## CI が見ているもの

push と pull request のたびに、**Linux**、**macOS**、**FreeBSD**、**NetBSD**、
**OpenBSD**、**DragonFly** でビルドして両方のスイートを実行し、mingw-w64 で
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

実際に壊れる種類の警告は、CI ではエラーにしています。暗黙の関数宣言 (64 ビットで
戻り値のポインタが切られます。OpenBSD で `tgoto()` が全テストを落としたのがまさに
これでした)、ポインタ型の不一致、暗黙の `int`、プロトタイプなし、return なし、
未初期化変数の使用です。

検証済みの環境と、検証していないことの一覧は
[BUILDING-unix.md](BUILDING-unix.md#what-has-been-verified-and-what-has-not) に
あります。要点は次のとおりです。

- Linux (Ubuntu 24.04 / gcc)、FreeBSD、NetBSD、OpenBSD で 110 テスト通過。
  Linux ではエンコーディングの 48 ケースを AddressSanitizer 下でも通しています。
- DragonFly は CI でビルドとテストを通していますが、それ以外の確認はありません。
- 実機・実端末・本物の IME での確認はしていません。すべてシリアルコンソール、
  pty、CI ランナー上です。
- Windows 版に実行時テストはありません。64 ビット版は未実行です。
