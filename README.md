# lez-stark-verify

A sample app + Basecamp module demonstrating **STARK proof verification on the Logos
Execution Zone (LEZ)** — and mapping where it's actually usable on modest hardware.

> **Private R&D repo** (EcoDev). Goal set by Franck: investigate zk-proof verification
> on LEZ (ideally STARK), stage resource-capped experiments on Sneg, and build a module
> + sample app on top.

## Not a cold start

The hard part is already proven (referral R&D, 2026-07):

- RISC0 guest ELF → **real STARK proof** (RISC0 STARK, ~116.6 s to prove)
- proof **verified on-chain** via a **standalone LEZ sequencer on Sneg**
- full privacy-preserving tx **settled in real verification mode** — measured
  **~9.6 GB peak RAM / ~16 min per tx** (unbounded Sneg run)
- a redirected recipient was proven **unprovable** (circuit-bound), i.e. verification
  actually rejects invalid witnesses.

So this repo is mostly **productizing** that path + finding its **feasibility envelope**
under RAM/CPU caps, then wrapping it as an installable module and a demo app.

## Plan (epics)

1. **Domain research & feasibility map** — what's meaningfully provable+verifiable on
   LEZ; RISC0 vs alternatives; the prove-side vs verify-side cost split (which side the
   node runs).
2. **Capped experiments on Sneg** — reproduce the proven path under cgroup RAM/CPU caps;
   a matrix (guest complexity × RAM cap × cores) → peak RAM / wall time / success-or-OOM;
   find the laptop/Pi-class envelope. Baseline to cap down from: ~9.6 GB / ~16 min.
3. **Verifier module** — a Basecamp module that verifies a STARK proof against LEZ.
4. **Sample app + docs** — a small UI on top (submit → prove → verify → show receipt) +
   a handout with the real cost numbers.

## Repo layout (planned)

```
docs/            research notes, feasibility matrix, measurements
experiments/     capped runs on Sneg (scripts + results, cgroup-limited)
module/          the LEZ STARK-verify Basecamp module
app/             the sample app / demo UI
```

## Environment

- **Sneg** (the farm build box) runs the capped experiments; RISC0 toolchain lives on
  `/extra` (root partition chokes on big proof builds).
- Caps via `systemd-run --scope -p MemoryMax=… -p CPUQuota=…` (or a cgroup) so each run
  has a hard RAM/CPU ceiling and we record peak + time honestly.
