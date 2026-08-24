# ADR-0009 — Pick the LEZ pin by dating the chain, not by taking the newest tag

**Status:** accepted (2026-08-24)
**Relates to:** #32, #33, #36, #34, ADR-0003

## Context

Our fork pins LEZ by git rev. Getting that pin wrong is not a build problem — it is the
`MethodNotFound` wall: if our `PRIVACY_PRESERVING_CIRCUIT_ID` differs from the one the live chain
verifies against, every privacy-preserving tx is rejected. ADR-0003 exists *entirely* because of
that mismatch: we stood up our own version-matched sequencer rather than fix the pin.

The compat epic (#32) was written around the sentence *"v0.2.1 … matches the live 0.2.1 chain
exactly."* That sentence was an assumption, and it was wrong. Acting on it would have produced a
bump that compiled, looked complete, and changed nothing — the wall would have survived and #34
would have stayed blocked, with no obvious signal that the premise was the problem.

Two heuristics were available and both are unreliable:

- **"Take the newest tag."** Newest is not what the chain runs. A chain genesised on v0.2.2 does not
  care that v0.2.4 exists.
- **"Take the tag whose name matches the testnet's name."** The "0.2.1 testnet" was running v0.2.2.
  Release naming and deployed code drift apart.

The authoritative fact — `PRIVACY_PRESERVING_CIRCUIT_ID` — is a risc0 build artifact. It is **not
checked into the LEZ tree**, so it cannot be compared from source without building each candidate.

## Decision

**Date the chain, then pick the release whose window contains that date.** Concretely, and with no
build required:

1. `getLastBlockId` on the sequencer to confirm it answers and to bound the range.
2. `getBlock` for an early block (2 is reliable; 1 may carry no timestamp) and for a recent one.
   Blocks come back borsh-encoded base64; scan the decoded bytes for a little-endian `u64` in
   plausible unix-ms range. Two matches at the same value is the timestamp.
3. Early timestamp = **genesis**. A LEZ testnet re-genesises on release, so genesis dates the
   deployed release. Recent timestamp confirms it is the same continuous chain, not a stale read.
4. Compare against `git tag --sort=creatordate` with `git log -1 --format=%ad <tag>`. The release
   whose date window contains genesis is what is running.
5. **Then** widen if it is free: `git diff --name-only <chain-tag> <newer-tag>`. If `lee/` is
   untouched and the range has no `!` commits, the circuit is unchanged and the newer tag costs
   nothing.

Applied 2026-08-24: genesis **2026-08-05 19:06 UTC**, continuous to block 21474 the same day this
was written. Tags: v0.2.1 08-02, v0.2.2 08-04, v0.2.3 08-06, v0.2.4 08-07. **The chain is v0.2.2.**
All three circuit-breaking commits (`09f4bfdd`, `3a5d8050`, `f16c3bd1`) first appear in v0.2.2 — so
v0.2.1 sat *behind* the chain across the circuit boundary. `lee/` is byte-identical v0.2.2→v0.2.4
with zero breaking commits, so **v0.2.4** was chosen: same circuit as the chain, current code.

Independently corroborated: `corpetty/muster`, driving the same testnet with the same wallet FFI,
cites `WalletConfig` as *"LEZ v0.2.2"* and builds *"the LEZ v0.2.2 localnet stack"*.

## Consequences

- The pin is chosen from evidence about the deployed chain, not from a release-notes reading.
- Cheap: four JSON-RPC calls and some `git log`. No build, no funded wallet, no proving.
- It is **indirect**. The genesis window says which release was *deployed*; it does not read the
  circuit ID. So it narrows the candidate set to one — it does not prove the match.
- **The proof stays empirical**: a privacy-preserving tx accepted by the sequencer. That remains the
  acceptance criterion on #33, and this method never replaces it — it just stops us from burning a
  build cycle on an obviously-wrong candidate first.
- Reusable. LEZ re-genesises on release, so this recurs; #36 is the standing watch, and step 5 is
  what makes "has the circuit actually changed?" a one-command question.

## The transferable lesson

The original premise was never measured, only asserted, and it was load-bearing for an entire epic.
`corpetty/muster` learned the same lesson harder on this stack: they reproduced a bug two independent
ways before discovering the *instrument* — a defaulted `identifier` field that always read `0` — was
what both measurements had in common. Their phrasing is worth keeping:

> Repeating a result is not verifying an instrument.

Ours is the cheaper cousin: **check the premise before building on it, especially when checking is
four HTTP calls and building is an afternoon.**
