# Halt — 2026-08-24 · zk-guess: fork source pushed (loss risk closed); compat re-aimed v0.2.1 → v0.2.4

> **Reconciled 2026-08-24** against `git`, `gh` (both repos), the ecodev wetware board, upstream
> LEZ/spel tags, and live sequencer probes. This supersedes the 2026-08-07 halt, which had passed
> its shelf life. Nothing has been built, played, or committed here since 2026-08-07.

## ▶ Resume this session

```bash
cd /home/alisher/lez-stark-verify && claude
```

No prior session has this repo as cwd (all zk-guess work ran from `~/ecodev`, which pivoted to
docs_assistant long ago — do **not** resume it). Read this file + `README.md` + `docs/adr/`.

## Where we stopped

Mid-flight on **EPIC D — the TOK pot** (#20/#27). The seal-heal + pot self-heal fixes are
**written, compiled, committed and pushed** (`dbfc847`) but **never played**. The one thing standing
between here and closing #29/#30/#31 is a **3-client play-test** — wetware, correctly routed, not
done. Separately, **LEZ compat (#32) was aimed at the wrong tag**: it targeted v0.2.1, but the live
testnet runs **v0.2.2**, so the planned bump would not have closed the `MethodNotFound` gap it
exists to close. Re-targeted to **v0.2.4** this session (same circuit as the chain, current code).

## Current state — verified 2026-08-24

**Repo `xAlisher/lez-stark-verify` (PRIVATE)**
- **Branch:** `feat/tok-pot` — **in sync with `origin/feat/tok-pot`** (0 ahead / 0 behind).
- **HEAD:** `dbfc847` "fix(room,pot): seal-heal + pot self-heal" (2026-08-07). `master` = `5e837a9`.
  `fix/roster-turn-sync` == `master` (merged, deletable).
- **Working tree clean** except untracked `halt.md` (this file) and
  `module/zk-eligibility/{.gitignore,LICENSE}` (a stub, no source).
- ⓘ `module/zk-guess-game/src/zk_guess_game_backend.cpp` has an mtime after 2026-08-07 but its
  **content matches HEAD** — that is the reverted one-line nix-sensitivity probe from the Aug-7
  session. **No uncommitted work was lost.** (The Aug-7 halt's "three uncommitted files" list is
  dead — do not resurrect it.)
- **Build:** `module/zk-guess-game/result` → `/nix/store/9gs77abw2fj3gmcic2jhcxi82gmd7ixh-logos-zk_guess_game-module`
  (symlink dated Aug 7 05:11). The heals **do compile**. They have **never run in front of a human**.
- **Released:** `zk_guess_game` **0.1.1**, signed + in the catalog. Bundles at
  `~/zk_guess_game-0.1.1-{linux-amd64,darwin-arm64}.lgx` (+ 0.1.0 and a `-test` build).
- **PRs: none, ever.** **Open review: none.**

**Board — 25 open.** No issue opened or closed since 2026-08-04, and no new comments since the Aug-7
🧫 wetware comment on #30. The only edits since are **this session's** (2026-08-24): #33 re-targeted
to v0.2.4 + evidence comment, #36 re-scoped, #32 annotated with the premise correction.

**Wetware routing is live and correct.** `lez-stark-verify#30` is labelled `wetware-required`,
carries the 🧫 comment with the three checks, and sits **unchecked** on the canonical cross-repo
board `xAlisher/ecodev#27` (last touched 2026-08-11). `#27` here is also `wetware-required`.

## What actually changed since the last halt

1. **The compat epic was aimed at the wrong tag — and it would have failed silently.**
   LEZ shipped `v0.2.2` (08-04) · `v0.2.3` (08-06) · `v0.2.4` (`47eba256`, **08-07**), then went
   quiet. More important than the tag list: **the live testnet is on v0.2.2, not v0.2.1.**
   `testnet.lez.logos.co` re-genesised **2026-08-05 19:06 UTC** (decoded from the block-2 borsh
   timestamp) and has run continuously since (block 21474 @ 08-24 09:47 UTC) — a genesis that falls
   **after v0.2.2 and before v0.2.3**. All three circuit-breaking commits (`09f4bfdd`, `3a5d8050`,
   `f16c3bd1`) are **first contained in v0.2.2**. So #33's old "bump to v0.2.1" target would have
   left the `PRIVACY_PRESERVING_CIRCUIT_ID` gap standing: `MethodNotFound` survives, #34 stays
   blocked, and the epic looks done while changing nothing.
   **Target is now `v0.2.4`** — `git diff v0.2.2 v0.2.4` touches only `lez/{indexer,sequencer,storage}`,
   `integration_tests`, CI and lockfiles; **`lee/` is byte-identical** and the range has **zero
   breaking commits**, so v0.2.4 is the same circuit as the live chain plus current code.
   ✅ **Board updated 2026-08-24:** #33 re-targeted + evidence comment, #36 re-scoped to "watch
   releases after v0.2.4", #32 carries a premise-correction comment.
2. **spel upstream is NOT a rebase pressure.** `logos-co/spel` main is only **4 commits** ahead of
   our fork base `0cb7e09`, **none LEZ-related** (release plumbing, an idl-gen path-dep fix, a lint
   table, a CLI scaffold URL). `v0.6.0-rc.2` was tagged **today (2026-08-24)** — release plumbing
   only. The fork can bump LEZ without rebasing first.
3. **The Sneg sequencer is DOWN.** `100.108.127.3:3040` → connection refused, while the host itself
   **pings fine (1.4 ms)** — so this is the sequencer process, not the network. Previously the known
   failure was "panics at genesis without `r0vm` on `PATH`"; now it isn't listening at all. Anything
   that assumes a local zone (capped experiments #5/#6, the Sneg custody proof) needs it restarted.
4. **`sequencer.logos.live` is UP** — HTTP 405 "POST is required" in 0.2 s, i.e. the JSON-RPC is
   alive. Still on rev `787a15aa`, i.e. pre-0.2.1 (see #34).

## ✅ Loss risk CLOSED — 2026-08-24

**The fork's zk-guess source is now committed and pushed.** `~/basecamp/forks/spel-fork-dev-repin`
branch `compat/bump-lez-0.2.1` → **`0908ecc`** "feat(zk-guess): commit the program + methods source",
pushed to `gh` = `xAlisher/spel-lez-dev-repin` (**private**). 14 files, 14,879 lines:
`zk-guess-program/src/lib.rs` (seal/guess honesty core + the TOK pot), `zk-guess-methods/src/lib.rs`,
six bins (`e2e_submit`, `settle_win`, `pot_e2e`, `pot_derisk`, `zkg_pot`, `zkg_builder_setup`), the
RISC0 guest, `build.rs`, and the lockfiles. **Verified by `git ls-tree` against the remote ref**, not
just by the push output. It is no longer single-disk.

- **Not claimed:** that it builds. Nothing was compiled — preservation only. The first real build is
  #33's job, and the v0.2.4 bump will rewrite these lockfiles anyway.
- **Deliberately left unstaged:** `referral-methods/Cargo.lock` and `referral-methods/guest/Cargo.lock`.
  Their working-tree drift **re-pins LEZ back to `787a15aa`**, i.e. it would partially revert this
  branch's own bump. Left dirty on purpose — decide during #33, don't commit them absent-mindedly.
- The copy in `module/zk-guess-lez/` here is still a read-only mirror with a dangling
  `../spel-framework` path dep that **never builds** — that is still #37, now with a live upstream
  to point at.

## Next steps (in order)

1. ~~Push the untracked fork source.~~ **DONE 2026-08-24** (`0908ecc`, pushed + verified on the
   remote). See above.
2. **Do the bump to `v0.2.4`** (the board is already re-aimed — see above; this is the work
   itself). Bump `spel-ffi-compile-test` first as the compile canary, then the workspace, in one
   hop. Keep the `package=` aliases (#35 — permanent). Expect churn in `spel-cli/src/pda.rs:134`,
   `account_inspect.rs:66`, `zk-guess-methods/src/bin/*`, plus artifact regeneration from the
   v0.2.2 action-struct refactor. **Acceptance = a PP tx from `zk-guess-program` accepted by
   `testnet.lez.logos.co`** (no `MethodNotFound`) — that is the real circuit-ID match; the
   genesis-window argument above is strong but indirect. If v0.2.4 is rejected, fall back to
   v0.2.2 (`lee/` is identical, so a rejection would point elsewhere).
3. **Play-test the two heals with ≥3 clients** — `wetware`, already routed (#30 + ecodev#27).
   The three checks are in the 🧫 comment on #30. This is the only thing that can close #29/#30/#31.
4. **Restart the Sneg sequencer** if any local-zone work is planned (needs `r0vm` on `PATH`).
5. **Close or re-scope #27** (EPIC D productization).
6. After the bump lands: **#34** retire the own-sequencer workaround (ADR-0003), **#37** decide the
   `zk-guess-lez` mirror's fate.

## Blockers / wetware

- **Multi-client play-test needs a human** — timing/roster race, no headless proof exists.
  On the board, tagged, not stalling anything else.
- **#27** is `wetware-required`.
- **Real settlement is ~30–40 min** (two real STARKs, ~9.6 GB each) — never in a tight verify loop.
- **mac builds = wetware** (M1 box).

## Context that's hard to re-derive

- **Custody model (EPIC D, learned the hard way):** faucet TOK sits in *auth-transfer-owned*
  accounts and a program cannot move another program's balance — the sequencer rejects it. A
  trustless pot must be **vault-style**: a program-derived PDA moved only via **ChainedCall** to
  auth-transfer. The LEZ **vault program already does this**, so the pot needs **no new program
  deploy**. Proven on Sneg: two 50-TOK stakes into the PDA (blocks 242/244) → `vault claim` 100 to
  the winner (block 246). The direct-balance `stake`/`settle` still in `zk-guess-program` is the
  **naive same-program variant**.
- **The honest gap in EPIC D:** payout is still triggered *manually* (`vault claim` by the pot
  owner). Binding settle to the on-zone win proof — only a proven EQUAL guess releases the pot — is
  the **F↔D integration** and is the last mile. Custody and real-funds movement are done; the
  binding is not.
- **Two proving modes are a deliberate ADR** (`docs/adr/0001`): per-turn = **dev-mode**, ~1–3 s,
  host local, *not cryptographically valid* — a lie is still caught because the guest assertion
  makes it **unprovable**. Real STARKs are only for win settlement. Don't "fix" dev-mode into the
  turn loop.
- **The commitment `SHA256(secret_le ‖ blind_le)` is computed in three places that must agree
  byte-for-byte:** the QML backend (`QCryptographicHash`), the RISC0 guest, and the on-LEZ program.
  Change one → change all three.
- **Sequencers:** public `sequencer.logos.live` (Hetzner, real mode, rev `787a15aa`, **UP**) and
  Sneg `100.108.127.3:3040` (**DOWN** as of today; needs `r0vm` on `PATH` or it panics at genesis).
  Both predate 0.2.1 — see #34.
- **Repo is PRIVATE** (`xAlisher/lez-stark-verify`) — keep it out of external comms.
- Origin: Franck/EcoDev ask (2026-07-31) — STARK verification on LEZ, productized into a game.
  Epics #1–#4 (research → capped Sneg experiments → module → app) are all still open.

## Session 2026-08-24 (later) — Phase 0 implemented

- **#38 done, option 1, on the PUBLIC account path** — fork `71f9a4d` on `compat/bump-lez-0.2.4`.
  `settle_win` + `e2e_submit` now build. Recorded as **ADR-0008**; the load-bearing reason is that
  settlement and the pot were on *different account models*, and a private-path settlement could
  never carry the public pot account — so this is the precondition for the F↔D binding (#27), not
  just a compile fix. Costs recorded: an orphan zero-value pot PDA per settlement, public turn log.
- **ADR-0009** — how to choose the LEZ pin: date the chain from its genesis block, don't take the
  newest tag. The method that caught #32's wrong premise.
- **Two environment fixes that block *any* guest build** and were not written down anywhere:
  - `~/.risc0` was a **dangling symlink** to `/extra/tmp/referral-risc0/risc0-home` (cleaned tmp dir,
    from 2026-07-29). `rzup` failed with `Io("File exists (os error 17)")` on every invocation.
    Repointed to `/extra/tmp/risc0-home`. ⚠ `/extra` root is **not user-writable** — only `/extra/tmp`
    — so this stays exposed to the same tmp-cleaning that broke it.
  - **`r0vm` was never installed.** `rzup install rust` (1.97.0) + `rzup install r0vm` (3.0.6, matching
    `risc0-zkvm 3.0.6`). Without it every real proof dies at `ProgramProveFailed("No such file or
    directory")`.
- **Dev-mode run against testnet confirmed the new plumbing** end to end up to proving: wallet init,
  default config authored by the SDK, program image id, public game account, **pot PDA derivation**.
- **Real-proving acceptance run in flight** at the time of writing (debug build — see caveat below).

⚠ **The in-flight run is a `cargo build` *debug* binary.** The risc0 guest is always compiled in
release, but the **host-side prover is not** — so its wall-clock is *not* a valid measurement for
#41. Correctness yes, timing no. Measure with `--release`.

## ✅ CHAIN-VERIFIED — 2026-08-24

**#33's acceptance test PASSED.** A privacy-preserving tx from `zk-guess-program` was accepted by
`testnet.lez.logos.co` and is on chain:

```
tx 3716ad3decd491da58b11ecddea265e68eb2d24491e5054b90257c1ff914eca0
```

Confirmed by `getTransaction`, **with the instrument controlled** — a bogus hash returns `null`,
ours returns the transaction. (That control is the point: #32's whole premise had been unverified,
and muster's parallel investigation went wrong by trusting an unchecked instrument.)

**What it settles:** the `MethodNotFound` wall is gone; the circuit matches; ADR-0009's
genesis-dating method was right; the original v0.2.1 target would have **failed** this test. And
**#34 is unblocked** — the run used the public testnet, not `sequencer.logos.live`, so ADR-0003's
only justification is gone.

**#44 — found by the same run, and it matters.** `settle_win` exited non-zero
(`Transaction not found in preconfigured amount of blocks`) *after* the tx had been accepted:
`seq_tx_poll_max_blocks: 5` against ~75 s testnet blocks, following minutes of proving.
**It reported failure for a settlement that succeeded** — muster's 20 s-sync-wrapper failure shape
one layer up. Widened to 60 (`ZKG_POLL_BLOCKS`) in fork `9a9ea26`. The real cure is to resolve from
the tx hash and treat poll expiry as *unknown*, never failure (#42).

## Plan + new issues — 2026-08-24

`docs/plan-to-working.md` (`d5c6f60`, pushed) defines the finish line and orders the work; **#43** is
its phase tracker. New issues filed from it, most of them findings bought by `corpetty/muster`
(`origin/main` `5c59d0f`) rather than by us:

- **#38** — `settle_win`/`e2e_submit` don't compile against the pot-era `InitGame`. Pre-existing
  (proved at the old pin), and it **blocks #33's on-chain acceptance test**.
- **#39** — the auth-transfer → foreign-path send panic is our pot's exact shape. Test before Phase 3.
- **#40** — wallet landmines: `register_private_account` bricks an account id **forever**, importing a
  key chain makes sync die **permanently**, `resolve_private_account().identifier` is always 0.
  ✅ verified we call neither; the issue exists to keep it that way.
- **#41** — measure the real settlement cost; muster measured 6m41s for a comparable proof against our
  unmeasured 30–40 min claim.
- **#42** — settlement must never be a sync module call: the SDK wrapper's 20 s timeout returns an
  error **while the zone keeps proving**, and the next call queues behind it.
- Comments re-scoping **#28** (only half a wall — the public/pot path *is* module-reachable via
  `send_generic_public_transaction`; only the private path is blocked) and **#34** (testnet is the
  default and works — but write no wallet config).

**Board is now 31 open** (25 + #38–#43).

## ✅ Mechanics fully proven on testnet — 2026-08-24 (later)

All three headless mechanics verified end to end, real proving, public testnet, fork `ef3fd12`:

| mechanic | result |
|---|---|
| win settlement (`settle_win`) | EXIT=0, 19m04s, 2 real proofs, both txs confirmed on chain |
| soundness (`e2e_submit`) | EXIT=0, 17m45s, 3 real proofs — guest halts on a swapped commitment |
| full pot lifecycle (`pot_e2e`) | EXIT=0, ~50min, 7 real proofs — 193/100/5/2/0 |

The pot result was **independently confirmed via direct `getAccountBalance` RPC** against all five
accounts (winner, B, host, builder, pot PDA), not trusted from the harness's own printout.

**Two more real bugs found and fixed:**
- **#44** — the SDK's default poll window (5 blocks, ~75s testnet blocks) races real proving and
  reports failure for settlements that land. Fixed across all five tx-submitting bins.
- **#45 (closed)** — `BUILDER_ADDR`'s registration didn't survive the 2026-08-05 testnet re-genesis
  (a settle_win in-guest panic: "was modified but not claimed"). Fresh account minted, rebaked,
  `zkg_builder_setup` made idempotent + mnemonic-persisting so this class of loss can't recur.
  **Generalizable finding:** a re-genesis invalidates every baked account *registration*, not just
  the LEZ pin — same failure class and root cause as the private-sequencer problem (ADR-0003).

**Closed on GitHub:** #38, #41, #45. **Board is now 28 open** (31 − 3 closed).

## Board (28 open, verified 2026-08-24)

`#37` mirror fate · `#36` watch releases after v0.2.4 *(re-scoped 08-24; v0.2.2 folded into #33)* ·
`#35` `package=` aliases permanent · `#34` retire own-sequencer *(unblocks only if #33 targets
v0.2.2+)* · `#33` bump LEZ pin → **v0.2.4** *(re-targeted 08-24)* · `#32` EPIC LEZ-compat
*(title still says 0.2.1; see its correction comment)* · `#31/#30/#29` room sync bugs *(the pushed heals aim here; #30 is
`wetware-required`)* · `#28` DevKit gap: no module exposes custom-program LEZ tx submission ·
`#27` EPIC D productization `wetware-required` · `#25` MLS-group room (blocked on a delivery-module
bump) · `#24` EPIC E win screen & payout · `#22` EPIC B entropy · `#18` EPIC 7 umbrella · `#17`
live game · `#16` M4 LEZ settlement · `#10` v1 MVP · `#9` v1 spec · `#7` EPIC 5 trustless v2 ·
`#6` reproduce baseline · `#5` capped harness · `#4/#2/#1` original epics.

---

_Halt shelf life ~2 weeks (`~/fieldcraft/protocols/halt-resume.md`) → recheck after **2026-09-07**.
Past that, re-run: `git status` + `git rev-list --left-right --count origin/feat/tok-pot...HEAD`,
`gh issue list`, the fork's untracked list, LEZ tags, and both sequencer probes._
