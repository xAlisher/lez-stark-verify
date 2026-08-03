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

## The STARK engine (full detail)
- **Guest** (`module/zk-guess/methods/guest/src/main.rs`, RISC0 zkVM): reads **private** `(secret,
  blind)` + **public** `(guess, commitment)`; asserts `SHA256(secret‖blind) == commitment`
  (a swapped number halts the guest → **unprovable**); commits `(commitment, guess, dir)` where
  `dir ∈ {0 below, 1 equal, 2 above}`. The secret **never enters the journal**. Proven 4 ways
  (verify OK · secret-absent · tampered-rejected · swapped-unprovable) — `docs/reports/mvp-headless-report.md`.
- **CLI** (`module/zk-guess/host/src/bin/zk-verify.rs`): `prove-turn <secret> <blind> <guess> [out]`
  → a receipt; `verify <receipt>` → JSON `{valid, commitment, guess, dir}` (pure — no prover, ~ms).
  `RISC0_DEV_MODE=1` = fast dev receipts (integration); unset = real cryptographic STARKs (~8 s/turn).

## The LEZ program & sequencer (full detail)
_This is the on-zone core (EPIC F). Proven on Sneg; **not yet wired into the room game** (see next)._
- **Program** (`module/zk-guess-lez/zk-guess-program/src/lib.rs`, a `spel_framework` `#[lez_program]`
  mirroring the referral `emit_credit`): `init_game(commitment)` seals `C` into a program account;
  `guess(guess, secret, blind)` = **privacy-preserving** — private `(secret, blind)` witnesses,
  asserts `SHA256(secret‖blind)==C`, computes dir, appends `(guess, dir)`. Built with
  `nssa_core`/`spel-framework` @ LEZ rev `787a15aa`.
- **Submission** (`.../zk-guess-methods/src/bin/e2e_submit.rs`): privacy-preserving txs carry the
  program **inline** (`ProgramWithDependencies`) — no `deploy-program` step. `wallet.
  send_privacy_preserving_tx(accounts, ix, &program)` → `poll_tx` → `sync_to_latest_block`. Inclusion
  in a block = the **sequencer verified the STARK** (against `PRIVACY_PRESERVING_CIRCUIT_ID`).
- **Sequencer we use:** the LEZ execution-zone **`sequencer_service`** (jsonrpsee, RPC **:3040**) —
  NOT the `:8080` cryptarchia node inscribe path. Run standalone (mock bedrock) + `RISC0_DEV_MODE=1`:
  `just run-sequencer standalone "" 3040`. **Deployed on Sneg** at `http://100.108.127.3:3040`
  (Tailscale) / `192.168.1.36:3040` (LAN). **Gotcha:** the copied binary panics at genesis without
  the RISC0 runtime → **copy `r0vm` to Sneg's PATH** (see memory `lez-sequencer-on-sneg`). Point the
  wallet via `wallet_config.json` `sequencer_addr` or `NSSA_SEQUENCER_URL`.
- **Proven on Sneg (dev-mode):** `init_game` block 99 · `GUESS(600000)=ABOVE` block 100 (verified on
  LEZ) · wrong-secret rejected. Real STARKs = `RISC0_DEV_MODE` off on both prover + sequencer.

## The trustless pot (full detail — EPIC D)
- **Token:** native **TOK** on the LEZ; the **pinata faucet** grants 150 TOK instantly spendable
  (`wallet pinata claim`), **no node** needed. Each player = one LEZ account (`wallet account new
  public` + one `auth-transfer init`).
- **Custody:** faucet TOK is auth-transfer-owned; a program can't move another program's balance
  directly → a trustless pot must be a **vault-style PDA** (balance moves via `ChainedCall` to
  auth-transfer). The LEZ **vault program already does this** → no new deploy. Proven on Sneg:
  `vault transfer A→POT 50` (blk 242) · `B→POT 50` (blk 244) · `vault claim POT 100` (blk 246) →
  winner=100. (`docs/epic-d-pot.md`.)

## Deployment & requirements — "does it work from the catalog?"
**Short answer: the GAME works out of the box; the per-turn STARK proof + on-zone settlement need a
prover (bundled or hosted) — but there IS a public sequencer, so no self-hosting.**

