# EPIC F — the guess game on the real LEZ (design)

_Making "verified on LEZ" literal: the per-turn STARK is verified by the **sequencer** (against
`PRIVACY_PRESERVING_CIRCUIT_ID`), not just client-side. Grounded in the referral `emit_credit`
e2e, which already ran this exact path. Issue #19 (epic #18)._

## The mechanism (from exploration)
- **Sequencer** = `sequencer_service` (jsonrpsee RPC :3040). Run standalone (no bedrock node):
  `just run-sequencer standalone "" 3040` with `RISC0_DEV_MODE=1` for fast dev receipts.
- **Submit** = `wallet.send_privacy_preserving_tx(accounts, instruction, &program)` → `poll_tx` →
  `sync_to_latest_block`. Inclusion in a block = "the sequencer verified it." Blueprint:
  `spel-fork-dev-repin/referral-methods/src/bin/e2e_submit.rs` (copy almost verbatim).
- **Program** = a `#[lez_program]` (spel_framework); the guest bin calls `program::main()`; the
  methods crate embeds the ELF via `risc0_build::embed_methods()`. `wallet deploy-program <elf>`
  records its image id on-zone.
- **On-zone verify** = `lee/state_machine/.../privacy_preserving_transaction/circuit/mod.rs`:
  `receipt.verify(PRIVACY_PRESERVING_CIRCUIT_ID)` — the *same* call we make client-side, tied to
  block production; the program's own id is bound inside the circuit output.

## The guess program (mirror referral-credit-program)
Crate `zk-guess-program` (`#[lez_program] mod zk_guess`) + methods crate `zk-guess-lez-methods`.

```rust
#[lez_program]
mod zk_guess {
    // seal: game master commits C = H(secret‖blind) into a program-owned game account.
    #[instruction]
    pub fn init_game(#[account(init, signer)] game: AccountWithMetadata, commitment: Vec<u8>)
        -> SpelResult { /* game.account.data = commitment (32B) */ }

    // turn: private (secret, blind) witnesses; public (guess, turn). Assert commitment-open,
    // compute dir, append (turn,guess,dir) to game.data. secret/blind never hit public state
    // → stay private; a swapped secret fails the open assert → unprovable (referral soundness).
    #[instruction]
    pub fn guess(#[account(mut, signer)] game: AccountWithMetadata,
                 guess: u64, secret: u64, blind: u64, turn: u32) -> SpelResult { ... }
}
```
- **Privacy split:** args used in-circuit but not written to any account stay private (secret,
  blind); args reflected into `game.data` become public (guess, dir) — exactly the referral
  pattern (recipient keys from committed data, not exposed).
- **Win + pot:** the winning `guess` (dir=EQUAL) is the settlement trigger; the pot is a separate
  program-owned account moved by the **pot program** (EPIC D, piñata pattern: `pinata.balance -=
  PRIZE; winner.balance += PRIZE`). Reveal `s` + prove `H(s‖r)=C` on the win instruction.

## Build order (F)
1. **F1 — reproduce the referral e2e first** (de-risk the toolchain): build `sequencer_service` +
   `wallet` (running now → `/extra/tmp/lez-target`), run the sequencer standalone, run
   `e2e_submit` → expect `bind ✓ · emit ✓ · redirect rejected ✓`. *Proves the whole path before my
   code.*
2. **F2 — write `zk-guess-program` + methods** (mirror referral crate layout: `Cargo.toml` with
   `spel-framework` path dep + `nssa_core` at rev `787a15aa`; `guest/` bin; `build.rs`
   `embed_methods`). Port the M1 guest logic (sha2 commitment-open + dir) into the two instructions.
3. **F3 — deploy + submit**: `wallet deploy-program`, then a `zk_guess`-flavoured `e2e_submit`
   (init_game + a guess turn); confirm inclusion via `poll_tx`/`get_block`/`get_account`; read-back
   shows game.data carrying the turn. Then wire the module's `submitGuess` to this submit path
   (replacing the local file-host) so turns settle on-zone.

## F1 — DONE ✅ (sequencer on Sneg, on-zone tx proven, 2026-08-03)
- Built `sequencer_service` + `wallet` on wild (~3 min, `CARGO_TARGET_DIR=/extra/tmp/lez-target`).
- **Sequencer runs on Sneg** at `100.108.127.3:3040` (Tailscale) / `192.168.1.36:3040` (LAN),
  bound `0.0.0.0`, standalone + `RISC0_DEV_MODE=1`. Reachable from wild (both routes).
- **Gotcha (key):** the copied binary panics at genesis on Sneg —
  `ProgramExecutionFailed("No such file or directory")` — because Sneg lacked the RISC0 runtime.
  **Fix: copy `r0vm` to Sneg and put it on PATH** (`~/lez-seq/r0vm`, from wild's
  `~/.cargo/bin/r0vm` = risc0 3.0.5). Then genesis executes fine.
- Launch: a run-script (`~/lez-seq/run.sh`, sets PATH incl r0vm + dev-mode) held up by a
  persistent ssh (`ssh sneg bash ~/lez-seq/run.sh` as a background task). Config is self-contained
  (inline genesis + signing key; bedrock mocked in standalone), `home` patched to a writable dir.
- **On-zone proof (against Sneg):** `account new` → `auth-transfer init` (block 32) →
  `pinata claim` (block 34) → **balance 150 TOK**. The whole submit→include→state path works;
  also validates EPIC D's staking (spendable TOK, no node).
- Wallet points at Sneg via `wallet_config.json` `sequencer_addr = http://100.108.127.3:3040`
  (fresh `LEE_WALLET_HOME_DIR`, dev-mode).

## F2 + F3 — DONE ✅ (guess turn settles on Sneg, 2026-08-03)
- Wrote `zk-guess-program` (`#[lez_program]`) + `zk-guess-methods` (guest ELF + `e2e_submit`) in the
  fork, mirroring referral. **Compiled first try** (one unused-import warning). Source mirrored to
  `module/zk-guess-lez/`.
- **Key realization:** privacy-preserving txs carry the program **inline** (`ProgramWithDependencies`)
  — no separate `wallet deploy-program`; `e2e_submit` submits directly.
- **Ran against Sneg** (`e2e_submit`, dev-mode): `INIT included block 99` · `GUESS(600000)=ABOVE
  included block 100 — verified on LEZ` · **wrong-secret rejected** (`Program error 4: commitment
  mismatch — sealed number was swapped`). `E2E COMPLETE`.
- **What's proven:** the guess logic runs as a real program on the zone; a turn is submitted,
  verified, and settled into a block on Sneg; the swap-soundness (commitment-open) holds on-zone.
- **Remaining for FULLY real:** `RISC0_DEV_MODE` off on both the Sneg sequencer + the prover →
  cryptographically-valid STARK receipts (heavier proving; the M1/M2 guest already proves in real
  mode). Dev-mode proves the *integration*; real-mode proves the *cryptography*.

## Honest notes
- Per-turn on-zone settlement adds latency (submit + block inclusion) — acceptable for a
  turn-based game; if slow, only stake+win settle on-zone. Default = on-zone per the decision.
- Dev-mode (`RISC0_DEV_MODE=1`) for iteration; drop it for real STARK receipts (both prover +
  sequencer must agree). Our M1/M2 already proved the real-mode guest works.
- Sequencer runs locally first (wild), then moves to Sneg (`NSSA_SEQUENCER_URL=http://<sneg>:3040`).
