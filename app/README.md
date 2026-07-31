# app/ — sample app: verify a STARK proof on LEZ (epic #4)

An interactive UI for the ZK **private-eligibility** verifier (module #3): prove a secret
value clears a public threshold, revealing only the claim.

- **`verifier.html`** — self-contained demo UI (theme-aware). Shows the flow: private
  witness ⟶ STARK ⟶ public journal ⟶ verdict, with the four guarantees (verify /
  zero-knowledge / integrity / soundness) and the real image id + measured costs.
  Live (private): https://claude.ai/code/artifact/ac28ec40-bac1-42e7-9bfd-130505b6bdfa
- It **mirrors the real RISC0 STARK proof** in `module/zk-eligibility/` (proven live, real
  mode) — the proof enforces exactly what the UI shows: eligible ⇔ value ≥ threshold,
  witness hidden, tampered/ineligible rejected.

## Next (productization)

Wrap the `verify` side as a Basecamp `mkLogosQmlModule` (off the `zone_sequencer` template):
C++/Rust backend exposing `verifyReceipt(receipt, imageId)` → this same UI in-app, driving
real receipts from `experiments/baseline/`. Verify is node-runnable (~ms); proving stays
off-node (~10 GB, per `experiments/results/feasibility.md`).
