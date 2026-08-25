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
VARIANT="linux-amd64"

# expected sha256 (pin — the same binaries the backend was verified against)
SHA_ZKVERIFY="8e671ba9b191aa367d5247fe725493a47d40678e07be0ad7c21c853532d4df6d"
SHA_R0VM="36c016a5bb2ded5bd1f8f92cc487e6ffaeb1e95ec05850c983081a0f716b515b"

# lgx CLI recomputes the manifest content-hashes when it (re)writes a variant. A tar-based repack
# leaves the manifest hashes stale → the signer rejects it ("Content hash mismatch"). So we MUST use
# `lgx` to add the prover, not tar. Auto-detect it (same as the platform's package script).
LGX_BIN="${LGX_BIN:-$(command -v lgx 2>/dev/null || find /nix/store -maxdepth 3 -type f -path '*/bin/lgx' 2>/dev/null | sort | tail -n1)}"

need() { command -v "$1" >/dev/null || { echo "need $1" >&2; exit 1; }; }
need sha256sum
[ -n "$LGX_BIN" ] && [ -x "$LGX_BIN" ] || { echo "lgx CLI not found; set LGX_BIN" >&2; exit 1; }

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
# 1) start OUT as a copy of IN; 2) extract the linux-amd64 variant; 3) drop the prover beside the .so;
# 4) `lgx add` the complete variant back (REPLACES it, recomputing all manifest hashes).
[ "$IN" != "$OUT" ] && cp -f "$IN" "$OUT" && chmod u+w "$OUT"
"$LGX_BIN" extract "$OUT" --variant "$VARIANT" --output "$WORK" >/dev/null
VDIR="$WORK/$VARIANT"
[ -d "$VDIR" ] || { echo "no $VARIANT variant in $IN" >&2; exit 1; }
install -Dm755 "$ZKV" "$VDIR/zk-verify"
install -Dm755 "$R0V" "$VDIR/r0vm"
# on-zone win settlement binary (optional): bundle if present beside the prover bins.
[ -x "$PROVER_DIR/settle-win" ] && install -Dm755 "$PROVER_DIR/settle-win" "$VDIR/settle-win" \
  && echo "  + bundled settle-win (on-zone settlement)"
# TOK pot client (optional, EPIC D): the game backend shells out to it per on-zone pot step.
[ -x "$PROVER_DIR/zkg_pot" ] && install -Dm755 "$PROVER_DIR/zkg_pot" "$VDIR/zkg_pot" \
  && echo "  + bundled zkg_pot (on-zone TOK pot)"
"$LGX_BIN" add "$OUT" --variant "$VARIANT" --files "$VDIR" \
  --main zk_guess_game_plugin.so --view qml/Main.qml --yes >/dev/null

echo "bundled prover into $OUT (hashes recomputed via lgx)"
"$LGX_BIN" verify "$OUT" 2>&1 | grep -iE 'valid|ok|error' | head -1 | sed 's/^/  lgx verify: /'
echo "  size: $(du -h "$OUT" | cut -f1)"
