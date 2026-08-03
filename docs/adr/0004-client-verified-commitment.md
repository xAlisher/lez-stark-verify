# ADR-0004 — `SHA256(secret‖blind)` commitment + client-verified win

**Status:** accepted

## Context
The game needs the number to be **immutable** from the moment play starts, and a win to be
**verifiable by every player** without trusting the host — and without an on-zone round-trip on the
happy path (ADR-0001 keeps play fast).

## Decision
Seal the number with a **hash commitment**: `C = SHA256(secret_le ‖ blind_le)`, broadcast up front;
the `blind` (a random nonce) stops anyone brute-forcing the 0…1e6 space of `secret` from `C`.
- **Immutability** — `C` is public before any guess; the guest asserts `SHA256(secret‖blind)==C`, so
  the host cannot answer as if the number were anything other than the sealed one (a swap makes the
  turn unprovable, ADR-0001).
- **Client-verified win** — on an exact guess the host reveals `(secret, blind)`; **every client**
  independently checks `SHA256(secret‖blind)==C` before accepting the win. No trust in the host, no
  sequencer needed for the result.

The commitment is computed **byte-identically** in three places, or the system is broken: the QML
backend (`QCryptographicHash`), the RISC0 guest, and the on-LEZ program. `secret`/`blind` are hashed
as little-endian `u64`.

## Consequences
- Provably-fair win with zero on-zone dependency on the common path; on-zone settlement (ADR-0003) is
  an optional notarization on top, not a requirement for correctness.
- The three implementations are a **coupling hazard** — any change to the byte layout must land in all
  three at once. Covered by the guest's 4-way proof tests.
