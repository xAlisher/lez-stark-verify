# zk-guess-lez — the guess game as a real #[lez_program] (EPIC F)

Source mirror of the crates built in the working fork (`~/basecamp/forks/spel-fork-dev-repin/`,
which has `spel-framework` + the LEZ git deps @ rev 787a15aa). Built + run against the **Sneg**
sequencer — see `docs/epic-f-onlez-design.md`.

- `zk-guess-program/` — `#[lez_program] mod zk_guess`: `init_game` seals C=SHA256(secret‖blind)
  into a game account; `guess` = private (secret,blind) + public guess, commitment-open assert
  (swap → unprovable), appends the turn.
- `zk-guess-methods/` — embeds the guest ELF (`ZK_GUESS_ELF`) + `e2e_submit.rs` (init → guess →
  wrong-secret-rejected). Mirrors the referral `emit_credit` e2e.

**Proven on Sneg (dev-mode):** INIT block 99 · GUESS(600000)=ABOVE block 100 (verified on LEZ) ·
wrong-secret rejected. Real-STARK mode = `RISC0_DEV_MODE` off on both sequencer + prover.

Build: `cd zk-guess-methods && CARGO_TARGET_DIR=/extra/.. RISC0_DEV_MODE=1 cargo build --release`.
Run: `LEE_WALLET_HOME_DIR=<fresh> NSSA_SEQUENCER_URL=http://100.108.127.3:3040 ./e2e_submit`.
