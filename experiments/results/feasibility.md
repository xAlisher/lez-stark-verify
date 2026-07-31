# Feasibility matrix — real STARK proof under RAM/CPU caps (epic #2)

Program: the referral `emit_credit` STARK proof (2²⁰ cycles). Harness: `cap-run.sh`
(cgroup v2, `MemoryMax` + `MemorySwapMax=0` + `CPUQuota`). Host: wild (x86_64). Baseline
unbounded: **116.6 s / 9.13 GB / ~13.6 cores**.

| Cap | Cores | Outcome | Wall | Peak |
|---|---|---|---|---|
| 12 GB | ~14 | **OK** | 2:24 | 9.13 GB |
| 10 GB | ~14 | **OK** | 2:10 | 9.13 GB |
| 8 GB | ~14 | **OOM** | killed 32 s | — |
| 6 GB | ~14 | **OOM** | killed 11 s | — |
| 4 GB | ~14 | **OOM** | killed 11 s | — |
| 12 GB | 4 | OK | 5:15 | 9.13 GB |
| 12 GB | 2 | OK | 9:59 | 9.13 GB |
| 4 GB, `SEGMENT_PO2=18` | ~14 | **OOM** | killed 10 s | — |
| 6 GB, `SEGMENT_PO2=19` | ~14 | **OOM** | killed 10 s | — |

## Findings

- **RAM floor ≈ 10 GB.** The proof completes at 10 / 12 GB (peak 9.13 GB) and **OOMs at
  ≤ 8 GB**. Proving is *not* laptop/Pi-class.
- **Cores scale ~inverse-linearly.** 14c → 2:24, 4c → 5:15, 2c → 10:00. Fewer cores still
  complete (given the RAM), just proportionally slower.
- **Segment-size tuning did NOT lower the RAM floor.** `RISC0_SEGMENT_PO2` 18/19 at 4–6 GB
  still OOM'd — per-segment sizing (as tried) doesn't fit *prove* into modest RAM. (Worth a
  deeper look: composite-proof residency may dominate; not a quick win.)

## Conclusion (feeds #1, #3, #4)

**Prove ≈ 10 GB box, minutes; verify is cheap (ms, tiny RAM).** So the productization stands:
the node/app **verifies** (runs anywhere), and **proving stays off-node** — a prover service
or a one-shot on a ≥10 GB machine. The minimum-viable spec for a *proving* node is ~10 GB RAM
+ several cores; a *verifying* node has no such constraint.

_Reproduce: `experiments/run-matrix.sh` (wild). Portable to Sneg — needs private-repo creds there._
