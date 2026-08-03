#!/usr/bin/env bash
# Inject the RISC0 STARK prover (zk-verify + r0vm, ~147 MB) into a built zk_guess_game .lgx so that
# per-turn proof works out of the box on a catalog install — no rzup / ZK_VERIFY_BIN / PATH r0vm.
#
# WHY a post-build repack (not the flake): the logos module-builder's portable bundler assembles the
# variant tree from the plugin .so + its ldd-traced deps only; a `postInstall` that drops files in $out
# is filtered out. Until the bundler gains an extra-files hook (upstream ask), we repack here. The
# backend resolves zk-verify + r0vm next to its own .so (dladdr) and sets RISC0_SERVER_PATH to the
# sibling r0vm (see src/zk_guess_game_backend.cpp: pluginDir / zkVerifyBin / proveGuess).
#
# Usage:  bundle-prover-into-lgx.sh <in.lgx> [out.lgx]
#   PROVER_DIR   dir holding zk-verify + r0vm (default: /extra/tmp/zkg-prover)
#   PROVER_URL   base URL to fetch them if PROVER_DIR is missing (release assets)
set -euo pipefail

IN="${1:?usage: bundle-prover-into-lgx.sh <in.lgx> [out.lgx]}"
OUT="${2:-$IN}"
PROVER_DIR="${PROVER_DIR:-/extra/tmp/zkg-prover}"
PROVER_URL="${PROVER_URL:-https://github.com/xAlisher/lez-stark-verify/releases/download/prover-linux-amd64-r0vm3.0.5}"
VARIANT="variants/linux-amd64"

# expected sha256 (pin — the same binaries the backend was verified against)
SHA_ZKVERIFY="8e671ba9b191aa367d5247fe725493a47d40678e07be0ad7c21c853532d4df6d"
SHA_R0VM="36c016a5bb2ded5bd1f8f92cc487e6ffaeb1e95ec05850c983081a0f716b515b"

need() { command -v "$1" >/dev/null || { echo "need $1" >&2; exit 1; }; }
need tar; need sha256sum

# resolve the two binaries: prefer a local dir, else fetch from the release
resolve() { # <name> <sha>
  local n="$1" sha="$2" p="$PROVER_DIR/$1"
  if [ -f "$p" ]; then echo "$p"; return; fi
  need curl
  local tmp="$(mktemp -d)/$n"
  curl -fSL "$PROVER_URL/$n" -o "$tmp"
  echo "$tmp"
}
ZKV="$(resolve zk-verify "$SHA_ZKVERIFY")"
R0V="$(resolve r0vm "$SHA_R0VM")"

check() { local f="$1" want="$2"; local got; got=$(sha256sum "$f" | cut -d' ' -f1); \
  [ "$got" = "$want" ] || { echo "sha256 mismatch for $f: $got != $want" >&2; exit 1; }; }
check "$ZKV" "$SHA_ZKVERIFY"
check "$R0V" "$SHA_R0VM"

WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
tar xzf "$IN" -C "$WORK"
[ -d "$WORK/$VARIANT" ] || { echo "no $VARIANT in $IN" >&2; exit 1; }
install -Dm755 "$ZKV" "$WORK/$VARIANT/zk-verify"
install -Dm755 "$R0V" "$WORK/$VARIANT/r0vm"

# repack (stable order; keep manifest/variant layout the packager expects)
tar czf "$OUT" -C "$WORK" .
echo "bundled prover into $OUT"
tar tzf "$OUT" | grep -E 'zk-verify|r0vm' | sed 's/^/  + /'
echo "  size: $(du -h "$OUT" | cut -f1)"