**Public endpoints (no self-hosting):**
- **Delivery** — the public `logos.dev` Waku network (rooms/chat/turns). Zero config.
- **LEZ sequencer** — **`https://testnet.lez.logos.co`** is a **live, hosted** LEZ testnet sequencer
  (responding, ~block 48.5k). **⚠ Version wall:** our program + wallet are built at LEZ rev
  `787a15aa` (which our **Sneg** sequencer runs and accepts — a guess turn settled in block 100),
  but a real-mode submit to the public testnet returns **`MethodNotFound`** — its RPC is a
  *different* LEZ version than ours. So on-zone settlement needs **either** rebuilding our
  `zk-guess-program` + wallet against the **testnet's** LEZ rev (identify + match), **or** keep the
  version-matched **Sneg** (private) sequencer. The public endpoint exists; the version match is the
  open deployment step.

| Feature | Works via catalog install? | Needs |
|---|---|---|
| Lobby, rooms **by code**, roster, chat, entropy, **turns**, guessing, **client-verified win** | ✅ **Yes — zero settings** | just `delivery_module` (auto-bundled); public logos.dev network |
| **Per-turn `verified on LEZ ✓`** | ✅ **bundled** (linux-amd64) | the host runs `zk-verify prove-turn` — needs `zk-verify` (~43 MB) **+ r0vm (~104 MB)**. **Both are now bundled into the `.lgx`** (82 MB gzipped): the backend resolves them next to its own plugin `.so` (`dladdr`) and sets `RISC0_SERVER_PATH` to the sibling `r0vm` — no `rzup` / `ZK_VERIFY_BIN` / PATH r0vm. Dev-mode local proof, fast. If the binaries are absent it falls back to `(unverified)` and the game still plays. |
| **On-zone settlement** (win + pot) on the **public** sequencer | ⚠ endpoint live but **version-mismatched** | `testnet.lez.logos.co` returns `MethodNotFound` for our rev-`787a15aa` submit → rebuild our program+wallet to the testnet's LEZ rev, or use version-matched **Sneg**. Then settle *infrequent* events (win + pot) on-zone (real STARK ~minutes, fine when infrequent); keep **per-turn** proofs fast (dev-mode local / prover service). |
| **TOK pot / staking** | ⚠ wire-up | LEZ wallet + `pinata` faucet + the public sequencer (all available); the vault pot flow is proven (EPIC D). |

**Recommended deployment architecture:** rooms/chat/turns on public delivery (zero-config); **per-turn
proof fast** (dev-mode local prover — bundle it, or a hosted prover service); **win + pot settle
on-zone** against the public `testnet.lez.logos.co` (real STARK, but infrequent so the minutes-scale
proving is fine). This gives real "on LEZ" value where it counts (the settlement) without making
every turn wait on a heavy proof.

**Bundling the prover (linux-amd64):** the logos module-builder's portable bundler assembles the
`.lgx` variant from the plugin `.so` + its `ldd`-traced deps only — a flake `postInstall` that drops
files in `$out` is filtered out. So the 147 MB prover is injected **post-build** by
`module/zk-guess-game/tools/bundle-prover-into-lgx.sh` (repack the `.lgx`, sha256-pinned;
`PROVER_DIR` local or `PROVER_URL` release assets). The clean flake-native path needs an extra-files
hook in `nix-bundle-logos-module-install` (upstream ask). A catalog release must run this repack step
(or that upstream hook) for the shipped `.lgx` to carry the prover.

## Repo & build
- This module: `module/zk-guess-game/` (universal ui_qml, module-builder master). Build:
  `nix build '.#lgx-portable'`, then `tools/bundle-prover-into-lgx.sh <built.lgx>` to bundle the
  prover. Install into Basecamp; needs the `delivery_module`.
- STARK engine: `module/zk-guess/` (`cargo run` proves 4-ways; `zk-verify` CLI).
- On-LEZ: `module/zk-guess-lez/` + the sequencer on Sneg (see `docs/epic-f-onlez-design.md`,
  `docs/epic-d-pot.md`, and the memory `lez-sequencer-on-sneg`).
- Issues: parent **#18**; A #21 · B #22 · C #23 · D #20 · E #24 · F #19 · groups #25.
