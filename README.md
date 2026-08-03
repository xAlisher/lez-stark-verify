# lez-stark-verify — ZK Guess

A provably-fair, multiplayer number-guessing party game on the full Logos stack — and the R&D
that proves **STARK proof verification on the Logos Execution Zone (LEZ)** is real, on hardware we
control. Rooms over **Logos Messaging**, per-turn honesty via **RISC0 STARK** proofs, and win
settlement on our **own public LEZ sequencer**.

> Originating goal (Franck, EcoDev, 2026-07): investigate zk-proof verification on LEZ (ideally
> STARK), stage resource-capped experiments on Sneg, and build a module + sample app on top. This
> repo productized that into a playable game whose honesty *is* the STARK.

## The pitch
A host secretly holds a number. Players take turns guessing; each guess is answered
**"higher / lower"** — but the answer ships with a **zero-knowledge STARK proof** that it's honest
*relative to a commitment sealed up front*. So the host **can't lie** about the direction and
**can't change the number** mid-game. The winning reveal is checked against the seal. *The house
can't cheat, and it's fun to feel why.*

## How it works (the flow)
1. **Lobby** — pick a name (prefilled with a 3-word handle derived from your key). Start a new
   game or join one by **invite code**.
2. **Room** — the code derives a private Waku topic (StationCrypto: Argon2id + XChaCha20). A live
   roster + chat sync over Logos Messaging. Host's **Start** unlocks at ≥2 players.
3. **Entropy** — everyone **mouse-draws**; the host folds all contributions (+ its own committed
   seed) into the number via SHA-256, so **no single player picks it**.
4. **Seal** — the host commits `C = SHA256(secret ‖ blind)` and broadcasts **only `C`**. The number
   is now immutable.
5. **Turns** — players guess in turn order with a range-bounded slider. Each guess goes over the
   room topic; the host proves it (below) and broadcasts the verdict; everyone narrows their range.
6. **Win** — an exact guess reveals `secret + blind`; every client checks
   `SHA256(secret‖blind) == C`. Provably-fair win. Optionally, **settle the win on the LEZ** (a real
   STARK on our sequencer) — see below.

---

## The STARK, in detail

**Guest** (`module/zk-guess/methods/guest`, RISC0 zkVM). Reads **private** `(secret, blind)` and
**public** `(guess, commitment)`. It:
- asserts `SHA256(secret ‖ blind) == commitment` — if the host swapped the number, this assertion
  fails and the guest **halts → no proof can be produced** (a lie is *unprovable*, not just
  detectable);
- computes `dir ∈ {0 below, 1 equal, 2 above}` by comparing `guess` to `secret`;
- **commits `(commitment, guess, dir)`** to the journal — the secret **never** enters the journal.

The commitment `SHA256(secret_le ‖ blind_le)` is computed **identically** in three places that must
agree byte-for-byte: the QML backend (QCryptographicHash), the RISC0 guest, and the on-LEZ program.

**CLI** (`module/zk-guess/host/src/bin/zk-verify.rs`):
`prove-turn <secret> <blind> <guess> [out]` → a receipt; `verify <receipt>` →
`{valid, commitment, guess, dir}`. **Verify is pure** (no prover, ~ms). **Prove** needs `r0vm`.

**Two proving modes** — this is the crux of the UX (see [ADR-0001](docs/adr/0001-two-tier-proving.md)):
| | Per-turn guess proof | Win settlement |
|---|---|---|
| where | host, **local**, bundled `zk-verify` | winner's machine → **sequencer.logos.live** |
| RISC0 mode | **dev-mode** (`RISC0_DEV_MODE=1`) | **real** (unset) |
| cost | **~1–3 s** | **two real proofs, ~30–40 min** (host-dependent) · ~9.6 GB each |
| meaning | fast, playable "verified on LEZ ✓" every turn | two real STARKs (seal + winning guess) verified on-zone, once per game |

Dev-mode receipts are fast but not cryptographically valid — right for a snappy party game where a
lie is still caught by the same guest assertion. Real settlement is where the STARK becomes
on-chain truth; it's **infrequent and non-blocking**, so the ~30–40 min (two proofs, and it scales
with the host machine) never gates play.

## The LEZ, in detail

The win (and, later, the pot) can settle **on-zone** on the **Logos Execution Zone**:

- **Program** (`module/zk-guess-lez/zk-guess-program`, a `spel_framework` `#[lez_program]`, LEZ rev
  `787a15aa`): `init_game(commitment)` seals `C` into a program account; `guess(guess, secret,
  blind)` is **privacy-preserving** — private `(secret, blind)` witnesses, asserts
  `SHA256(secret‖blind)==C`, appends `(guess, dir)`. Same soundness the referral `emit_credit`
  program uses.
