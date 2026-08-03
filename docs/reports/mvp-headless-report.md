# MVP headless test report — M1 honesty core + M2 game loop

_Real STARK, `RISC0_DEV_MODE` unset. Built + run on `wild` (RISC0 3.0.5/3.0.6, CPU prover),
target + toolchain on `/extra`. Epic #10 · M1 #11 · M2 #13. Code: `module/zk-guess/`._

## What this proves
The v1 mechanic works end to end on a **real** STARK: a sealed number, per-turn proofs of
*above/below* that **verify**, the **secret never revealed** in the journal, a swapped number
**unprovable**, and a converging game that ends in a proven **exact** hit — all headless, no UI.

---

## M1 — honesty core (the 4 ways)  ✅ PASS

`cargo run --release` (guest = `guess`, host harness). Compile 1m32s, then:

```
sealed.  commitment = 9322c0b129bbe81cd2a088bc28a3010e551b027bd12ddf749a7b05c20a3c059c
check 1 ✓ verified · journal = (commit, 600000, ABOVE)
check 2 ✓ secret absent from journal (140 bytes)
check 3 ✓ tampered receipt rejected
check 4 ✓ swapped secret unprovable
ALL 4 CHECKS PASSED — real STARK, secret never revealed.
```

| Check | Meaning | Result |
|---|---|---|
| 1 | a valid turn verifies; journal = `(commitment, guess, dir)` | ✅ `dir=ABOVE` for guess 600000 |
| 2 | the secret is **absent** from the 140-byte journal | ✅ |
| 3 | a tampered receipt is rejected | ✅ |
| 4 | a **swapped** secret can't be proved (guest halts on commitment-open) | ✅ |

**Note on check 4:** the run prints a `commitment mismatch: sealed number was swapped` panic on
stderr — that **is** check 4 working: the guest asserts `SHA256(secret‖blind)==commitment`, so
proving a different secret against the original commitment halts the guest and yields no receipt.
The harness catches the `Err` and reports the pass.

**Design confirmed:** honesty (can't lie about above/below) **and** immutability (can't swap the
number) both come from one tiny guest; the secret is a private witness that never enters the
journal. Digest parity held first try (sha2 in both guest and host).

---

## M2 — headless game loop  ✅ PASS

`cargo run --release --bin play` — seal `s`, binary-search guesses, prove+verify every turn,
narrow the range, win on `EQUAL`, then reveal `s` and re-check it against the seal. Recompile
37s, then 20 real proofs:

```
── zk-guess · headless game ──
sealed a number in 0..=1000000
commitment = 9322c0b129bbe81cd2a088bc28a3010e551b027bd12ddf749a7b05c20a3c059c

turn  1 · guess  500000 · BELOW ✓verified  7711ms · range 0..=1000000
turn  2 · guess  750000 · ABOVE ✓verified  7345ms · range 500001..=1000000
turn  3 · guess  625000 · ABOVE ✓verified  7563ms · range 500001..=749999
 …
turn 13 · guess  573119 · ABOVE ✓verified  9242ms · range 572998..=573240
turn 19 · guess  573117 · BELOW ✓verified  7518ms · range 573116..=573118
turn 20 · guess  573118 · EQUAL ✓verified  7956ms · range 573118..=573118
★ EXACT after 20 turns in 158.3s
reveal: secret = 573118
✓ revealed secret matches the seal — game was honest end to end.
```

- **Correctness:** the binary search converged 0..=1,000,000 → 573,118 in 20 turns; every
  direction (`BELOW`/`ABOVE`/`EQUAL`) was the truth, and **each was a verified STARK receipt**
  before the range moved.
- **Win path:** `EQUAL` triggered the reveal, and the revealed `573118` **re-hashed to the same
  commitment** posted at seal time — the game is provably honest start to finish.
- **Timing:** ~7.3–9.2s per turn (real CPU proof of the tiny compare guest), 158.3s total.
  Fast enough that the per-turn wait is exactly the "bluff window" the chat UI is designed around.

**M1 + M2 together = the whole v1 mechanic, proven headless.** What remains is wiring (M3 chat UI
to this backend) and settlement (M4 pot on LEZ) — no open cryptographic risk in the core.

---

## Space discipline
Root `/` held at **14G free / 82%** across both builds; all growth (build target, risc0
toolchain, proof segments) landed on `/extra` (163G free), crate cache on `/data`. Nothing
space-heavy touched root.
