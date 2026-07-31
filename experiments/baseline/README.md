# Baseline — reproduce the RISC0 STARK proof + on-LEZ verify (issue #6)

The unbounded reference run the capped experiments (#2) measure against. This is the
**referral private-execution program**: a RISC0 zkVM guest that computes a credit note
and binds its recipient `(npk,vpk)→account_id` in-circuit, proven as a STARK and settled
privately on a live LEZ sequencer.

## Environment

| | |
|---|---|
| RISC0 | r0vm / cargo-risczero **3.0.5** (`rzup`), guest rust 1.97.0 |
| LEZ | `logos-blockchain/logos-execution-zone` @ **787a15aa** (pinned in the spel fork) |
| Prove workspace | `~/basecamp/forks/spel-fork-dev-repin/referral-methods` (companion repo `xAlisher/spel-lez-dev-repin`) |
| Target dir | `CARGO_TARGET_DIR=/extra/tmp/referral-risc0/target-lez` (root partition chokes on the guest build) |
| Sequencer | **live on Sneg**, tmux `referral-seq` → `./sequencer_service sequencer_config.json --port 3040 --listen-address 127.0.0.1` (real verification mode, `RISC0_DEV_MODE` unset) |

## Reproduce

```bash
cd ~/basecamp/forks/spel-fork-dev-repin/referral-methods
export PATH="$HOME/.risc0/bin:$HOME/.cargo/bin:$PATH"
export CARGO_TARGET_DIR=/extra/tmp/referral-risc0/target-lez
export RISC0_DEV_MODE=0            # REAL proof — no dev-mode shortcut

# 1) executor tests (logic sanity, fast)
cargo test --test emit_credit

# 2) real STARK proof (the prove-side baseline)
/usr/bin/time -v cargo test --test prove --release -- --nocapture

# 3) end-to-end: submit the proven tx to the live LEZ sequencer on Sneg (full verify)
#    (sequencer already running; see seq-real.log for block progress)
```

## The verification ladder (all real mode, zero dev-mode)

| Stage | Cost (prior measured) |
|---|---|
| Executor tests (3/3) | fast |
| **Real STARK proof** (program) | **~116.6 s** |
| Full privacy-preserving tx on-chain (PP-circuit ~8× the program) | **~16 min · ~9.6 GB peak RAM** |
| Redirected recipient | **rejected** (unprovable — bind is in-circuit) |

## Fresh measured run

_Populated by the `/usr/bin/time -v` prove run — prove wall time + peak RSS below._

<!-- BASELINE_RESULT -->
