# ADR-0001 — Two-tier proving: per-turn dev-mode, win real-mode on-zone

**Status:** accepted

## Context
A RISC0 STARK for our guess guest costs **~16 min / ~9.6 GB peak** to prove in real mode, and ~ms to
verify. A number-guessing party game has *many* turns and *one* win. If every turn had to wait ~16 min
for a real proof, the game would be unplayable. But if nothing is ever a real STARK, "verified on LEZ"
is theatre.

Two facts make a split possible:
1. The guest's honesty assertion (`SHA256(secret‖blind)==C`, halt on mismatch) holds in **dev-mode
   too** — a lying host still can't produce *any* receipt for a swapped number. Dev-mode weakens the
   *cryptographic* receipt, not the game's logic.
2. The heavy proving is **client-side**; on-zone verification is cheap. So on-zone settlement is only
   expensive for whoever submits, and only when they choose to.

## Decision
Prove in **two tiers**:
- **Per turn** — the host proves locally with the **bundled `zk-verify` in dev-mode**
  (`RISC0_DEV_MODE=1`), ~1–3 s. Every turn shows `verified on LEZ ✓` and plays instantly.
- **Win settlement** — **opt-in**, **real mode** (`RISC0_DEV_MODE` unset), submitted to our LEZ
  sequencer (ADR-0003). It runs **in the background** (non-blocking): the win screen shows the winner
  immediately; a countdown + spinner track the ~16 min proof, resolving to `settled on LEZ ✓ block N`.

## Consequences
- The game is snappy; a real STARK still lands where it matters (the settled outcome), once per game.
- Per-turn `verified on LEZ ✓` is honest about game logic but **not** a real cryptographic receipt —
  documented as such (README, GAME-DESIGN). Anyone who wants per-turn real proofs pays the per-turn
  cost; that's a config, not a rewrite.
- The 16 min is never on the critical path — "you can leave, the block lands whether or not you watch."
- Follow-up: a dedicated single-tx `settle-win` binary bound to the actual game's `(secret, blind, C)`
  so settlement proves *this* game's number in ~16 min flat (today's opt-in path reuses the e2e binary).
