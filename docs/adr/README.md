# Architecture Decision Records — ZK Guess

Each ADR is one decision we can argue on its own: the context, the call, and what it costs. Status is
one of `accepted`, `proposed`, or `superseded`.

| # | Decision | Status |
|---|---|---|
| [0001](0001-two-tier-proving.md) | Two-tier proving — per-turn dev-mode (fast) vs win real-mode on-zone | accepted |
| [0002](0002-bundle-prover.md) | Bundle the prover (`zk-verify` + `r0vm`) into the `.lgx` via post-build repack | accepted |
| [0003](0003-own-public-sequencer.md) | Run our own version-matched public sequencer; why the official testnet doesn't work | accepted |
| [0004](0004-client-verified-commitment.md) | `SHA256(secret‖blind)` commitment + client-verified win | accepted |
| [0005](0005-distributed-entropy.md) | Distributed mouse-draw entropy → sealed number | accepted |
| [0006](0006-waku-transport-idempotency.md) | `delivery_module` (Waku) transport + best-effort idempotency | accepted |
| [0007](0007-sequencer-basic-auth.md) | Basic-auth wiring for the auth-gated sequencer (base64-in-username) | accepted |

Cross-cutting design lives in [`../GAME-DESIGN.md`](../GAME-DESIGN.md); the sequencer deploy in
[`../sequencer-deploy-plan.md`](../sequencer-deploy-plan.md).
