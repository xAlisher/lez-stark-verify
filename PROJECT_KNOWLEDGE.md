# Project Knowledge — lez-stark-verify

Accumulated, permanent lessons about this codebase. Raw session captures live in
`docs/retro-log.md` and get folded in here at retro time, then cleared.

## Wallet (`wallet` crate, `WalletCore`) — loading, identity

- **Always check whether `storage.json` already exists before choosing a constructor.**
  `WalletCore::new_init_storage()` unconditionally builds a **blank** `Storage`
  (`last_synced_block=0`), discarding any real sync progress already on disk — every CLI
  invocation of a tool built on it will resync the *entire chain from genesis* if this is called
  unconditionally. Use `WalletCore::new_update_chain()` (which loads via `Storage::from_path`)
  whenever a prior `storage.json` exists; reserve `new_init_storage` for the true first-ever run
  of a wallet dir.

- **`create_new_account_public(None)` is not idempotent.** It always allocates a fresh,
  never-before-used slot in the key tree (`generate_new_public_node_layered()`) — it is a "mint
  another account" call, not a "get my account" call, despite reading like the latter at call
  sites that only ever want one persistent identity. To get a stable, reusable identity: check
  `w.storage().key_chain().public_account_ids().next()` first, and only mint a new one if the key
  chain is genuinely empty.

- **Never let a cosmetic post-success step discard a real success.** If an operation's
  money-moving step has already succeeded (a claim landed, a tx confirmed), any *further* fallible
  step used purely for reporting (a final balance resync, a final print) must be non-fatal
  (`.ok()`, not `?`). A transient hiccup there must not turn an already-successful operation into
  a reported failure — the caller has no way to distinguish "it failed" from "it succeeded but the
  receipt got lost" unless the code itself keeps them apart.

## QProcess output capture (`zk_guess_game_backend.cpp`, `PotOutputBuffer`)

**Never share one mutable buffer between "consume complete lines as they arrive" (for live
progress narration) and "give me the full transcript when the process exits" (for the completion
callback).** They have different lifetimes: the former needs to erase what it's already consumed
(so a burst of thousands of lines doesn't grow unbounded), the latter needs everything, including
lines the former already ate. Sharing one buffer means the incremental parser silently starves the
final snapshot of exactly the last lines (which — since every line is newline-terminated — are the
first thing consumed). `PotOutputBuffer` (`src/pot_output_buffer.h`) keeps two independent buffers:
an append-only `m_complete` for the final snapshot, and a transient `m_pending` safe to truncate as
lines are parsed. Regression test: `tests/pot_output_buffer_test.cpp` (also covers a line split
mid-word across two separate `QProcess` reads — a genuine, separate chunk-boundary concern).

This was the actual cause of a two-session-long "funding never shows in the UI" investigation
(issues #48/#49) — the underlying money mechanics were fine throughout; only the completion
signal was silently empty by the time the UI's callback ran.

## Delivery / Waku

- Use the `logos.test` preset (`relay: true`, no explicit `entryNodes`) for this module's
  `createNodeAsync` config, not `logos.dev` — the `logos.dev` fleet's cluster ID has moved before
  without notice (logos-messaging/logos-delivery#4114) and a hardcoded `entryNodes` list pinned to
  the old cluster silently disconnects every peer milliseconds after connecting. `logos.test` ships
  its own bootstrap nodes and is the network upstream commits to keeping stable
  (logos-co/logos-delivery-module#84).
