# v1 MVP — build plan (the anon guessing room, end to end)

_Decomposes the spec ([`v1-sample-app-spec.md`](v1-sample-app-spec.md) / #9) into buildable
milestones. The MVP proves the **STARK honesty loop** (seal → guess → prove → verify → win)
and the **chat UX** on real RISC0 + LEZ. Single-host trust (v1); the no-house upgrade is v2
([`v2-trustless-custody.md`](v2-trustless-custody.md))._

## MVP definition — the smallest thing that proves the idea
A player types a guess in a chat room; a **real RISC0 STARK** proves *above/below* honestly
against a **sealed** number; the receipt **verifies** (on-node + client-side); an exact guess
**reveals** the number and proves it matches the seal. Everything else (real on-chain pot,
matchmaking, malicious security) is out.

**In scope:** the compare guest + commitment-open, the prove/verify CLI, a headless game loop,
the chat-room UI wired to the real backend, module packaging.
**Out (stretch/M4):** real on-chain pot settlement (stubbed + seam documented), multiplayer
netcode, malicious security, real-value stakes.

**Guiding principle:** headless truth before UI. Prove the mechanic in a driver (M2) before
wiring pixels (M3) — trivial-experiment-first.

---

## M1 — Honesty core (the STARK)  ⭐ the crux
Extend the **proven** `module/zk-eligibility` guest (secret vs threshold → bool) into the game
oracle: **`sign(g − s)` + a commitment-open check** so the host can't lie *or* swap the number.

- **#… Guest:** private `(secret, blind)`, public `(guess, commitment)`; assert
  `H(secret‖blind) == commitment`; commit `(commitment, guess, dir)` where dir ∈ {below,equal,above};
  secret never committed. **Prove 4 ways** (verify OK · secret-not-in-journal · tampered rejected ·
  swapped-secret unprovable) — same rigor as the original.
- **#… CLI:** `prove-turn <commitment> <guess>` (writes receipt) + `verify <receipt>` → JSON
  `{valid, commitment, guess, dir}`; regenerate fixtures (valid + tampered).

**Done when:** a real receipt (dev-mode UNSET) proves above/below vs a sealed number, verifies
in ms, and the secret is provably absent from the journal.

## M2 — Headless game loop
A driver that **seals** a number and runs N turns end-to-end against the CLI — no UI.

- **#… Game driver:** seal `s` (+blind) → commitment; loop: pick/accept a guess → `prove-turn`
  → `verify` → narrow the range → detect `equal` win; print a turn transcript + timings.

**Done when:** the transcript shows a binary-search converging to the secret with every turn
STARK-verified, and a win detected + the seal opened. This *is* the mechanic, proven.

## M3 — Chat-room UI (Basecamp module)
Port the mock ([`app/guess-room.html`](../app/guess-room.html)) to a real Basecamp module driven
by the M1 backend — the proving/verify beats become live, not scripted.

- **#… QML chat view:** log + message-type delegates (chat / stake+proving / verdict / win) +
  input parser (`/guess`); proving = a **live delegate**, not a modal; `re-verify` chip.
- **#… QtRO backend + packaging:** replica à la `ZkVerifyBackend` — `submitGuess(int)`, signals
  `turnProving/turnVerdict(dir,ms)/won(secret)`; drives the CLI via `QProcess`; build the `.lgx`
  (reuse the `zk_eligibility_ui` scaffold).

**Done when:** typing `/guess` in the module walks stake → proving → LEZ-verify → verdict with a
real receipt behind it; `re-verify` re-checks locally.

## M4 — LEZ settlement (stretch)
- **#… On-zone table:** commitment + pot + turn order + automatic payout on LEZ — or a
  documented **stub** with the exact sequencer seam, if the zone plumbing isn't ready for MVP.

---

## Reuse map (most of this exists)
| Need | Already have |
|---|---|
| STARK prove/verify | `module/zk-eligibility` (proven 4×) — extend the guest |
| verify tool the UI drives | `host/src/bin/zk-verify.rs` (`gen`/`verify` → JSON) |
| backend `QProcess → verify → emit` | live in `zk_eligibility_ui` (`ZkVerifyBackend`) |
| module `.lgx` scaffold | `module/zk-eligibility-basecamp` |
| the UI itself | `app/guess-room.html` mock (delegates, states, palette) |

## Acceptance (MVP demo)
1. Seal a number → commitment shown.
2. `/guess` → real STARK proof of above/below → verified on LEZ (ms) → verdict; range narrows.
3. Tampered/inconsistent receipt **fails** verify.
4. `re-verify` → `verified locally ✓`.
5. Exact guess reveals `s`, proves it matches the seal. (Pot payout real in M4, else stubbed.)
