# module/ — LEZ STARK verifier (epic #3)

A Basecamp module (`mkLogos*`) that **verifies a RISC0 STARK receipt against a LEZ
program's image id**. Per the research (`docs/research.md`), verify is the cheap,
node-runnable side — this is the demo's core capability.

## Interface (planned)

```
verifyReceipt(receiptBytes, imageIdHex) -> { valid: bool, journal: bytes, error?: string }
```

- `receiptBytes` — a serialized `risc0_zkvm::Receipt` (produced off-node by the prover).
- `imageIdHex` — the program's image id (e.g. `referral_methods::REFERRAL_CREDIT_ID`).
- Verification: `receipt.verify(image_id)` (risc0-zkvm), matching what the LEZ sequencer
  enforces on-chain. Optionally cross-check against the live sequencer.

## Why a module (not just a CLI)

Basecamp modules are how this reaches operators: the sample app (#4) calls `verifyReceipt`
and shows the result. Keeps proving out entirely — the node only verifies.

## Plan

1. Thin Rust core wrapping `risc0_zkvm::Receipt::verify(image_id)` (+ decode the journal).
2. Wrap as a `mkLogosModule` / `mkLogosQmlModule` backend (linux-amd64 first; parity later
   via the `lgx merge` cross-machine flow).
3. Feed it fixtures from the referral baseline (a valid `emit_credit` receipt + a
   redirected-witness receipt that must be **rejected**).

## Reuse

- `~/basecamp/modules/zone-sequencer-rs` / `logos-zone-sequencer-module` — the LEZ verify
  path already packaged.
- `spel-fork-dev-repin/referral-methods` — the image id + a source of real receipts.
