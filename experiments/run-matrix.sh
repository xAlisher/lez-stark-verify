#!/usr/bin/env bash
# run-matrix.sh — the real STARK proof under a RAM/CPU cap matrix (epic #2).
# Baseline (unbounded): 116.6 s / 9.13 GB peak / 2^20 cycles / ~13.6 cores.
set -uo pipefail
cd "$(dirname "$0")"                     # experiments/  (cap-run writes results/ here)
H=./cap-run.sh
PROVE='cd ~/basecamp/forks/spel-fork-dev-repin/referral-methods && \
  export PATH="$HOME/.risc0/bin:$HOME/.cargo/bin:$PATH" && \
  export CARGO_TARGET_DIR=/extra/tmp/referral-risc0/target-lez && \
  unset RISC0_DEV_MODE && \
  exec cargo test --test prove --release -- --ignored --nocapture'

run(){ "$H" "$1" "$2" "$3" -- bash -lc "$PROVE"; }
runseg(){ "$H" "$1" "$2" "$3" -- bash -lc "export RISC0_SEGMENT_PO2=$4; $PROVE"; }

printf "label,mem,cpu,outcome,wall,peak_gb\n" > results/matrix.csv

echo "## memory sweep (full CPU ~14 cores) — find the OOM threshold"
run mem-12g 12G 1400%
run mem-10g 10G 1400%
run mem-8g   8G 1400%
run mem-6g   6G 1400%
run mem-4g   4G 1400%

echo "## core sweep (12G headroom) — time vs cores"
run cpu-4c  12G 400%
run cpu-2c  12G 200%

echo "## segment-size lever at 4G (does smaller RISC0_SEGMENT_PO2 fit under the cap?)"
runseg seg18-4g 4G 1400% 18
runseg seg19-6g 6G 1400% 19

echo "## DONE"
column -s, -t results/matrix.csv
