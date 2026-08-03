#!/usr/bin/env bash
# Launch Basecamp with the zk-guess backend wired up, so the ZK Guess module's
# QProcess can find the real `zk-verify` tool + the bundled receipt fixtures.
#
# Install the module first (once):
#   ~/zk_guess_ui-0.1.0-linux-amd64.lgx  ->  Basecamp Package Manager ▸ Install from file
#   (unsigned MVP build → allow unsigned), or install into an isolated instance.
#
# Then run this to launch Basecamp with the backend reachable, open "ZK Guess",
# and click "Verify honest turn" / "Verify tampered turn".
set -euo pipefail

export ZK_VERIFY_BIN="$HOME/.local/share/zk-guess/zk-verify"
export ZK_FIXTURES="$HOME/.local/share/zk-guess/fixtures"

[ -x "$ZK_VERIFY_BIN" ] || { echo "missing $ZK_VERIFY_BIN — run the M1 build + stage step first"; exit 1; }
echo "ZK_VERIFY_BIN=$ZK_VERIFY_BIN"
echo "ZK_FIXTURES=$ZK_FIXTURES"

exec "$HOME/logos-basecamp-current.AppImage" "$@"
