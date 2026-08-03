# ADR-0005 — Distributed mouse-draw entropy → the sealed number

**Status:** accepted

## Context
If the host alone picks the number, "provably fair" is hollow — the host could choose a number they
expect no one to guess, or collude. The number must be one **no single participant controls**, while
still being a single sealed value the host can prove against (ADR-0004).

## Decision
On **Start**, every player **mouse-draws** in a canvas; the strokes are hashed into a per-player
contribution and broadcast. The host folds **all** contributions **plus its own committed seed** into
the number:
`secret = SHA256(host_seed ‖ sorted(contributions)) mod 1_000_001`, then seals `C` (ADR-0004).
Contributions are sorted so every host derives the same value regardless of arrival order.

## Consequences
- No single player picks the number; the host's own seed is committed, so it can't grind the fold
  after seeing others' contributions on the happy path.
- **Known refinement (not yet built):** a commit-reveal *proof* that the host couldn't bias the final
  fold. Today the host is trusted to fold honestly; the win is still client-verified against `C`, so
  the host can't lie about *the sealed number* — only, in principle, bias *which* number got sealed.
  Tracked as a follow-up (full anti-bias entropy).
