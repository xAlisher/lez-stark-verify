# zk-eligibility — a working ZK module (use case: private eligibility)

**Use case:** prove a *secret* value clears a *public* threshold — "balance ≥ X",
"age ≥ 18", "score ≥ N" — **without revealing the value**. A RISC0 STARK guest computes
the predicate; the journal carries only `(threshold, eligible)`. This is the shielded
capability a LEZ-style zone is for (private airdrops, KYC-free access, solvency).

## It works — proven four ways (real proof, `RISC0_DEV_MODE` unset)

```
[1] verify OK · journal = (threshold=10000, eligible=true)     ← proof verifies
[2] secret present in journal? false                            ← zero-knowledge of the witness
[3] tampered receipt verifies? false                            ← integrity (a mutated receipt is rejected)
[4] proof for ineligible witness produced? false                ← soundness (value < threshold cannot prove)
ALL 4 CHECKS PASSED
```

## Run it

```bash
export PATH="$HOME/.risc0/bin:$HOME/.cargo/bin:$PATH"
export CARGO_TARGET_DIR=/extra/tmp/referral-risc0/zk-eligibility-target
unset RISC0_DEV_MODE          # REAL proof (dev mode fakes it)
cargo run --release           # from module/zk-eligibility/
```

- **guest** `methods/guest/src/main.rs` — reads `value` (private) + `threshold`, asserts
  `value >= threshold`, commits only `(threshold, true)`.
- **host** `host/src/main.rs` — proves, verifies, and runs the four checks above.

## Where this goes

This is the ZK core. Per `docs/research.md` + `experiments/results/feasibility.md`, **verify
is the cheap, node-runnable side** (proving needs ~10 GB). Next: wrap `verify` as the Basecamp
module (#3, off the `zone_sequencer` template) and drive it from the sample app (#4) —
submit a receipt → verify on the node → show ✓ / reject a tampered or ineligible one.
