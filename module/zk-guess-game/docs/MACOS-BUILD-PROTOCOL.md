# macOS (darwin-arm64) build protocol — zk_guess_game

You **cannot** build the `darwin-arm64` variant on Linux (nix darwin needs a Darwin machine). This is the
verified, end-to-end recipe used to cut the `darwin-arm64` asset for `zk_guess_game-v0.1.1` (2026-08-03).
Signing happens back on the Linux box — the signer is content-agnostic over the `.lgx`.

## Build box
- **`m1`** — `ssh m1` = `sher@192.168.1.43` (LAN-only; `ping 192.168.1.43` to check it's up — don't trust
  `tailscale status`, which may not list it). Apple Silicon, macOS 26.x, `nix` + `git` preinstalled.
- **NOT** `diana-mbp` / `alishers-macbook-pro` (100.125.46.88) — that's Diana's personal MacBook, flagged
  in `~/.ssh/config` as "NOT a build machine."

## One-time toolchain (bare box)
```sh
# host cargo (builds zk-verify + settle-win host binaries)
curl -sSf https://sh.rustup.rs | sh -s -- -y --profile minimal
# RISC0: r0vm + cargo-risczero + guest rust toolchain
curl -L https://risczero.com/install | bash          # installs ~/.risc0/bin/rzup
~/.risc0/bin/rzup install r0vm 3.0.5
~/.risc0/bin/rzup install cargo-risczero 3.0.5
~/.risc0/bin/rzup install rust                        # guest (RISC-V) toolchain
export PATH="$HOME/.cargo/bin:$HOME/.risc0/bin:$PATH"
```
r0vm lands at `~/.risc0/extensions/v3.0.5-cargo-risczero-aarch64-apple-darwin/r0vm` (symlinked to
`~/.cargo/bin/r0vm`).

## Source
From the Linux box, rsync the working trees (avoids gh auth on the mac):
```sh
rsync -az --exclude=.git --exclude='result*' --exclude=target --exclude='*/target' \
  ~/lez-stark-verify/            m1:~/lez-stark-verify/
rsync -az --exclude=.git --exclude=target --exclude='*/target' \
  ~/basecamp/forks/spel-fork-dev-repin/  m1:~/spel-fork-dev-repin/     # holds settle_win.rs
```

## Build (on m1) — `unset RISC0_DEV_MODE` for REAL proofs
```sh
export PATH="$HOME/.cargo/bin:$HOME/.risc0/bin:$PATH"; unset RISC0_DEV_MODE

# 1. the module plugin — MUST be .#lgx-portable, NOT .#lgx (which yields a darwin-arm64-DEV variant
#    that silently won't load in Basecamp). Produces the darwin-arm64 variant .lgx (plugin + Qt-linked
#    dylibs), ~3.6 MB.
cd ~/lez-stark-verify/module/zk-guess-game
nix --extra-experimental-features 'nix-command flakes' build '.#lgx-portable' --out-link result-lgx-portable

# 2. zk-verify (per-turn STARK prover CLI). Bin lives at host/src/bin/zk-verify.rs.
cd ~/lez-stark-verify/module/zk-guess && cargo build --release --bin zk-verify
#   -> module/zk-guess/target/release/zk-verify   (Mach-O arm64)

# 3. settle-win (on-zone win settlement). It's its OWN workspace ([workspace] in its Cargo.toml) — build
#    from INSIDE the crate dir, not with -p from the fork root (root default-run is `spel`).
cd ~/spel-fork-dev-repin/zk-guess-methods && cargo build --release --bin settle_win
#   -> ~/spel-fork-dev-repin/zk-guess-methods/target/release/settle_win   (Mach-O arm64)
#   (LEZ git deps — lee/wallet/common @787a15aa — fetch from public github; no auth needed.)
```

## Bundle the prover into the darwin variant (on the Linux box or m1 — lgx is content-agnostic)
The stock `tools/bundle-prover-into-lgx.sh` is hard-coded to `linux-amd64`; for darwin, bundle by hand
(three arm64 bins: `r0vm`, `zk-verify`, `settle-win`, renamed from `settle_win`):
```sh
LGX=zk_guess_game-0.1.1-darwin-arm64.lgx   # copy of the .#lgx-portable output; chmod u+w
W=$(mktemp -d); lgx extract "$LGX" --variant darwin-arm64 --output "$W"
install -m755 zk-verify  "$W/darwin-arm64/zk-verify"
install -m755 r0vm       "$W/darwin-arm64/r0vm"
install -m755 settle_win "$W/darwin-arm64/settle-win"          # NOTE rename _ -> -
lgx add "$LGX" --variant darwin-arm64 --files "$W/darwin-arm64" \
  --main zk_guess_game_plugin.dylib --view qml/Main.qml --yes    # main is .dylib on darwin, .so on linux
lgx verify "$LGX"                                                 # "Package structure is valid"
```

## Sign (Linux box) + attach to the SAME tag
```sh
# LD_LIBRARY_PATH = AppImage usr/lib + nix libsodium (see /release §3)
lgx_signer sign "$LGX" ~/.config/logos-signing/keys/xAlisher.jwk xAlisher https://github.com/xAlisher
lgx_signer verify "$LGX"       # is_signed: yes · signature_valid: yes · signer_name: xAlisher
gh release upload zk_guess_game-v<ver> "$LGX" --repo xAlisher/lez-stark-verify
```

## Headless sanity check (catches the #1 darwin-load failure)
```sh
otool -L variants/darwin-arm64/zk_guess_game_plugin.dylib   # deps must be @loader_path/@rpath/system —
                                                            # NO /nix/store, /Users, /extra paths.
```
Bundled dylibs (boost/ssl/crypto/sodium) resolve via `@loader_path`; Qt frameworks + `libintl` via
`@rpath` supplied by Basecamp at load. **Real** verification (Basecamp GUI loads it + a live settlement)
is wetware — do it on a mac with Basecamp installed.
