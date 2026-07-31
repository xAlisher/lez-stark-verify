# v2 — the trustless path: nobody holds the secret

_Exploration doc for the ZK guessing game. v1 = a host holds the secret number and
proves each above/below honestly (the zkMastermind model — proven, shippable). v2 asks
the harder question: **can the game run with no house at all — no party ever holding the
secret in the clear?** This is a research track. Working notes._

Related: [research.md](research.md) (prove-heavy/verify-cheap), the `zk-eligibility`
comparison guest (the honesty core we already proved 4 ways).

---

## 0. The reframing that governs everything

**A STARK does not give you v2 by itself.** A zkVM proves *a computation ran correctly on
a witness* — but whoever runs the prover **holds that witness in plaintext**. So the naive
"feed the secret `s` as a private input to the guest" just moves the trust onto whoever
runs the prover. They know `s`. They can play, or leak to a friend.

So the STARK buys exactly one of the two things v2 needs:

| Property | What it means | Given by |
|---|---|---|
| **Honesty** | the "above/below" can't be a lie relative to the committed `s` | ✅ the STARK / zkVM |
| **Custody** | *no single party ever holds `s` in the clear* | ❌ needs a separate layer |

**v2 = a custody layer + the honesty layer.** Everything below is about the custody layer,
and how to fuse it with the STARK so the world still gets a cheap public verify.

The randomness question ("mouse-draw?") is *solved* and orthogonal: commit-reveal over each
player's entropy contribution (mouse scribble → hash) gives a fair, uncontrolled seed. The
open problem is not *generating* `s` — it's *holding* it after generation.

---

## 1. What v2 must do, precisely

1. **Sealed setup.** Commit-reveal mouse-draws produce `s`, which lands **only as a
   distributed secret** (shares / ciphertext) across a committee, plus a public commitment
   `C = H(s ‖ r)`. No machine ever has `s` in the clear.
2. **Per-turn oracle.** Given a guess `g`, the system emits one bit `b = sign(g − s)`
   (above / below / equal) **with a proof `b` is consistent with `C`** — and `s` stays sealed.
3. **Win + reveal.** A guess with `b = equal` triggers: reveal `s`, prove `H(s‖r) = C`, pay
   the pot. On-chain, atomic.
4. **No single point of custody.** Any minority of the committee colluding must **not**
   recover `s`. (Threshold trust, not single-host trust.)
5. **Anti-grief.** If the committee stalls, `s` must eventually settle so the pot isn't
   locked forever.

The hard requirements are **2** and **4** together: compute a comparison against a secret
*nobody holds*, and prove it honest.

---

## 2. The four families (custody layer)

