# Plan — from here to a working ZK Guess

_Written 2026-08-24. Supersedes nothing; complements `halt.md` (state) and the epics (#18, #32)._

## What "working" means here

One sentence, so the finish line stops moving:

> **Three people on the public testnet join a room by code, play a provably-fair game to a win, and
> the pot pays out to the winner automatically because the win was proven on-zone — with no manual
> `vault claim`, no private sequencer, and no step that only works on Alisher's disk.**

Everything below is ordered by what that sentence is blocked on, not by what is interesting.

## Where we actually are

| Piece | State |
|---|---|
| Room / entropy / seal / turns / win | **Works** — released as `zk_guess_game` 0.1.1, signed, in the catalog |
| Per-turn honesty (dev-mode STARK) | **Works** — ~1–3 s, deliberate (ADR-0001) |
| Seal-heal + pot self-heal | **Committed, compiles, never played** (`dbfc847`) — #30, wetware |
| LEZ v0.2.4 compat | **Compiles + CHAIN-VERIFIED** (`c703c60`) — PP tx accepted by testnet 2026-08-24 |
| On-zone win settlement | **PROVEN on testnet** (fork `ef3fd12`) — 2 real proofs, 19m04s, tx confirmed on chain |
| Soundness (wrong secret) | **PROVEN on testnet** — guest halts in-guest, no receipt possible for a lie |
| Pot 3-way split (fund→stake→win→settle) | **PROVEN on testnet** — 193/100/5/2/0, verified via direct RPC — payout still **manual** |
| Own sequencer (`sequencer.logos.live`) | Up, but pre-0.2.1 — the workaround we want to retire (#34) |
| Sneg sequencer | **Down** (port refused, host pings) |

The gap between rows 4–6 is the whole job. **The game works; the money and the proof don't meet
yet — but every individual mechanic they'd need to meet is now proven on chain (2026-08-24).**

---

## What Muster already learned (don't re-pay for it)

`~/basecamp/refs/muster` (pulled to `origin/main` `5c59d0f`, 2026-08-24) has been driving the same
`testnet.lez.logos.co` with the same wallet FFI since early August, and filed a bug report that the
LEZ team lead answered in detail. Its `docs/labbook/lez-core-error-conventions.md` and
`demo/poc/BUG-private-transfer-recipient-identifier.md` are worth reading in full before touching
the wallet path. The load-bearing findings:

**1. It independently confirms the v0.2.2 target.** Muster's labbook cites `WalletConfig` as
*"LEZ v0.2.2, `lez/wallet/src/config.rs`"* and its toolchain notes build *"the LEZ v0.2.2 localnet
stack"*. That is a second, unrelated source agreeing with the genesis-window dating in #33 —
the live chain is v0.2.2-era, **not** the v0.2.1 the fork was pinned to. The re-target was right.

**2. Two calls cause *permanent*, unrecoverable damage. We are clean — keep it that way.**
- `register_private_account` **initializes** the account, and an account id can be foreign-initialized
  exactly once, ever. Muster called it once and made that account id permanently uncreditable.
- A wallet built by **importing a key chain** (`import_private_account`) panics on sync when a payment
  arrives at an identifier not in the import — *before* the synced-block marker is written, so the
  block replays and every later sync dies identically. Permanent.

  ✅ **Checked 2026-08-24: `zk-guess-methods` calls neither.** Every wallet is `new_init_storage`
  (fresh) or mnemonic-restore (`zkg_pot`) — precisely the two modes upstream says are safe. Do not
  "improve" peer setup by importing keys, and do not add a registration step.

**3. `wallet_ffi_resolve_private_account` returns `identifier: 0` for every account.** The field is
defaulted and never populated. Muster built an entire (wrong) bug report on it and reproduced it two
ways before discovering the *instrument* was broken. `e2e_submit`/`settle_win` call
`resolve_private_account` — they don't read `identifier`, so we're fine, but never trust that field.

**4. A confirmed panic whose trigger we may sit right on top of.** The send path hardcodes the
destination as foreign with no nullifier secret key while a separate step fills a membership proof
for any account the wallet recognises; they disagree and an `expect` fires. Trigger: *the destination
identifier is already in the sending wallet's key chain with a cached state whose commitment is on
chain* — which **the first foreign-path send after an auth-transfer init already satisfies.**
Our pot moves funds by **ChainedCall to auth-transfer**. Treat this as a live hazard in step 3 and
test for it explicitly rather than discovering it during a demo.

**5. Real measured proving cost: 6 min 41 s** for one shielded transfer at ~1300% CPU (13 cores),
testnet, 2026-08-11. Our docs claim ~30–40 min for two real STARKs. Both can be true (custom guest vs
builtin transfer), but it means **our number is an assumption, not a measurement** — measure it once
before designing UX around it.

**6. The SDK's sync call wrapper has a hardcoded 20 s timeout**, and worse than failing, it returns
an error *while the zone keeps proving*, so the next blocking call queues behind that work and the UI
looks hung on a stale stage. Any settlement path that goes through a sync module call is broken by
construction.

**7. A concrete lead for #28.** Muster's `demo/GAPS.md` documents that `lez_core` exposes
`send_generic_public_transaction` and `token_elf`/`ata_elf`, and that a client can drive a custom
program on the **public** path by building RISC0-serde instruction encoding (`instruction: any` is a
`Vec<u32>`, not CBOR) and program ids (image ids of the ELFs). The **private** path is *not*
expressible through LIDL v0.4.0. Our pot is public-path (PDA + vault), so **#28 is not a hard wall
for the pot** — only for the privacy-preserving guess. That splits #28 into a doable half and a
blocked half.

---

## The plan

### Phase 0 — Make the chain answer us (unblocks everything)

The v0.2.4 bump compiles but is unproven, and the two bins that could prove it don't build (#38).

1. ~~**Decide #38.**~~ **DONE 2026-08-24** — option 1, implemented on the **public** account path
   (fork `71f9a4d`, [ADR-0008](adr/0008-public-account-path-for-settlement.md)). Both bins build.
   The account-model unification is the part that matters: settlement and the pot are now on the
   same path, which is the precondition for step 8. Two environment fixes were needed to get here —
   `~/.risc0` was a dangling symlink into a cleaned tmp dir, and `r0vm` was never installed.
2. ~~**Run the acceptance test** from #33.~~ **DONE 2026-08-24 — PASSED.** A privacy-preserving tx
   from `zk-guess-program` was accepted by `testnet.lez.logos.co` and is on chain:
   `tx 3716ad3decd491da58b11ecddea265e68eb2d24491e5054b90257c1ff914eca0`, confirmed by
   `getTransaction` **with a bogus-hash control returning `null`**. The `MethodNotFound` wall is
   gone, ADR-0009's dating method is vindicated, and the v0.2.1 target we re-aimed off would have
   failed this test. **No fallback to v0.2.2 needed.**
   The run also exposed **#44**: the client reported failure for a settlement that had succeeded
   (`seq_tx_poll_max_blocks: 5` vs ~75 s blocks) — fixed, and the deeper treatment is #42's.
3. **Restart the Sneg sequencer** only if a local zone is still wanted (needs `r0vm` on `PATH`).
   Muster's evidence suggests it may no longer be needed at all — see Phase 1.

**Done when:** a tx hash from our program appears in a testnet block. **✅ MET.**

**Beyond the minimum acceptance, all three headless mechanics are now proven end to end on
testnet** (2026-08-24, fork `ef3fd12`): `settle_win` (19m04s, 2 proofs, tx confirmed on chain),
`e2e_submit`'s soundness check (17m45s, 3 proofs — a swapped commitment makes the guest halt with
no receipt), and `pot_e2e`'s full lifecycle (~50min, 7 proofs — fund via Pinata mining → init_game
→ stake×2 → record_win → settle_win → 3-way split 193/100/5/2/0, independently confirmed via direct
`getAccountBalance` RPC, not just the harness's own printout). Two real bugs found and fixed along
the way: **#44** (the default 5-block poll window races real proving and reports false failures —
fixed across all five submitting bins) and **#45**, closed (`BUILDER_ADDR`'s registration didn't
survive the 2026-08-05 re-genesis — fresh account minted, `zkg_builder_setup` made idempotent).
**#38 and #41 are closed** as a result — see their comments for the full evidence.

