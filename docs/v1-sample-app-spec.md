# v1 sample app — the anon guessing room (STARK per-turn, LEZ verify)

_The STARK deliverable (Franck's ask: "sample app to show zk proof verification on LEZ,
ideally STARK") as a **cypherpunk chat room**. A number is sealed; players guess privately;
each turn a **RISC0 STARK** proves above/below honestly and the **LEZ node verifies it**;
exact guess takes the pot._

- **Reference mock:** [`app/guess-room.html`](../app/guess-room.html) (interactive; the
  proving/verify beats are animated, secret hardcoded, NPCs scripted).
- **Honesty core reused:** the proven `module/zk-eligibility` guest (comparison → journal,
  verified 4 ways). v1 = host model; the no-house upgrade is [`v2-trustless-custody.md`](v2-trustless-custody.md).
- **Trust model (v1):** the host holds `s` — trusted **not to leak/play**, but **not** for
  honesty (that's the STARK). Commit-reveal + mouse-draw entropy make "the host hand-picked a
  number for a friend" un-suspectable. This is the zkMastermind trust model — proven prior art.

---

## 1. Why chat-first (the design thesis)

STARK proving takes ~1–2 s per turn. In a button/board UI that's dead latency. **In a chat
room it becomes the game:** the wait is when players **bluff and poke each other**. Guesses
are private, so the *only* information channel about who's close is **what people say — and
they can lie.** The proving beat is the poke window.

Everything the aesthetic does is load-bearing, not decoration:
- anonymous handles + monospace + inline system lines = a real cypherpunk channel;
- **teal = proven/verified, amber = stake/proving** (the two-signal palette: the math vs the
  money, always visually separated);
- the **✓ is a trust primitive** — click `re-verify`, your own client re-checks the receipt
  → `verified locally ✓`. You trust the room because you can *check* it, not because you know
  anyone.

---

## 2. The room (one screen, three regions)

```
┌ header ─────────────────────────────────────────────────────────────┐
│ ⌗ guess   ● 4 anon   pot ▲ 45          🔒 sealed 0x8f3a…c1 · immutable │
├ log ───────────────────────────────────────────┬ rail ──────────────┤
│ — room #guess · range 0–1,000,000 · sealed 🔒   │ you know           │
│ null_ptr  someone has to open. watch this       │  0 … 750,000       │
│ ▓ null_ptr staked 5 · proving ⣾ 1.3s            │  [====------]      │
│ mallory   while you wait — i heard it ends in 8 │ pot ▲ 50           │
│ ✓ null_ptr  500000  BELOW  · LEZ 4ms [re-verify]│ in the room …      │
│ anon_7f3c below 500k noted. thanks 🫡           │ legend …           │
├ input ──────────────────────────────────────────┴────────────────────┤
│ you ›  type to chat…   /guess 573118 to play                          │
└───────────────────────────────────────────────────────────────────────┘
```

### Message / component types
| Type | Render | Carries |
|---|---|---|
| **chat** | `hh:mm  nick  text` (nick colored) | the bluff/poke channel |
| **system** | dim, `— … —` | joins, round start, "your move", seal |
| **stake+proving** | amber left-border, live spinner + elapsed | a turn *in flight* (STARK generating) — stays in the log while chat flows |
| **verdict** | teal left-border, `✓ nick  num  ABOVE/BELOW  · LEZ Nms` + `re-verify` chip | the resolved turn (direction only; number private for others) |
| **win** | teal glow, `★ nick EXACT · pot→nick · secret … ✓ matches seal` | payout + honest reveal |

### Rail (state at a glance)
- **you know** — your personal `lo … hi` range (carved by your own verdicts) + a bar.
- **pot** — amber, big; stake/turn.
- **in the room** — the anon handles.
- **legend** — the two-signal key + "guesses stay private, only verdicts hit the log".

---

## 3. The turn loop (state machine)

```
SEAL      round starts → commit-reveal + mouse-draw entropy → s sealed,
          commitment C = H(s‖r) posted to LEZ. (host holds s)
IDLE      players chat; turn order advances
GUESS     player types /guess g → g committed client-side (never leaves as plaintext
          to opponents); stake debited → pot += stake   [amber "staked" line]
PROVE     host runs the guest: assert H(s‖r)==C ; b = sign(g − s) ;
          commit (C, g_or_hash(g), b) → RISC0 STARK receipt        [amber "proving…" line]
VERIFY    receipt → LEZ node: verify(receipt, image_id)  (ms)      [flips to teal ✓]
          + any client may re-verify locally (the chip)
SETTLE    verdict appended (direction only); ranges update
WIN       b == EQUAL → guest reveals s, proves H(s‖r)==C → LEZ pays pot → winner  [win line]
          → re-seal, next round
```

**What's proven each turn (the guest):** consistency with the commitment (`s` is the sealed
one, unchanged) **and** the direction bit — so the host **cannot lie** about above/below and
**cannot swap** the number. The secret stays out of the journal; only `(C, g, b)` is committed.

**What lives on LEZ:** the commitment `C`, the pot & stakes, every per-turn **verify**, and
the automatic **payout**. (Verify is cheap → it's the node-runnable, demo-relevant half; prove
is heavy → host/prover, off the hot path. See [`research.md`](research.md).)

---

## 4. Build components

| Piece | Basis | Work |
|---|---|---|
| **Guest program** | extend `module/zk-eligibility` guest (already: private value vs public threshold → journal) | add commitment-open check + emit sign bit `{above,below,equal}` instead of a bool; equal-path reveals `s` |
| **Prover/host CLI** | `host/src/bin/zk-verify.rs` pattern (gen/verify) | `prove-turn <C> <g>` → receipt; `verify <receipt>` already exists |
| **Chat-room view** | new QML view (cf. `zk_eligibility_ui`'s `ZkVerifyView.qml`) | log model, message-type delegates, input parser (`/guess`), rail; **proving state = a live delegate**, not a modal |
| **Backend** | QtRO replica like `ZkVerifyBackend` (`.rep` slots/signals) | `submitGuess(int)`, signals `turnProving(handle)`, `turnVerdict(handle, dir, ms)`, `won(handle, secret)`; drives the prover via `QProcess`, verify via the tool |
| **Table state** | LEZ | commitment, pot, turn order, settle/payout (the sequencer path) |
| **Entropy ritual** | mouse-draw → hash → commit-reveal | fair seed; nobody suspects a hand-picked number |

**Reuse note:** the *verify* side (backend `QProcess` → `zk-verify verify` → parse JSON →
emit signal) is exactly what `zk_eligibility_ui` already does and we've run live. v1 mostly
adds the **compare guest**, the **chat delegates**, and the **turn/pot/table** state.

---

## 5. Scope

**In v1:** one sealed number, host-holds-`s` + honest per-turn STARK, chat + bluff, private
guesses (from opponents), staking/pot, exact-wins + honest reveal, local re-verify, mouse-draw
entropy.

**Out (→ later):** no-house custody (v2, [`v2-trustless-custody.md`](v2-trustless-custody.md) —
threshold-FHE keeps STARK); malicious-secure multiplayer; anti-collusion; real-value stakes;
matchmaking/lobby persistence.

---

## 6. Acceptance (demo)

1. Room seals a number; header shows the commitment; players chat.
2. `/guess` walks **stake → proving (live) → verified on LEZ → verdict**, chat flowing
   throughout; the guesser's range narrows.
3. A tampered/inconsistent receipt **fails** verify (reuse the `tampered.receipt` fixture path).
4. `re-verify` on any turn re-checks locally → `verified locally ✓`.
5. An exact guess reveals `s`, proves it matches the seal, pays the pot; re-seal.

---

## 7. Open questions

- **Guess privacy mechanism** — commit-reveal per guess vs. a shared/committed input so the
  *host* also doesn't see `g` in the clear (matters more in v2).
- **Turn model** — strict round-robin vs. free-for-all with a rate/stake gate.
- **Prover placement** — host process now; a dedicated prover service if the compare guest
  grows (it's tiny today → seconds).
- **Anti-grief** — VDF/time-lock reveal of `s` if a round is abandoned (shared with v2).
