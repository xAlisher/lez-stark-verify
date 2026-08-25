# Retro Log

## [fail] 2026-08-25
to work autonomously

## [fail] 2026-08-25
"no honest state, no resolving" root-caused as TWO real bugs in `zkg_pot.rs::open_wallet`, not a UI
perf issue. (1) unconditionally called `WalletCore::new_init_storage`, which always builds a BLANK
`Storage` (last_synced_block=0) even when storage.json already had real sync progress -- so every
single CLI invocation resynced the entire chain from genesis (confirmed: 22831 blocks, ~18-90s+ per
call, paid again on every retry inside the fund loop). (2) `create_new_account_public(None)` is NOT
idempotent -- it always mints a brand-new never-before-used account, so every action (including the
`bal` check meant to detect "already funded") talked to a DIFFERENT on-zone address each time, leaving
5 orphaned accounts in one iso's wallet dir. My first two hypotheses (Rust println! buffering,
`QByteArray::remove` O(n^2) in the C++ readyReadStandardOutput handler) were both directly disproved
by measurement (a live piped test, and a 20k-line microbenchmark showing 1ms vs 0ms) before I stopped
guessing and instrumented the actual running process (ps/proc wchan/fd, back-to-back CLI repro).
Lesson: when two plausible-sounding perf theories both fail to reproduce, stop theorizing about the
C++ side and go straight to reproducing the exact failure via the same binary + same env the app uses --
that took two `zkg_pot bal` calls back-to-back to nail both bugs conclusively.

## [fail] 2026-08-25
Two more real bugs found live during the second play-test round, both fixed:
- Relaunching the 3 isos myself (after the #49/#49b fix) dropped `ZKG_POT_BIN` from the
  environment, so `potBinary()` fell back to a bundled path that was never packaged --
  fund silently no-opped (lastError WAS set correctly, just easy to miss; the deeper issue
  is my own relaunch command was incomplete, not a new protocol bug). Fixed by relaunching
  with the full original env.
- **#50** -- `zkg_pot fund`'s final `w.sync_to_latest_block().await?` runs AFTER the claim
  already landed on-chain; a transient hiccup there returns Err and the process exits
  non-zero WITHOUT ever printing `addr`/`balance` -- discarding a real, already-successful
  claim. Confirmed live: iso3's claim genuinely landed (150 TOK, verified independently)
  while the app still showed "Fund from faucet". Fixed to match `bal`'s already-resilient
  `.ok()` pattern, and hardened `fundOnZone()`'s failure path in C++ to re-verify real
  chain balance via `checkExistingFunding()` before accepting a reported failure -- never
  trust a CLI exit code alone for money.

## [fail] 2026-08-25
User correctly caught a real contradiction: I could verify on-chain that all 3 iso wallets are
genuinely funded (balances climbing normally with every successful claim -- 750->900 TOK etc,
addresses stable, sync fast -- the underlying money mechanics from #49/#49b/#50 are solid), but the
UI never once displayed "my balance" or hid the "Fund from faucet" button across many rounds, even
right after `checkExistingFunding()` should have run at room entry. Added an always-on "potBusy" +
"potBusyLabel" honest-state banner (driven from launchPot() start/finish, not just the fund/stake
states) specifically to make this diagnosable live instead of guessing blind. Still unresolved:
whether `checkExistingFunding()`'s own `bal` launchPot call is actually running at room entry at
all, or running but its success branch (setOnZoneFunded/setMyBalance) isn't reaching the QML-bound
property for some deeper reason. Next step: watch for `zkg_pot bal` process spawns specifically
(only checkExistingFunding triggers that action) at the moment of room entry, to settle whether it
fires at all before looking further at the property-sync layer.
