# ADR-0006 — `delivery_module` (Waku) transport + best-effort idempotency

**Status:** accepted

## Context
Rooms, roster, chat, turns, guesses, and verdicts all flow over the Logos `delivery_module` — Waku
pub/sub on the public `logos.dev` network, keyed by a StationCrypto-derived topic. Waku is
**best-effort**: messages can be **dropped, reordered, or duplicated**. Naive state that trusts
one-shot, in-order delivery corrupts under this.

Two live bugs proved it:
1. A dropped `turn` message left a player's `currentTurnId` stale forever → no slider, **game stuck**.
2. A re-delivered `guess` (after its verdict already cleared the spinner) re-set `provingName` to an
   already-resolved guess → **spinner stuck** with no second verdict coming.

## Decision
Make the room state **converge regardless of delivery order/duplication**, host-authoritative:
- **Turn** — the host **re-broadcasts the current turn every 5 s**; clients already in sync no-op
  (change-guarded setter), a client that missed the message recovers within 5 s.
- **Guess/proving** — ignore a `guess` whose value is **already resolved** in the turns feed (kills
  late/duplicate re-sets and duplicate host verdicts); clear the proving spinner on a **real
  turn-advance** (new `currentTurnId`), not on the 5 s re-broadcast.
- Payload decode is tolerant (base64 envelope *or* raw JSON) so a differently-encoded delivery isn't
  silently dropped.

## Consequences
- Turn and proving state are eventually-consistent under best-effort delivery — no permanent stalls
  from a single dropped/duplicated message.
- Recovery latency is bounded by the 5 s re-broadcast, not instant. Acceptable for a party game.
- A *missed guess the host never receives* still stalls until re-guess — a deeper reliability gap that
  MLS-group rooms (pending a delivery-module bump) would address with richer membership/delivery.
