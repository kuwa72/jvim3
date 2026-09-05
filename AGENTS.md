# JVim3 project instructions

詳細なGitHub運用は [GITHUB_WORKFLOW.md](GITHUB_WORKFLOW.md) を参照する。

## Issue 開発フロー

1. `master` から作業ブランチを作る。
2. TDDで、まず `scripts/test-*.sh` に失敗するテストを追加する。
3. Redを確認してから実装し、Greenを確認する。
4. `test`、`strict`、必要に応じて `asan` / `ubsan` を実行する。
5. PRのbaseを `master` にしてPRを作成する。
6. GitHub Actionsの全CIが終了して成功するまで待つ。
7. CI成功後にPRをマージする。
8. マージ後に関連Issueがclosedになったこととマージコミットを確認する。

PR作成やCI起動だけではIssue対応を完了しない。調査結果・設計判断はIssueコメントに残し、着手順・依存関係はTracking Issueで管理する。
