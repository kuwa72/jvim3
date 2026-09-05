# Project Instructions for Claude Code

詳細なGitHub運用は [GITHUB_WORKFLOW.md](GITHUB_WORKFLOW.md) を参照する。

## Issue 開発フロー

- TDDで、まず `scripts/test-*.sh` に失敗するテストを追加してRedを確認し、その後実装してGreenを確認する。
- `test`、`strict`、必要に応じて `asan` / `ubsan` を実行する。
- `master` ベースでPRを作成する。
- GitHub Actionsの全CIが終了して成功するまで待つ。
- CI成功後にマージし、関連Issueのクローズとマージコミットを確認する。

PR作成やCI起動だけではIssue対応を完了しない。調査結果・設計判断はIssueコメントに残し、着手順・依存関係はTracking Issueで管理する。
