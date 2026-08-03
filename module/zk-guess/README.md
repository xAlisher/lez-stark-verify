# zk-guess — M1 honesty core (DRAFT, not yet built)

Issue #11 · epic #10 · plan [`docs/v1-mvp-plan.md`](../../docs/v1-mvp-plan.md)

The game oracle: prove the **honest direction** of a public guess vs a **sealed** secret,
without revealing the secret, **and** prove the secret is the committed one (no swap). Extends
the proven `module/zk-eligibility` guest (secret-vs-threshold → bool) → (guess-vs-secret → dir
+ commitment-open).

> **Status: BUILT + PROVEN ✅** — the 4-ways harness (M1) and the headless game (M2) both pass
> on a real STARK (`RISC0_DEV_MODE` unset). Results: [`docs/reports/mvp-headless-report.md`](../../docs/reports/mvp-headless-report.md).

## Build (M1 acceptance)
1. Scaffold a RISC0 project (reuse the `zk-eligibility` layout): `methods/guest/src/main.rs` ←
   `guest_main.rs`, `host/src/main.rs` ← `host_main.rs`; rename the generated `METHOD_*` image
   constants to `GUESS_ELF` / `GUESS_ID`.
2. **`unset RISC0_DEV_MODE`** (a real proof; the swap test asserts inside the guest).
3. `CARGO_TARGET_DIR=/extra/... cargo run --release` → expect the **4 checks** to pass.

## The one thing to confirm on first build — digest parity
The guest hashes with `risc0_zkvm::sha::Impl` (SHA-256, accelerated); the host commits with
`sha2::Sha256`. Both are SHA-256 and **must produce the identical 32-byte digest** for the
commitment-open to pass. If check #1 fails on "commitment mismatch," this parity is the suspect.

## The 4 ways (same rigor as the original)
1. valid turn receipt verifies against the image id
2. the **secret is absent** from the journal (journal = `(commitment, guess, dir)` only)
3. a tampered receipt is rejected
4. a **swapped secret is unprovable** — proving with a secret that doesn't open the commitment
   halts the guest, so no receipt can exist
