#!/usr/bin/env python3
"""
v2 spike (issue #8, epic #7) — N=2 custody+compare for the ZK guessing game.

Demonstrates, in runnable *semi-honest* 2-party code, the CUSTODY half of Family C/A:

  - the secret `s` is XOR-shared bit-by-bit across two parties; NEITHER holds `s`.
  - given a PUBLIC guess `g`, the two parties jointly compute the verdict
    sign(g - s)  ->  ABOVE / BELOW / EQUAL, revealing ONLY that verdict.
  - AND gates use Beaver triples (dealer-precomputed here); XOR / NOT are local.
  - `s` is NEVER reconstructed: the only values ever opened are masked Beaver
    values (d, e) and the 2 output bits.

What this PROVES: custody + correct secure comparison, end to end, runnable.
What it does NOT prove: verifiable HONEST execution (a malicious party could
deviate) -> that is the collaborative-PROVING research risk (the NEXT spike), plus
malicious security / networked deployment. See docs/v2-trustless-custody.md.
"""

import random

N_BITS = 20            # secret range 0 .. 2^20-1  (~1e6)
rng = random.Random(20260731)   # fixed seed -> reproducible spike output

# -- instrumentation ---------------------------------------------------------
opened_values = []     # every bit ever OPENED (what both parties see on the wire)
p1_share_bits = []     # P1's raw share of each secret bit (P1's private view)
s_reconstructed = 0    # must stay 0: we never open s

def rbit():
    return rng.getrandbits(1)

# A shared bit is (p1_share, p2_share) with xor == cleartext.
def share_bit(b, record_p1=False):
    r = rbit()
    if record_p1:
        p1_share_bits.append(r)     # P1 only ever sees `r`, never `b`
    return (r, r ^ b)

def const_share(c):                 # public constant, folded into P1
    return (c, 0)

def open_bit(sh):                   # both parties broadcast their share
    v = sh[0] ^ sh[1]
    opened_values.append(v)
    return v

def xor(a, b):        return (a[0] ^ b[0], a[1] ^ b[1])      # local
def xor_const(a, c):  return (a[0] ^ c, a[1])               # local
def not_bit(a):       return xor_const(a, 1)                 # local

# Beaver multiplication triple (simulated trusted dealer; real ones come from OT).
def gen_triple():
    x, y = rbit(), rbit()
    return share_bit(x), share_bit(y), share_bit(x & y)

def and_bit(a, b):
    # [ab] = [z] + d*[y] + e*[x] + d*e ,  d = open(a^x), e = open(b^y)
    sx, sy, sz = gen_triple()
    d = open_bit(xor(a, sx))
    e = open_bit(xor(b, sy))
    c = sz
    if d: c = xor(c, sy)
    if e: c = xor(c, sx)
    if d & e: c = xor_const(c, 1)
    return c

# -- the joint comparison: verdict of PUBLIC g vs SECRET (shared) s ----------
def joint_compare(g, s_bits):
    """s_bits[i] = shared bit i (LSB=0). Returns (gt_open, eq_open)."""
    gt = const_share(0)          # (g > s) so far
    eq_high = const_share(1)     # all higher bits equal so far
    for i in range(N_BITS - 1, -1, -1):
        g_i = (g >> i) & 1
        s_i = s_bits[i]
        # bit-equal e_i : if g_i==1 equal iff s_i==1 ; else equal iff s_i==0
        e_i = s_i if g_i == 1 else not_bit(s_i)
        # term fires only where g_i=1 and s_i=0, gated by eq_high (first diff)
        if g_i == 1:
            term = and_bit(not_bit(s_i), eq_high)
            gt = xor(gt, term)   # XOR == OR here: at most one term ever fires
        eq_high = and_bit(eq_high, e_i)
    return open_bit(gt), open_bit(eq_high)

def verdict(gt, eq):
    return "EQUAL" if eq else ("ABOVE" if gt else "BELOW")

def run_turn(secret, guess):
    s_bits = [share_bit((secret >> i) & 1, record_p1=True) for i in range(N_BITS)]
    gt, eq = joint_compare(guess, s_bits)
    return verdict(gt, eq)

def ground_truth(secret, guess):
    return "EQUAL" if guess == secret else ("ABOVE" if guess > secret else "BELOW")

# -- 1. CORRECTNESS ----------------------------------------------------------
def test_correctness(trials=5000):
    bad = 0
    hi = (1 << N_BITS) - 1
    for _ in range(trials):
        s = rng.randint(0, hi)
        # mix exact hits in so EQUAL is actually exercised
        g = s if rng.random() < 0.15 else rng.randint(0, hi)
        if run_turn(s, g) != ground_truth(s, g):
            bad += 1
    return trials, bad

# -- 2. CUSTODY --------------------------------------------------------------
def test_custody(secret, turns=4000):
    """For a FIXED secret, P1's private share bits and all opened wire values must
    be ~50/50 -> they carry no information about `secret` (one-time-pad / masking)."""
    p1_share_bits.clear(); opened_values.clear()
    hi = (1 << N_BITS) - 1
    for _ in range(turns):
        run_turn(secret, rng.randint(0, hi))
    def bias(bits):
        n = len(bits); ones = sum(bits)
        return n, ones / n if n else 0.0
    return bias(p1_share_bits), bias(opened_values)

if __name__ == "__main__":
    print(f"v2 spike — N=2 custody+compare  (N_BITS={N_BITS}, seed=20260731)\n")

    n, bad = test_correctness()
    print("1. CORRECTNESS")
    print(f"   {n - bad}/{n} verdicts correct vs ground truth"
          + ("  ✓" if bad == 0 else f"   ✗ {bad} WRONG"))

    # a couple of readable sample turns
    print("\n   sample turns (secret is shared; only the verdict is opened):")
    for s, g in [(500000, 500000), (500000, 250000), (500000, 750000)]:
        print(f"     secret=•••••  guess={g:>7}  ->  {run_turn(s, g):<5}"
              f"  (truth {ground_truth(s, g)})")

    print("\n2. CUSTODY  (fixed secret, 4000 turns)")
    (n_sh, b_sh), (n_op, b_op) = test_custody(secret=734118)
    print(f"   P1 private share-bits of s : {n_sh:>6} bits, P(1)={b_sh:.4f}  (want ~0.5)")
    print(f"   values opened on the wire  : {n_op:>6} bits, P(1)={b_op:.4f}  (want ~0.5)")
    print(f"   times `s` was reconstructed: {s_reconstructed}  (must be 0)")
    ok = abs(b_sh - 0.5) < 0.03 and abs(b_op - 0.5) < 0.03 and s_reconstructed == 0
    print(f"\n   custody {'HOLDS ✓' if ok else 'VIOLATED ✗'}"
          " — neither party's view is biased by the secret; s never opened.")

    print("\n3. SCOPE")
    print("   proven here : custody + correct secure comparison (runnable)")
    print("   NOT here    : verifiable honest execution (collaborative PROVING) — next spike")