### A — Secret-sharing + MPC comparison
`s` is Shamir-shared across the committee (generated *inside* MPC from the mouse-draws, so
it's never assembled). Each turn the committee runs a **secure comparison** protocol on `[s]`
vs public `g` → one output bit; `s` stays shared. Secure comparison is a well-studied MPC
primitive (bit-decomposition / DGK-style).
- **Trust:** honest-threshold committee. No single party.
- **Honesty:** needs pairing with a proof the committee ran the protocol (verifiable MPC /
  MPC-in-the-head), else the committee could lie.
- **Cost:** interactive between members *every turn*. Fine here — turns are human-slow.
- **Maturity:** MPC comparison is mature; the *verifiable* variant is the fiddly part.

### B — Threshold FHE
`s` is encrypted under a **threshold public key** (decryption key split across the committee);
`Enc(s)` is public/on-chain. Each turn, **anyone** homomorphically computes `Enc(sign(g−s))`
from the public ciphertext and public `g`; the committee then **threshold-decrypts just the
1-bit result** — never `s`.
- **Trust:** threshold committee for the key. `s` is never decrypted.
- **Nice:** the heavy homomorphic compute is public/non-interactive; only a 1-bit decryption
  needs the committee (much less interaction than full MPC).
- **Cost:** FHE comparison is heavy (TFHE does comparisons in ~tens of ms–seconds); one per
  turn is tolerable.
- **Maturity:** threshold-decryption-to-a-committee has real blockchain precedent (Anoma's
  **Ferveo** threshold-encrypts the mempool to the validator set — same custody shape).
  Verifiable FHE (proving the homomorphic step) is the rough edge.

### C — Collaborative proving over a shared witness  ⭐ recommended
The elegant fusion: the committee **jointly produces the STARK/SNARK** while `s` is
**secret-shared** among them, so *no prover learns `s`*, and the output is a normal proof the
whole world verifies in ms. One primitive delivers custody **and** honesty **and** cheap
public verify.
- Setup: MPC over the mouse-draws lands `[s]` as shares + public `C`.
- Turn: committee runs **collaborative proving** on the same guest we already have — check
  `[s]` opens to `C`, compute `b = sign(g − s)`, commit `(C, g, b)`. No member learns `s`;
  anyone verifies the receipt.
- **Trust:** honest-threshold committee = a natural fit for the **LEZ sequencer set**.
- **Prior art:** *collaborative zk-SNARKs* (Ozdemir & Boneh, USENIX '22) and follow-ups
  (zkSaaS, scalable-coZK). Proving with a secret-shared witness.
- **Maturity (from the lit-check, § below) — the critical finding:** collaborative
  **SNARK** proving is **production-real** — TACEO's `co-snarks` runs in **World ID**
  (~18M users, iris codes secret-shared + jointly proven) and Renegade's dark pool. Overhead
  is **~1× solo on a fast LAN** (malicious-minority), ~2× for full malicious security. **BUT
  it is all pairing/Plonk-family (Groth16/Plonk/Marlin/HyperPlonk). Collaborative
  STARK/FRI proving DOES NOT EXIST** — FRI's hashing is non-arithmetic and brutal inside
  MPC, so no one has built a collaborative RISC0/zkVM prover, prototype or otherwise.
- **Consequence:** Family C is viable **only if the oracle's proof is a Plonk-family SNARK**
  (not RISC0/STARK). Two sub-options: **(C1)** the per-turn oracle uses collaborative
  *SNARK* proving (TACEO stack) while RISC0/STARK stays for solo-proved, node-verified parts;
  **(C2)** SNARK-wrap the RISC0 STARK and prove the wrapper collaboratively — *itself
  unproven*. **Betting C on collaborative STARK is a no-go.**

### D — TEE / enclave (pragmatic bridge, off-thesis)
`s` lives inside an SGX/TDX/Nitro enclave that generates it, answers each turn, and attests it
ran the right code.
- **Trust:** the hardware vendor + enclave integrity (side-channel history is poor).
- **Verdict:** fastest to build, but it's "trust Intel/AWS," not "trust math" — philosophically
  **off-brand for Logos**, whose whole point is not trusting a party or a chip. Keep as a
  v1.5 bridge only if a live demo is needed before the crypto lands. Not the destination.

### (rejected) E — VDF / time-lock
Time-lock encrypting `s` gives *eventual reveal*, not *repeated private queries*, so it can't
serve per-turn above/below. **But** it's the right tool for requirement 5 (anti-grief): publish
a time-lock / VDF encryption of `s` at setup so if the committee vanishes, `s` auto-opens and
the pot settles. Belt-and-suspenders, not the oracle.

---

## 3. Recommended architecture

> **STARK is a hard requirement of the deliverable** (Franck's ask: "sample app to show zk
> proof verification on LEZ, ideally STARK"). That constraint drives the whole recommendation
> and — helpfully — *resolves* the C-vs-B fork. STARK lives on three surfaces; only one is
> touched by the lit-check:
>
> | Surface | STARK? | Touched by "no collaborative STARK"? |
> |---|---|---|
> | LEZ **verification** (receipt vs image id) | ✓ | no — this is the demo |
> | Per-turn **proving, v1** (single host → RISC0 receipt) | ✓ | no — solo proving works today |
> | Per-turn proving **only if the witness is secret-shared** | — | yes — the one corner that doesn't exist |

**The sample app is v1 — pure STARK, end to end** (host holds `s`, generates a RISC0 STARK
each turn, the LEZ node verifies it). Already built + proven 4×. This is the deliverable; the
collaborative-proving finding does **not** touch it.

**v2 "no house" — the STARK-preserving path is Family B, not C1:**

> **Threshold-FHE custody + solo STARK proof + threshold 1-bit decrypt.**
> `s` lives as `Enc(s)` under a committee key. Each turn, *any* prover homomorphically
> computes `Enc(sign(g−s))` **on public ciphertext** (so its witness isn't secret) and
> **STARK-proves the FHE compare** (`open-check` of `C`, the homomorphic eval, commit `(C,g,·)`).
> The committee **threshold-decrypts just the 1 output bit**. Add the VDF escrow (E) for
> anti-grief.
>
> Because the witness is public ciphertext, there is **no shared witness → no collaborative
> proving → solo STARK is fine.** The only multi-party step is a 1-bit threshold decrypt (a
> signature-like op, *not* a proof). **STARK preserved.**

**Why B over C1 here:** C1 (collaborative SNARK over a secret-shared witness) is production-real
but **trades the STARK away** — it swaps the proof system to Plonk/Groth16. Given the STARK
requirement, C1 is deprioritized. B keeps solo STARK proving; its cost is real (proving an FHE
evaluation inside a STARK is heavy) but it is *standard-shaped solo proving that exists*, not
the collaborative kind that doesn't. Committee = LEZ sequencer set for the threshold key.

```
Setup    players scribble → commit-reveal → MPC folds contributions into [s] (shares)
         + public commitment C = H(s‖r).           Also: publish VDF-encrypt(s) as escrow.
Turn     player submits g  (public, or committed for privacy-from-opponents)
         → committee runs COLLABORATIVE PROVING of the guest:
              assert open([s]) ⊕ C  consistent
              b = sign(g − s)
              commit (C, g, b)
         → one STARK receipt; nobody learned s; anyone verifies in ms on the sequencer.
Win      b = equal → guest reveals s, proves H(s‖r)=C → pot pays out, on-chain, atomic.
Grief    committee stalls past T → VDF escrow opens s → anyone settles the pot.
```

**Why this and not the others:** it collapses custody + honesty + public-verify into one
primitive and maps the committee onto infrastructure LEZ *already has* (a sequencer set).
B is the fallback if collaborative STARK proving proves impractical (its heavy compute is
public, only a 1-bit decrypt is threshold). A is the fallback-of-the-fallback (mature crypto,
but interactive every turn and needs a bolt-on honesty proof). C is strictly the most
elegant and the most uncertain — hence: **spike it before committing.**

**"Private from opponents" upgrade:** if `g` is also committed/shared rather than public,
the comparison is secret-vs-secret — still fine for A/B/C — and the collaborative proof
commits only `b`. This is the genuinely novel bit (§ the prior-art note: zkMastermind guesses
are public; a staked race with guesses private *between players* is the differentiator).

---

## 4. The honest risk ledger

| Risk | Severity | Note |
|---|---|---|
| ~~Collaborative STARK proving may not compose with RISC0~~ → **CONFIRMED: collaborative STARK doesn't exist** | **resolved → steer** | lit-check: coSNARK proving is production-real but **SNARK-only**; FRI is MPC-hostile. **But the sample app never needed it** — it's solo-proved STARK (v1). Only the secret-*shared*-witness flavor of v2 hits this wall, and we avoid that flavor: **Family B keeps solo STARK proving** (compute on public ciphertext). C1 would trade STARK away → deprioritized. |
| Committee liveness (must be online each turn) | med | turns are human-slow; VDF escrow covers total stall |
| Committee threshold-collusion recovers `s` | med | standard threshold assumption; size/stake the set accordingly |
| Verifiable-MPC / verifiable-FHE overhead (families A/B) | med | the honesty bolt-on is the cost driver, not the comparison |
| "No formal LEZ MPC/threshold primitive today" | **high** | this is the real gap — v2 is a *capability* LEZ would gain, not a config of what exists |

**Bottom line on maturity:** v1 rests on shipped tech. v2 rests on **research-grade
primitives** (collaborative proving / threshold FHE) that Logos does **not** have off the
shelf. That's not a reason to skip it — it's exactly the kind of capability a sample-app R&D
track exists to de-risk — but it must be named as research, not plumbing.

---

## 5. The minimal spike (trivial-experiment-first)

Before any committee/sequencer integration, answer the one question that kills or greenlights
Family C:

> **Can two parties, each holding a share of `s`, jointly produce a single proof of
> `sign(g − s)` without either learning `s`, that a third party verifies in ms?**

- **Toy it at N=2** with an MPC-friendly proving stack (not RISC0 first — pick one with a
  collaborative-proving implementation), a 32-bit `s`, public `g`, output one bit + `C`-check.
- Measure: proving wall-clock, inter-party communication, verify cost.
- **Kill criterion:** if 2-party collaborative proving of a comparison is > ~minutes or needs
  bespoke unfinished tooling → fall back to **Family B** (public FHE compare + 1-bit
  threshold decrypt), whose parts are more available.
- **Parallel cheap win:** implement the **VDF anti-grief escrow** independently (it's useful
  under *every* family and needs no committee).

Deliverable of the spike: a feasibility note appended here + a go/no-go on C-vs-B for v2.

### Spike result — custody+compare (issue #8, `experiments/v2-spike/`)

**Ran the custody+compare half first** (the part I'd called mature) to nail the foundation
before betting on collaborative proving. Runnable Python, semi-honest 2PC, 20-bit `s`:

- **Correctness:** 5000/5000 verdicts (ABOVE/BELOW/EQUAL) match ground truth. ✓
- **Custody:** for a fixed secret over 4000 turns, P1's private share-bits are `P(1)=0.498`
  and every value opened on the wire is `P(1)=0.489` — statistically independent of `s`
  (one-time-pad shares + Beaver masking). `s` reconstructed **0** times. ✓

So the **custody + secure-comparison mechanic is de-risked in runnable code**: two parties
compute `sign(g−s)` with neither ever holding `s`.

**What the spike deliberately does NOT cover — and why it's the real crux:** it gives an
*answer*, not a *proof of honest execution*. A **malicious** party could deviate and the
others couldn't catch it. Closing that gap = **collaborative proving** (Family C) or
threshold-FHE-with-a-proof (Family B). That is still the go/no-go, now sharpened:

> the open bet is not "can shares compare?" (yes) — it's "can you bolt **verifiable
> honesty** onto the shared comparison *collaboratively*, cheaply enough?"

Next: the maturity lit-check on collaborative STARK/zkVM proving → decides C vs B.

### Lit-check result — collaborative proving maturity (the deciding input)

**Verdict: collaborative *SNARK* proving is production-real; collaborative *STARK* proving
does not exist.**

- **Foundation:** Ozdemir & Boneh, *Experimenting with Collaborative zk-SNARKs*, USENIX
  Security '22 ([usenix](https://www.usenix.org/conference/usenixsecurity22/presentation/ozdemir),
  [eprint 2021/1530](https://eprint.iacr.org/2021/1530)) — lifts a solo SNARK prover to N
  provers over a **secret-shared witness**; verifier checks the *same* single proof. Covered
  Groth16 / Marlin / Plonk / Fractal. **Overhead ~1× solo on a 3 Gbit/s LAN** (malicious
  minority), **~2×** for full malicious security; degrades on WAN.
- **Production:** TACEO `co-snarks` ([github](https://github.com/TaceoLabs/co-snarks)) — in
  **World ID** (~18M users; iris codes secret-shared + jointly proven,
  [TACEO](https://core.taceo.io/articles/taceo-proof-prod/)); **Renegade** dark pool on
  Starknet ([docs](https://docs.renegade.fi/core-concepts/mpc-zkp)). Both **SNARK** (Plonk/
  Groth16 family). Library self-labeled experimental/un-audited.
- **STARK is the gap:** every construction is pairing/sumcheck-SNARK. **No collaborative
  RISC0/zkVM/FRI prover exists** — FRI's Merkle/hashing steps are non-arithmetic, so the
  "MPC cheaply emulates the algebraic prover" trick doesn't carry to STARKs. Nearest FRI-
  adjacent work — *How to Prove Statements Obliviously*, CRYPTO '24
  ([Springer](https://link.springer.com/chapter/10.1007/978-3-031-68403-6_14)) — is
  *single-server blind* proving, a different trust model closer to the FHE fallback.
- **Scalability frontier (if we pursue C1):** scalable/sublinear coSNARKs
  ([eprint 2024/143](https://eprint.iacr.org/2024/143),
  [2025/1388](https://eprint.iacr.org/2025/1388.pdf)); a caution that some earlier
  malicious-security claims were subtler than stated
  ([eprint 2025/1026](https://eprint.iacr.org/2025/1026)) — read the security proofs before
  relying on them.

**Go/no-go (given the STARK requirement):** the **sample app stays STARK** — it's v1 (solo
RISC0 proving + LEZ verify), untouched by this finding. For v2 "no house", **choose B
(threshold-FHE): it preserves solo STARK proving** because the homomorphic compute runs on
*public* ciphertext (no shared witness). **C1** (collaborative SNARK) is production-proven but
**trades STARK away**, so it's deprioritized under this constraint. **No-go** on collaborative
STARK (nonexistent). Either way the custody+compare foundation (§ spike) is settled.

---

## 6. What ships regardless

v1 (host + honest per-turn STARK, mouse-draw entropy, commit-reveal so nobody suspects a
hand-picked number) is the demo that *proves the mechanic* on real Logos tech today. v2 is
the "no house at all" upgrade that turns the demo into a statement about what LEZ can be.
Ship v1; spike v2's Family-C question in parallel; let the spike decide v2's shape.
