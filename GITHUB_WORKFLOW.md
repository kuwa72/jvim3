# GitHub運用

## Issue

- 個別Issueには、目的、背景、前提、対象外、受け入れ条件、確認方法を書く。
- 複数Issueの順序や依存関係は、Tracking Issueにまとめる。
- 調査結果、設計判断、方針変更、実測結果は関連Issueのコメントに時系列で残す。
- 着手時、PR作成時、CI完了時、マージ時にTracking Issueを更新する。
- Issueを閉じても参照し続ける方針は、このファイルに残す。Issueには現在の計画と進捗だけを書く。

## TDDからマージまで

1. `master` から作業ブランチを作る。
2. 期待する動作を確認するテストを `scripts/test-*.sh` に追加する。
3. テストが失敗することを確認する（Red）。
4. 実装してテストを通す（Green）。
5. `./scripts/build-unix.sh test`、`strict`、必要に応じて `asan` / `ubsan` を実行する。
6. コミットしてPRを作成する。PRのbaseは `master` にする。
7. GitHub Actionsの全CIが終了し、成功したことを確認する。
8. CI成功後にPRをマージする。
9. マージ後に関連Issueが閉じたことと、マージコミットを確認する。

PRを作成した時点やCIを起動した時点では完了としない。CI確認・マージ・Issueクローズ確認までを完了条件とする。

## GitHub上の情報の置き場所

| 情報 | 場所 |
| --- | --- |
| 個別の仕様・受け入れ条件 | 個別Issue |
| 複数Issueの着手順・依存関係 | Tracking Issue |
| 調査結果・設計判断・進捗 | Issueコメント |
| 変更内容・テスト・CI結果 | Pull Request本文 |
| 恒久的な開発方針 | このファイル、または `CONTRIBUTING.md` |
| 長い設計資料 | リポジトリ内の文書 |

このリポジトリではGitHub Discussions、Projects、Wikiを使わないため、恒久的な方針はリポジトリ内の文書で管理する。