- **Submission** carries the program **inline** (`ProgramWithDependencies`) — no `deploy-program`
  step. Inclusion in a block = the **sequencer verified the STARK** against
  `PRIVACY_PRESERVING_CIRCUIT_ID`.
- **Prove-side vs verify-side split** — the heavy proving (**~16 min / 9.6 GB per proof**, two per
  settlement) happens **in the submitting wallet (client)**; the **sequencer only verifies** the
  receipt (seconds, low RAM). So a
  public sequencer runs on a modest box.
- **Our sequencer:** **`https://sequencer.logos.live`** — our own standalone LEZ sequencer, real
  verification mode, version-matched to rev `787a15aa`. We run our own because the public
  `testnet.lez.logos.co` is a *different* LEZ version and rejects our program (`MethodNotFound`).
  See [ADR-0003](docs/adr/0003-own-public-sequencer.md) and `docs/sequencer-deploy-plan.md`.

**Proven end-to-end, real mode, over the public endpoint:** `INIT` → block 110,
`GUESS(600000)=ABOVE` → block 181 *verified on LEZ*, wrong-secret **rejected at proving**.

---

## Install & run — "does it work from the catalog?"

**Yes, zero-config.** The `.lgx` **bundles the prover** (`zk-verify` + `r0vm`, ~147 MB, ~82 MB
packed) beside the plugin, so per-turn `verified on LEZ ✓` works out of the box — no `rzup`, no
`ZK_VERIFY_BIN`, no PATH `r0vm` (see [ADR-0002](docs/adr/0002-bundle-prover.md)). Rooms/chat/turns
run on the public `logos.dev` Waku network. Only **real on-zone win settlement** needs the sequencer
URL + auth, supplied via env.

**Install the signed release (Linux x86-64)** — `zk_guess_game v0.1.1`, ✓ Signed by xAlisher:
- Download the `.lgx` from
  https://github.com/xAlisher/lez-stark-verify/releases/tag/zk_guess_game-v0.1.1
- Install via Basecamp's Package Manager / `lgpm` (no `--allow-unsigned` needed). Needs the
  `delivery_module` (auto-resolved). The STARK prover **and** the on-zone `settle-win` binary are
  bundled in the `.lgx`, so real settlement works out of the box.

**macOS (Apple Silicon / darwin-arm64)** — `zk_guess_game v0.1.1`, ✓ Signed by xAlisher:
- Download `zk_guess_game-0.1.1-darwin-arm64.lgx` from the same release above and install it the same
  way. Same bundled prover + `settle-win`. Build recipe:
  [`module/zk-guess-game/docs/MACOS-BUILD-PROTOCOL.md`](module/zk-guess-game/docs/MACOS-BUILD-PROTOCOL.md).

**Build from source:**
```
cd module/zk-guess-game
nix build '.#lgx-portable'
tools/bundle-prover-into-lgx.sh <built.lgx>   # inject the prover (via lgx add — hashes stay valid)
# then sign + install the resulting .lgx into Basecamp
```

## Repo layout
```
module/zk-guess-game/   the game — universal ui_qml + QtRO backend (rooms, turns, loaders, settlement)
module/zk-guess/        the STARK engine — RISC0 guest + zk-verify CLI (prove-turn / verify)
module/zk-guess-lez/    the on-LEZ program (#[lez_program]) + submission path
app/                    HTML prototypes (guess-room, verifier)
docs/                   design docs, the sequencer deploy plan, and docs/adr/ (decision records)
experiments/            capped RISC0 runs on Sneg (the feasibility envelope)
```

## Status
Real + confirmed across isolated Basecamps: rooms by code, roster + chat, distributed-entropy seal,
turn order, per-turn guessing with the range slider + proto-style proving spinner, client-verified
win, win screen, **bundled per-turn STARK**, and **real-mode win settlement on
`sequencer.logos.live`** — the win now settles on-zone and the UI shows the actual settlement
**tx hash + block** (verified live: block 1649, tx `e7491f6c…f2259`). The prover is capped to half
the cores so the machine stays usable while it proves. Settlement is currently two proofs
(`init_game` + winning guess); collapsing to one, the TOK pot (EPIC D, proven standalone on Sneg),
and per-turn on-zone settlement are the next wire-ups. Decision records: [`docs/adr/`](docs/adr/).

## Environment
Sneg (farm build box) runs capped experiments; the RISC0 toolchain lives on `/extra`. Never build on
`/` (it's small). See `docs/GAME-DESIGN.md` for the full handoff.
