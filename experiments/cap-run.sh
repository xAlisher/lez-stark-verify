#!/usr/bin/env bash
# cap-run.sh — run a command under a HARD RAM/CPU cap; record wall + peak RSS + OOM.
#
#   ./cap-run.sh <label> <MemoryMax e.g. 4G> <CPUQuota e.g. 200%> -- <cmd...>
#
# Uses a transient systemd user scope (cgroup v2). MemorySwapMax=0 so hitting the
# cap OOM-kills instead of silently swapping (which would fake "it fits"). Writes
# results/<label>.log and appends a CSV row to results/matrix.csv.
set -uo pipefail
LABEL="$1"; MEM="$2"; CPU="$3"; shift 3; [ "${1:-}" = "--" ] && shift
mkdir -p results
LOG="results/${LABEL}.log"

START=$(date +%s)
systemd-run --user --scope -q \
  --unit="caprun-${LABEL}-$$" \
  -p MemoryMax="$MEM" -p MemorySwapMax=0 -p CPUQuota="$CPU" \
  -- /usr/bin/time -v "$@" >"$LOG" 2>&1
RC=$?
END=$(date +%s); DUR=$((END-START))

# peak RSS + wall only exist if /usr/bin/time finished (i.e. not OOM-killed mid-run)
PEAK_KB=$(grep -m1 "Maximum resident" "$LOG" 2>/dev/null | grep -oE "[0-9]+")
WALL=$(grep -m1 "Elapsed (wall" "$LOG" 2>/dev/null | sed -E 's/.*: //')
if [ -z "$PEAK_KB" ] || [ "$RC" != "0" ]; then
  # OOM / failure: time -v summary absent or non-zero exit
  if grep -qiE "memory allocation of|Killed|out of memory|Cannot allocate|SIGKILL|oom" "$LOG" 2>/dev/null \
     || [ "$RC" = "137" ] || { [ -z "$PEAK_KB" ] && [ "$RC" != "0" ]; }; then
    OUTCOME="OOM"
  else
    OUTCOME="FAIL(rc=$RC)"
  fi
  PEAK_GB="-"; WALL="${DUR}s(killed)"
else
  OUTCOME="OK"
  PEAK_GB=$(awk "BEGIN{printf \"%.2f\", $PEAK_KB/1024/1024}")
fi

echo "${LABEL},${MEM},${CPU},${OUTCOME},${WALL},${PEAK_GB}" | tee -a results/matrix.csv
