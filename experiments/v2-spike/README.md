# v2 spike — N=2 custody+compare

Issue #8 · epic #7 · design: [`docs/v2-trustless-custody.md`](../../docs/v2-trustless-custody.md)

The smallest real test of the **custody+compare** half of Family C/A: two parties, each
holding a *share* of the secret `s`, jointly compute the per-turn verdict `sign(g − s)`
(ABOVE / BELOW / EQUAL) revealing **only the verdict** — with neither party ever holding `s`.

## Run

```bash
python3 two_party_compare.py      # no deps, ~1s
```

## What it does

- `s` is **XOR-shared bit-by-bit** across P1/P2 → neither holds it.
- Public guess `g`; a boolean comparator circuit: XOR/NOT are **local**, AND gates use
  **Beaver triples** (dealer-precomputed here; real ones come from OT).
- The **only** values ever opened are the masked Beaver values `(d,e)` and the 2 output
  bits. `s` is never reconstructed.

## Result (seed 20260731)

```
1. CORRECTNESS   5000/5000 verdicts correct vs ground truth  ✓
2. CUSTODY (fixed secret, 4000 turns)
   P1 private share-bits of s :  80000 bits, P(1)=0.4983   (~0.5)
   values opened on the wire  : 247746 bits, P(1)=0.4886   (~0.5)
   times s was reconstructed  : 0
   custody HOLDS ✓ — neither party's view is biased by the secret; s never opened.
```

P1's private shares **and** everything on the wire are statistically independent of the
secret (one-time-pad shares + Beaver masking) → the transcript carries no information about
`s`. The custody + secure-comparison mechanic is de-risked **in runnable code**.

## Scope — what this does and does NOT settle

| | |
|---|---|
| ✅ proven here | custody (nobody holds `s`) + correct secure comparison, end to end |
| ❌ not here | **verifiable honest execution** — a *malicious* party could deviate and the others couldn't catch it |
| ❌ not here | malicious security, networked deployment, guess `g` also private |

The `❌ verifiable honest execution` gap **is** the real go/no-go for Family C: it's the
**collaborative-PROVING** research risk — jointly producing a *proof* (not just an answer)
over the shared witness, so a cheating party is caught. That needs an MPC-friendly proving
stack and is the next spike; the maturity lit-check informs C-vs-B for it.

This spike shows the *custody+compare* foundation is sound and cheap; the open bet is
whether you can bolt *verifiable honesty* onto it collaboratively (C) or fall back to
threshold-FHE with its own proof of the homomorphic step (B).
