# Project Instructions for Claude Code

## Issue 開発フロー
- **TDD (テスト駆動開発) の徹底**: 実装に先立ち、まずは期待される動作を検証するテストケース（`scripts/test-*.sh`）を追加して失敗を確認（Red）し、その後実装を行ってテストをパス（Green）させる。
- **PR・CI 検証・マージ**:
  - 作業ブランチを作成してコミット。
  - GitHub CLI (`gh pr create`) で PR を作成。
  - GitHub Actions CI の完了・成功 (`gh pr checks`) を確認。
  - CI 成功を確認した上でマージ (`gh pr merge`) まで完結させる。
