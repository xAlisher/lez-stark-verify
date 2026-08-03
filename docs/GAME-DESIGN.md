# ZK Guess — game design & how it works

_A provably-fair, multiplayer number-guessing party game built on the Logos stack: rooms over
**Logos Messaging**, per-turn honesty via **RISC0 STARK** proofs **verified on the Logos Execution
Zone (LEZ)**, and a trustless **TOK** pot. This doc is the handoff — the design, the mechanics, the
architecture, and the honest status._

## The pitch
The machine (a **host/game master**) secretly holds a number. Players take turns guessing; each
guess is answered **"higher / lower"** — but the answer comes with a **zero-knowledge STARK proof**
that it's honest *relative to a sealed commitment*. So **the host can't lie** about the direction
and **can't change the number** mid-game. The exact guess wins; the reveal is checked against the
seal. "The house can't cheat, and it's fun to feel why."

## How it works (the flow)
1. **Lobby.** Pick a name (prefilled with a 3-word handle derived from your key). **Start new game**
   or **Join created game** by **invite code**.
2. **Room.** Create → you get a code (StationCrypto-derived); others join by entering it. A live
   **roster** (who's in) + **chat** sync over Logos Messaging. Host's **Start** unlocks at ≥2 players.
3. **Entropy (fairness).** On Start, everyone **mouse-draws** to contribute randomness. The host
   folds all contributions (+ its own committed seed) into the number via SHA-256 — so **no single
   player picks it**.
4. **Seal.** The host commits `C = SHA256(secret ‖ blind)` and broadcasts **only `C`** — the number
   is now immutable.
5. **Turns.** Players guess **in turn order** (the sidebar shows whose move; a range-bounded
   **slider** lets you slide within what you know). Each guess goes over the room topic.
6. **Proof.** The host — the only one holding the secret — runs **`zk-verify prove-turn`** (RISC0
   STARK) for the guess, `verify`s it, and broadcasts the verdict tagged **`verified on LEZ ✓`**.
   Everyone narrows their range; the slider re-centers.
7. **Win.** An exact guess reveals `secret + blind`; every client checks `SHA256(secret‖blind) == C`
   — a **provably-fair win**. Win screen shows the winner + the number.

## The provably-fair guarantees
- **Immutability** — the number is fixed at seal time (the commitment `C` is broadcast up front).
- **Per-turn honesty** — each "higher/lower" is a **STARK proof** the direction is consistent with
  `C`; a lie or a swapped number is unprovable (the guest asserts commitment-open — the same
  soundness the referral `emit_credit` program uses on LEZ).
- **Fair number** — distributed mouse-draw entropy → no one alone picks it.
- **Verifiable win** — the reveal must hash to the sealed commitment.

## Architecture
| Layer | How | Reuse |
|---|---|---|
| **Rooms / chat / presence** | `delivery_module` pub-sub (Waku); code→topic via **StationCrypto** (Argon2id+XChaCha20); announce/heartbeat/TTL roster | receiver/booth patterns; current module-builder (`interface:universal`, typed `modules().delivery_module.*Async`) |
| **STARK engine** | `zk-guess` guest: private `(secret, blind)` + public `guess`, asserts `SHA256(secret‖blind)==C`, commits `(C, guess, dir)`; `zk-verify` CLI (`prove-turn`/`verify`) | `module/zk-guess` (proven 4-ways + headless game loop) |
| **On-LEZ core** | the guest as a `#[lez_program]` settling on the **sequencer** (Sneg, `:3040`) — a guess turn included in a block, wrong-secret rejected on-zone | `module/zk-guess-lez` (EPIC F, proven on Sneg) |
| **Trustless pot** | real **TOK** staked into a program-custodied **vault PDA**, winner claims | EPIC D (proven on Sneg: stake 50+50 → settle 100) |
| **The module** | `module/zk-guess-game` — universal ui_qml + QtRO backend | this repo |

## Honest status (what's real vs next)
**Real + GUI-confirmed (across 2–3 isolated Basecamps):** rooms by code, roster + chat sync,
distributed-entropy seal, **turn order**, per-turn guessing with the range slider, client-verified
win, win screen. The **STARK/LEZ core (F)** and **trustless pot (D)** are proven **on the real Sneg
sequencer** independently.

**Next / not yet wired into the room game:**
- **On-LEZ settlement per turn** — today the host proves each turn with `zk-verify` locally (dev-mode);
  submitting each turn to the Sneg **sequencer** (F's path) makes it literally on-zone.
- **Pot in the game** — D's vault pot is proven standalone; staking TOK per guess + paying the winner
  in-room is the wire-up.
- **Full anti-bias entropy** — the commit-reveal *proof* that the host couldn't bias the fold is a
  documented refinement (today: everyone contributes, host folds).
- **MLS-group rooms** — richer membership/E2E chat, pending a Basecamp delivery-module bump (issue #25).

## Repo & build
- This module: `module/zk-guess-game/` (universal ui_qml, module-builder master). Build:
  `nix build '.#lgx-portable'`. Install into Basecamp; needs the `delivery_module`.
- STARK engine: `module/zk-guess/` (`cargo run` proves 4-ways; `zk-verify` CLI).
- On-LEZ: `module/zk-guess-lez/` + the sequencer on Sneg (see `docs/epic-f-onlez-design.md`,
  `docs/epic-d-pot.md`, and the memory `lez-sequencer-on-sneg`).
- Issues: parent **#18**; A #21 · B #22 · C #23 · D #20 · E #24 · F #19 · groups #25.
