# ADR-0002 — Bundle the prover into the `.lgx` via a post-build repack

**Status:** accepted

## Context
Per-turn proof (ADR-0001) needs `zk-verify` (~43 MB) **and** `r0vm` (~104 MB) present at runtime —
`zk-verify` spawns `r0vm` as its prover server. Requiring a catalog installer to `rzup install r0vm`,
set `ZK_VERIFY_BIN`, or get `r0vm` on PATH is a broken out-of-box experience.

The Logos module-builder's portable bundler assembles the `.lgx` variant from the plugin `.so` **plus
its `ldd`-traced dependencies only**. A flake `postInstall` that drops files in `$out` is **filtered
out** — verified empirically (the `.lgx` stayed 4.3 MB with the binaries in `$out`). There is no
extra-files hook in `nix-bundle-logos-module-install`.

## Decision
Inject the prover **after** the flake build, with `module/zk-guess-game/tools/bundle-prover-into-lgx.sh`:
repack the `.lgx` tarball, dropping `zk-verify` + `r0vm` into `variants/linux-amd64/` beside the
plugin `.so` (sha256-pinned; sourced from a local dir or the `prover-linux-amd64-r0vm3.0.5` release).
The backend resolves them **next to its own `.so`** via `dladdr`, and sets `RISC0_SERVER_PATH` to the
sibling `r0vm`. Result: 82 MB gzipped `.lgx`, per-turn proof works with zero config.

## Consequences
- Zero-config per-turn `verified on LEZ ✓` on a catalog install (linux-amd64).
- The 147 MB of binaries are **not** committed to git — they live as release assets, pulled at repack.
- The clean flake-native path needs an **extra-files hook upstream** in
  `nix-bundle-logos-module-install`; until then a catalog release must run this repack step (or that
  hook). Filed as a follow-up ask.
- Linux-amd64 only for now; a darwin bundle needs a darwin `r0vm` + a darwin repack.
