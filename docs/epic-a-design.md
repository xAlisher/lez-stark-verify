# EPIC A — networked room, on CURRENT tooling + MLS groups (locked design)

_Synthesis of three explorations (latest module-builder, current delivery API, deep group dive).
The room is an **MLS chat group** (membership + E2E chat + crypto identity), joined via a
**code→owner-adds handshake**, with a **raw delivery topic** for fast turn state. Issue #21._

## Tooling (current — do NOT clone stale scorched-earth)
- **module-builder `6ef42ea8`** (2026-07-01, master) — my receiver/booth already use it. Bump off `38ddf92`.
- **Universal ui_qml pattern**: `metadata.json` → `type:ui_qml`, `interface:universal`,
  `main:<name>_plugin`, `view:qml/Main.qml`, `dependencies:[delivery_module]`, `codegen:{rep:src/<name>.rep}`.
  Backend derives `LogosUiPluginContext`, includes `logos_sdk.h`, uses **typed `modules().delivery_module.*Async`**.
  Scaffold: `nix flake init -t github:logos-co/logos-module-builder#ui-qml-backend`. **Base on
  `~/basecamp/modules/receiver-basecamp`** (freshest working reference).
- **Delivery threading rule (critical):** ui-host is single-threaded → **sync `createNode` deadlocks**.
  Receiver's recipe: `createNodeAsync` (ignore cb) → `startAsync` on `QTimer::singleShot(~3s)` →
  `subscribeAsync` + wire `.on(...)` events, all **after** `onContextReady()`.

## Group primitive (the room) — `liblogoschat.so` C ABI
Lives in the **peers fork** `~/projects/logos-libchat-mls-android` (pure-Rust libchat + C ABI),
**reusable host-side** — the x86_64 `liblogoschat.so` is **already built** at
`/extra/tmp/libchat-mls-build/target-host/release/liblogoschat.so` (needs `liblogosdelivery.so` +
`librln.so` from the installed `delivery_module`). Headless driver: `scripts/desktop-peer-mls/peer.c`
(`newgroup`/`addmember`/`groupsend`).
- `logoschat_create_group(name,desc)→convo_id` · `logoschat_add_group_member(convo_id,peer_hex_addr)`
  · `logoschat_group_members(convo_id)→[{account,device}]` (roster) · `logoschat_send_message(convo_id,bytes)`
  (1:1 **and** group) · events `CONVERSATION_STARTED` / `MESSAGE_RECEIVED{convoId,content,senderAccount}`
  / `MEMBERS_CHANGED{convoId}`.
- **GroupV1** (my #103 fix) = persists across node restart → use it (V2 has no reload).
- **Constraints:** add-by-**address**, **no join-by-code, no self-join** (MLS); add is async (commit
  round, ~seconds); message `content` is opaque bytes → define our own `{t:"chat"|"move",...}` envelope.
- **No presence primitive** — roster = members, not online. DIY liveness (ping on the raw topic).

## Architecture (reconciles groups with the user's code-join spec)
```
Lobby: [Start new game] / [Join created game]
CREATE  → logoschat_create_group("zk-guess: <name>", <desc>) → convo_id
        → derive an invite CODE; publish a StationCrypto code-topic (delivery) for the join handshake
        → creator shows the code; Start button enabled at ≥2 members
JOIN    → enter code → derive the code-topic → announce {my hex address} on it (delivery)
        → creator's module sees the announce → logoschat_add_group_member(convo_id, address)
        → MLS Welcome → joiner's CONVERSATION_STARTED → in the group
ROSTER  → logoschat_group_members(convo_id) on MEMBERS_CHANGED → the "joined users" sidebar
CHAT    → logoschat_send_message(convo_id, {t:"chat",...}) → MESSAGE_RECEIVED (reuse chat-ui MessageListModel)
TURNS   → raw delivery topic (derived from convo_id) for fast guess/verdict broadcast (no MLS-commit cost)
```
- **Code→owner-adds handshake** = the bridge between "enter a code" (spec #3–4) and MLS add-by-address.
- **Two transports:** MLS group = identity/membership/chat (authenticated, E2E); raw delivery topic =
  high-frequency turn state (the guess loop, already built in `zk-guess`).

## Module shape (mirror receiver + booth's core split)
`zk-guess-game` = **core** module (`type:core`) wrapping `liblogoschat.so` (create/add/roster/message via
`QProcess`/FFI, like booth's `radio_module` wraps delivery) + **ui_qml** (`interface:universal`) lobby/room
QML driving it via a `.rep` backend that also does the delivery code-topic + turn topic. Reuse
`StationCrypto` (code→topic), chat-ui `MessageListModel` (chat pane), announce/TTL only for liveness.

## Build plan (A, incremental — each a build+GUI-verify cycle)
1. **De-risk groups host-side:** two `peer.c` peers → `newgroup`/`addmember`/`groupsend` → confirm
   create→welcome→message→roster (the F1-style "prove known-good first").
2. **Core module** wrapping `liblogoschat.so` (create/add/roster/message + events).
3. **ui_qml lobby/room** (mirror receiver): lobby, create→code, join→code handshake, roster sidebar,
   creator Start gated ≥2.
4. Build `.lgx` (builder `6ef42ea8`), install into an isolated BC, GUI-verify (wetware).

## Honest scope
A is a **large native-integration build** (a core module linking the MLS group lib + a ui_qml
lobby/room + the delivery handshake + build/GUI cycles). Foundation is now fully de-risked (tooling,
group API, host lib already built, architecture locked) — the module build is the next phase.
