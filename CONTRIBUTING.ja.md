# JVim 3 への貢献

[English](CONTRIBUTING.md) | **日本語**

Issue と pull request は <https://github.com/kuwa72/jvim3> へ。Issue、pull
request、コミットメッセージ、いずれも日本語で構いません。英語と同じように歓迎
します。

## 取り込みやすい変更の条件

3 つあります。

1. `./scripts/build-unix.sh test` が通ること。282 ケースで、1 分半ほどで終わります。

2. `./scripts/build-unix.sh strict` が通ること。CI と同じ `-Werror=` 群
   （暗黙の宣言、ポインタ型の不一致、プロトタイプなし、return なし、暗黙の
   `int`、未初期化変数の使用）を push の前に確認できます。この中には gcc では
   警告で済み、FreeBSD のジョブが使う clang では止まるものがあります。
3. `./scripts/build-unix.sh asan` と `./scripts/build-unix.sh ubsan` が通ること。
   テストでは見えない誤りで止まるビルドに対して、同じケースを走らせます。
   バッファを 1 バイト越えて読んでもエディタが書き出すバイト列は変わらないので、
   全ケース通過のままバグが出荷されます。どちらも CI のジョブで、レポートは
   `log_path` から集めます。stderr はスイートが捨てるため、そこからは取れません。

`-Wpointer-sign` の警告は想定内で、そのまま残しています。ソースは `char` と
`unsigned char` (`char_u`) を意図的に混ぜており、236 個あります。理由は
[BUILDING-unix.md](BUILDING-unix.md#warnings) に書いてあります。

CI は push と pull request のたびに、Linux、FreeBSD、NetBSD、OpenBSD、DragonFly
の全部でテストを実行し、Windows 版を両アーキテクチャでクロスビルドします。fork
からの pull request は読み取り専用のトークンで動くので、ビルドとテストは走ります
が、何も公開はできません。

## 特に助かること

- **64 ビット Windows 版を実際に動かしてみること。** CI がしているのはコンパイル
  だけで、Windows の実行時テストはどちらのアーキテクチャにもありません。動いても
  動かなくても、報告してもらえると助かります。
- **実機の本物の IME で使ってみること。** CI はランナーと pty とシリアルコンソール
  だけです。本物の IME で長く使った確認はありません。
- **ディストリビューション向けパッケージ。** まだどこにもありません。作ったら教えて
  ください。README からリンクします。

リリースが配布するバイナリそのものを試すのに mingw ツールチェインは要りません。
`scripts/fetch-ci-build.sh` が、任意のコミットについて CI がビルドしたパッケージを
ダウンロードします。クラッシュしたら `scripts/resolve-crash.sh` が
`%LOCALAPPDATA%\jvim3\report.log` のレポートをソース行に戻します。

## GitHubの運用

Issue、Tracking Issue、TDD、プルリクエスト、CI、マージの運用は [GITHUB_WORKFLOW.md](GITHUB_WORKFLOW.md) にまとめています。

## どこに何があるか

| | |
| --- | --- |
| [BUILDING-unix.md](BUILDING-unix.md) | Linux と BSD でのビルド。スクリプトが検出するもの。CI がカバーする範囲。検証済みのことと、していないこと。 |
| [BUILDING-mingw.md](BUILDING-mingw.md) | Windows 版のビルドと、UTF-8・Unicode GUI・DPI 対応・テキスト描画の詳しい説明。 |
| [BUILDING.ja.md](BUILDING.ja.md) | ビルド手順の日本語版 (両プラットフォーム)。 |
| [USAGE.ja.md](USAGE.ja.md) | 起動方法、設定、エンコーディングの考え方、IME、トラブルシューティング。 |

## テスト

合うスイートにケースを足してください。文字コードとマルチバイト編集なら
`scripts/test-encoding.sh`、移動・オペレータ・レジスタ・マーク・undo・ex コマンド
なら `scripts/test-editing.sh` です。どちらも実際の pty (`scripts/ptyrun.c`) 越しに
エディタを動かしてバイト列を比較するので、人が打つのと同じ入力経路を通ります。

まだ直せないバグのケースを入れておきたいときは、`ok` の代わりに `knownfail` を
指定してください。スイートは `knownfail` を既知の失敗として数え、通常の失敗とは別に扱います。通るようになったら教えてくれます。

## コミットメッセージ

その変更が何をするのかを、命令形の 1 文で書いてください。`fix:` や `feat:` の
ような接頭辞は付けません (`git log` を見てください)。50 文字程度です。知る価値の
ある理由は本文に書いてください。日本語でも構いません。

CLA も sign-off もありません。これはパブリックドメインです
([LICENSE](LICENSE))。パッチを送ることで、それもパブリックドメインに置くことに
なります。

## ブランチとリリース

`master` は 5 つの環境すべてでビルドとテストが通る状態を保ちます。`master` に入れる
前に複数プラットフォームで CI を回したい作業は、作業内容の名前を付けたトピック
ブランチで進め、rebase で戻します。merge コミットは使いません。

リリースはタグから CI が作ります。すべての環境でビルドとテストが通ってからしか
作られないので、壊れたビルドがリリースになることはありません。リリースは、この
リポジトリを見ていない人にとって意味のある変更があるときに出すものです。CI、
テスト、ドキュメントの作業は、出す価値のあるものが溜まるまで `master` に置いて
おきます。手順は [RELEASING.md](RELEASING.md) にあります。

## セキュリティ

非公開の報告窓口はありません。ネットワーク機能のないローカルのテキストエディタで、
メンテナは 1 人です。通常の公開 issue を立ててください。
