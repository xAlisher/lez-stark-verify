# Research — STARK proof verification on LEZ (epic #1)

Grounded in the reproduced baseline (`experiments/baseline/`) and the referral
reference program. Working notes; updated as the matrix (#2) fills in.

## 1. What LEZ is (for this purpose)

The **Logos Execution Zone** is a shielded-state zkVM execution layer: programs run as
**RISC0 zkVM guests**, and state transitions are accepted only with a **valid STARK
receipt** verified against the program's **image id**. Precedents already on the zone:

- the **piñata** program (PoW faucet — `wallet pinata claim`);
- **private execution** — a guest reads `(npk, vpk)` from its own committed `data` and
  **binds the output-note recipient in-circuit**, so a redirect is *unprovable*
  (verification rejects it). This is the referral `emit_credit` program we baseline on.

So "STARK verification on LEZ" = **verify a RISC0 receipt against an image id**, on-chain
via the sequencer. That's the capability the sample app should showcase.

## 2. The cost split — prove is heavy, verify is cheap (this is the whole story)

Measured, real mode (`experiments/baseline/`):

| Side | Cost | Runs where |
|---|---|---|
| **Prove** (generate the STARK) | **116.6 s · 9.13 GB peak · 2²⁰ cycles · ~13.6 cores** | a capable box / service |
| **Verify** (check the receipt vs image id) | **cheap** — ms-scale, tiny RAM | **the node** / sequencer |
| Full private tx on the sequencer (PP-circuit composition ~8×) | ~16 min · ~9.6 GB | the sequencer |

**Consequence for productization:** the *node-runnable, demo-relevant* thing is **verify**
— it fits on a laptop/Pi. Proving is the heavy side and belongs off-node (a prover service,
or a one-shot on a capable machine). A sample app that **verifies a proof on LEZ** is both
the honest story and the cheap-to-run one.

## 3. RISC0 vs alternatives

| System | Fit for LEZ verify | Note |
|---|---|---|
| **RISC0 STARK (zkVM)** | **the proven path** — LEZ runs RISC0 guests; verify against image id | general Rust guest; recursion (`compress`) shrinks the receipt for cheaper on-chain verify |
| Plonky2/3 | research | fast recursive SNARK/STARK, but circuit-specific — not the zkVM model LEZ uses |
| Winterfell / Stone | research | STARK libraries; more manual, no zkVM guest ergonomics |

For the sample app, **stay on RISC0** (matches the zone). Alternatives are a cost-reduction
research thread, not the demo path.

## 4. Levers for "runs on modest hardware"

The prove side is the constraint (~9–10 GB). Levers under test (#2 matrix):

- **`RISC0_SEGMENT_PO2`** — smaller segments → lower peak RAM, more segments (slower). The
  matrix tests whether a smaller segment size lets the *proof* fit under a 4–6 GB cap.
- **Prove off-node entirely** — verify is cheap, so the node never proves; the app verifies
  a proof produced elsewhere. (Default recommendation.)
- **Receipt compression** (`compress`/recursion) — smaller receipt → cheaper on-chain verify.

## 5. Recommended demo target

**A Basecamp module + sample app that verifies a RISC0 STARK proof on LEZ** — submit a
proof/receipt → verify against the image id (on the node / via the sequencer) → show the
result (valid ✓ / a redirected witness rejected ✗). Proving is a separate, pre-run step
(the heavy side). This is node-runnable, demo-clean, and tells the real zk story.

→ feeds epics #3 (verifier module) and #4 (sample app).
