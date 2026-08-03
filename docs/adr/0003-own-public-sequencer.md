# ADR-0003 — Run our own version-matched public sequencer

**Status:** accepted

## Context — how we use a sequencer
On-zone win settlement (ADR-0001) submits a **privacy-preserving transaction** to a LEZ
**`sequencer_service`** (jsonrpsee RPC). The tx carries our `zk-guess` program **inline**
(`ProgramWithDependencies`) — there is no separate `deploy-program` step. The wallet proves the tx
locally (real STARK), submits it, and the **sequencer verifies** the receipt against
`PRIVACY_PRESERVING_CIRCUIT_ID` and includes it in a block. Inclusion = on-zone verification.

Our program + wallet are built against **LEZ rev `787a15aad34acb89f750eadc4c41bfdf5c0d59a8`**. The
program's `PRIVACY_PRESERVING_CIRCUIT_ID`, the wallet's RPC method set, and the sequencer's verifier
**must all come from the same LEZ revision** — this is a tightly-coupled circuit + protocol contract,
not a loose API.

## Why the official public testnet doesn't work
`https://testnet.lez.logos.co` is a **live, hosted** LEZ testnet sequencer (responding, ~block 48.5k).
But a real-mode submit from our rev-`787a15aa` wallet returns **`SequencerError: MethodNotFound`**.

The testnet runs a **different LEZ revision** than ours. Two ways this bites:
1. **RPC surface drift** — the method our wallet calls doesn't exist (or was renamed) on the testnet's
   version → `MethodNotFound`, before verification is even attempted.
2. Even past the RPC, a **different `PRIVACY_PRESERVING_CIRCUIT_ID`** would reject our receipt — the
   verifier and the prover must share the exact circuit.

There is **no published mapping** from `testnet.lez.logos.co` to a source rev, so a builder can't
match it: we can't rebuild our program+wallet against "the testnet's rev" because we don't know it,
and it can move under us. (See ADR-0007's issue draft — the upstream ask is to pin/publish it.)

## Decision
Run **our own** standalone LEZ sequencer, **version-matched to rev `787a15aa`**, at
**`https://sequencer.logos.live`**:
- Built from `logos-blockchain/logos-execution-zone` @ `787a15aa`, `--features standalone`
  (mocks the Bedrock L1), `RISC0_DEV_MODE` **unset** → real verification, `r0vm` on PATH.
- Hosted on the existing Hetzner VPS (`116.202.19.154`, the box that serves `msg.logos.live`), in the
  `logos-storage` docker compose. Caddy fronts it with TLS + Basic auth (ADR-0007); 3040 is
  docker-net only. Cost is light — the sequencer only *verifies* (ADR-0001).

**Proven end-to-end, real mode, over the public endpoint:** `INIT` → block 110,
`GUESS(600000)=ABOVE` → block 181 *verified on LEZ*, wrong-secret **rejected at proving**.

## Consequences
- We control the version, so our program always matches — no `MethodNotFound`, no circuit mismatch.
- We own the operational surface (uptime, TLS, auth, funding via genesis). Details in
  `../sequencer-deploy-plan.md`.
- We stay **rev-pinned** to `787a15aa`; adopting a newer LEZ means rebuilding program+wallet+sequencer
  together, in lockstep.
- The *ideal* end state is settling on an official, long-lived public LEZ whose rev is published so
  builders can match it — tracked upstream (ADR-0003's companion issue).
