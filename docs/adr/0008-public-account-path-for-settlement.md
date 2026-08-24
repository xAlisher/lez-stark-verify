# ADR-0008 — Settlement runs on the public account path

**Status:** accepted (2026-08-24)
**Supersedes in part:** the private-account choice implicit in the original `settle_win`
**Relates to:** #38, #27, #16, ADR-0001, ADR-0003

## Context

`settle_win` — the binary the game backend shells out to for "settle the win on the LEZ" — stopped
compiling against our own program when EPIC D landed the pot. `init_game` grew from
`{ commitment }` to `{ commitment_bytes, room_id, host_addr, host_bps, deadline_block }` and now
also creates a program-owned **pot PDA**. `settle_win` still called the pre-pot signature.

This was invisible for weeks because the file was untracked and had never been compiled by anyone
(#38, fixed by `0908ecc`). It is not a LEZ-version problem: the same errors reproduce at the old
`15144ddb` pin.

Fixing it forced a decision that had never been made explicitly. The old `settle_win` created its
game account with `create_new_account_private` and submitted with a single private account. The pot
PDA that `init_game` now requires is **public by construction** — it is moved by ChainedCall to
auth-transfer, which is the only trustless custody the zone offers (proven on Sneg, blocks
242/244/246). So the settlement path and the pot path were on **two different account models**, and
a private-path settlement could never carry the public pot account.

Three further facts bear on the choice:

1. **The secret is protected by the STARK, not by account visibility.** `secret` and `blind` are
   private *inputs* to the guest. The journal commits only `(commitment, guess, dir)`. Nothing about
   making the account public puts the secret on chain.
2. **`C` is already public.** The protocol broadcasts the commitment to every player at seal time
   (README, step 4). A public game account holding `C ‖ turn log` discloses nothing the room does
   not already have.
3. **The private path is the defective one.** Upstream (via `corpetty/muster`,
   `BUG-private-transfer-recipient-identifier.md` §6) has confirmed a panic on the first foreign-path
   send after an auth-transfer init, a permanently-bricking `register_private_account`, a
   permanently-sync-killing key-chain import, and a `resolve_private_account().identifier` field that
   always reads `0`. Every one of those lives on the private path. See #39, #40.

## Decision

**`settle_win` and `e2e_submit` submit on the public account path**, mirroring the pattern already
proven by `zkg_pot` / `pot_e2e`:

- the game account comes from `create_new_account_public`;
- the pot is `AccountId::for_public_pda(&program_id, &PdaSeed::new(room_id))`;
- accounts go in as `AccountIdentity::Public(game)` + `AccountIdentity::PublicNoSign(pot)`;
- `room_id = SHA256("/zkg/settle/room/v1" ‖ C)` — deterministic, and distinct per sealed number so
  two settlements cannot collide on the same PDA;
- `host_bps = 0`, `deadline_block = u64::MAX` — this pot exists only because `init_game` creates one.
  It is never staked into and never settled, so it takes no cut and cannot expire.

This is **option 1** of the three in #38 (settlement seals its own throwaway game). It is explicitly
a stopgap for the account model, not the F↔D integration.

## Consequences

**Good.**
- The shipped settlement path compiles and runs again.
- Settlement and the pot are now on the **same account model**, which is the precondition for ever
  binding them (#27). On the private path that binding was not expressible at all — so this is not
  merely a fix, it unblocks the last mile.
- It routes settlement away from every one of the four confirmed private-path defects.
- One fewer wallet API in use: no `create_new_account_private`, no `resolve_private_account`.

**Costs, stated plainly.**
- Each settlement leaves an **orphan zero-value pot PDA** on chain. Harmless, but it is litter, and
  it is the honest price of option 1.
- The game account and its turn log are publicly readable. Per (1) and (2) above this leaks nothing
  the room does not already know — but it is a real change in what an outside observer can see, and
  should not be waved away. Anyone who wants settlement unlinkable to a room needs option 2 or 3.
- `settle_win` still seals a *throwaway* game rather than settling the room's real one. The payout
  therefore remains manual (`vault claim`). **This ADR does not close the F↔D gap; it makes closing
  it possible.**

## Alternatives rejected

- **Keep the private path and derive a private pot PDA** (`for_private_pda(pid, seed, npk, vpk, id)`).
  Technically expressible from the Rust wallet, but it walks straight into the defect cluster above,
  and it keeps settlement structurally unable to touch the public pot. Rejected.
- **Split `init_game` into a pot-free seal + a separate `init_pot`** (#38 option 2). Cleaner, and
  probably right eventually — but it changes the deployed program, which is a bigger blast radius
  than a stopgap warrants while the v0.2.4 bump is still unproven on chain.
- **Go straight to real F↔D binding** (#38 option 3). That is the destination, but it needs the
  auth-transfer panic tested first (#39) and the chain acceptance test passing. Doing it now would
  stack an unverified design on an unverified bump.
