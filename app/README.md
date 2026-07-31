# app/ — sample app: verify a STARK proof on LEZ (epic #4)

A small demo on top of the verifier module (#3) that tells the zk story end to end and is
**cheap to run** (verify-side only, per `docs/research.md`).

## Demo flow

1. **Load a receipt** — a pre-proven `emit_credit` RISC0 receipt (proving happens off-node;
   see `experiments/baseline/`).
2. **Verify on LEZ** — call the module's `verifyReceipt(receipt, imageId)`.
3. **Show the result:**
   - a valid receipt → **✓ verified** + the decoded journal (the credit note / bound recipient);
   - a **redirected-witness** receipt → **✗ rejected** (the recipient bind is in-circuit, so a
     redirect is unprovable) — the money shot: *verification actually rejects a cheat.*

## Why this is the right demo

- Runs on modest hardware (verify is ms-scale / tiny RAM — the prove-side 9 GB stays off-node).
- Shows a real, non-trivial zk property (private recipient binding), not a toy.
- Mirrors the one-click node UX approach: one screen, clear ✓/✗, copyable image id + journal.

## Build order

Blocked on #3 (the module). Then: a thin QML/Basecamp UI (or a CLI first) that drives
`verifyReceipt` over the two fixtures and renders the verdict.