### Phase 1 — Retire the private sequencer (#34)

ADR-0003 stood up `sequencer.logos.live` purely because the circuit ID didn't match the chain.
Phase 0 removes that reason, and Muster shows `WalletConfig`'s *default* already points at
`testnet.lez.logos.co` and works.

4. Repoint `settle_win` (and the module's `settleOnLez`) at `testnet.lez.logos.co`.
5. **Write no wallet config.** Let `WalletConfig::from_path_or_initialize_default` author it —
   Muster lost a debugging cycle to a hand-written flat `{"sequencer_addr": …}` that doesn't
   deserialize and makes `create_new` return null. Override only via the struct shape if at all.
6. Close #34, update ADR-0003 with "why it existed, why it's gone".

**Done when:** no zk-guess path references our own sequencer.

### Phase 2 — Confirm the game still plays (#30, wetware, runs in parallel)

7. **Three clients, one room, the three checks** in the 🧫 comment on #30: a stalled entropy
   submitter still gets sealed within a prune tick; a player whose envelope is dropped is sealed
   anyway; a late joiner sees the pot panel; the creator has no "Place bet".

This needs hands and is already routed (#30, `ecodev#27`). It does not block Phases 0–1 and should
be run against whatever build is current when someone has the time.

**Done when:** #29/#30/#31 close on observed behavior, not on "it compiles".

### Phase 3 — Bind the payout to the proof (the actual last mile)

This is the F↔D integration — #27, #24, #16, and #38 option 3 are all this one thing.

8. **Make settlement act on the room's real game and pot**, not a throwaway: a proven EQUAL guess is
   what authorizes the pot release. Today the proof and the payout are two unrelated events joined by
   a human running `vault claim`.
9. **Test the auth-transfer panic (finding 4) explicitly** before wiring it in: an owned-path or
   auth-transfer init followed by a foreign-path send, in one wallet, on testnet. If it fires, it
   fires in the pot path, and it is better found by a test than by three people mid-game.
10. ~~**Measure the real settlement cost once**~~ **DONE 2026-08-24** — 19m04s for two real proofs,
    measured on the release build, both txs confirmed on chain. README and ADR-0001 corrected
    in place (#41, closed). Both prior published figures (~30–40 min, ~16 min/proof) were
    overestimates.

**Done when:** a win moves TOK to the winner with nobody typing `vault claim`.

### Phase 4 — Make settlement survivable as UX (#24)

11. **Settlement is a job, not a wait** — never a sync module call (finding 6). Kick it off, persist
    the pending state, show progress, let the app close and come back to it. Muster's phrasing is the
    right frame: *"a private payment is not an interaction you wait on, it is a job you start."*
12. **Win screen** shows: the reveal check, the on-zone tx hash, and the payout state as three
    separate truths — they now complete at very different times.

**Done when:** a settling game can be closed and reopened without losing the settlement.

### Phase 5 — Optional: pull the pot into the module (#28)

Only if the CLI shell-out becomes the thing that hurts. Finding 7 says the public path is reachable
from a module via `send_generic_public_transaction` + hand-built RISC0-serde encoding + ELF image ids.
The guess path stays out-of-process regardless (private path isn't expressible in LIDL v0.4.0), so
this removes one of two shell-outs, not both. **Re-scope #28 to say that** — it is currently written
as a flat wall, and it is only half a wall.

---

## Ordering, honestly

Phase 0 → 1 → 3 → 4 is the critical path. Phase 2 is parallel and human-gated. Phase 5 is optional.

**The one thing that could invalidate this plan:** if the Phase 0 acceptance test fails at v0.2.4
*and* at v0.2.2, then the circuit mismatch isn't a version-pin problem at all and #32's whole premise
needs re-opening. That test is cheap and it is first for exactly that reason.

## Sources

- `~/basecamp/refs/muster` @ `origin/main` `5c59d0f` — `docs/labbook/lez-core-error-conventions.md`,
  `demo/poc/BUG-private-transfer-recipient-identifier.md` §6, `demo/GAPS.md`, `demo/RUNBOOK.md`
- This repo: `halt.md`, `docs/adr/0001`, `docs/adr/0003`, issues #24 #27 #28 #30 #32 #33 #34 #38
- Fork: `~/basecamp/forks/spel-fork-dev-repin` @ `compat/bump-lez-0.2.4` `c703c60`
