#!/usr/bin/env bash
set -euo pipefail


NEVT=80000
NPROC=6
BASESEED=77788
MAXENG=100

BASE_FCL="run_inconel_offline_ST.fcl"
CUSTOM_GEOM="geom/pt_material_override.txt"
FINAL_COMBINED="Inconel_6Core_Full.root"

die(){ echo "ERROR: $*" >&2; exit 1; }

cd /work

[ -r "$BASE_FCL" ] || die "Missing /work/$BASE_FCL"
[ -r "$CUSTOM_GEOM" ] || die "Missing /work/$CUSTOM_GEOM"

PRODROOT="/cvmfs/mu2e.opensciencegrid.org/Musings/Production/v00_07_00"
export FHICL_FILE_PATH="/work:${PRODROOT}:${FHICL_FILE_PATH:-}"
export MU2E_SEARCH_PATH="/work:${MU2E_SEARCH_PATH:-}"

TIMESTAMP=$(date +%Y%m%d_%H%M%S)
WORKDIR="work_inconel_${TIMESTAMP}"
mkdir -p "$WORKDIR"

echo "=================================================="
echo "Starting Parallel Run: $NEVT events on $NPROC cores"
echo "Work Directory: $WORKDIR"
echo "Base FCL: $BASE_FCL"
echo "Geometry override: /work/$CUSTOM_GEOM"
echo "FHICL_FILE_PATH=$FHICL_FILE_PATH"
echo "MU2E_SEARCH_PATH=$MU2E_SEARCH_PATH"
echo "=================================================="

base=$(( NEVT / NPROC ))
rem=$(( NEVT % NPROC ))
first=1
pids=()

for i in $(seq 1 $NPROC); do
  n=$base
  if [ "$i" -le "$rem" ]; then n=$((n+1)); fi

  job_dir="$WORKDIR/job_$i"
  mkdir -p "$job_dir"

  # Copy base fcl into job dir so #include "run_inconel_offline.fcl" resolves locally too
  cp -f "/work/$BASE_FCL" "$job_dir/$BASE_FCL"
  # Also copy the included file if you want it fully self-contained:
  cp -f "/work/run_inconel_offline.fcl" "$job_dir/run_inconel_offline.fcl" 2>/dev/null || true

  seed=$(( BASESEED + i*MAXENG ))
  echo "Core $i: Events $first-$((first+n-1)) ($n events, seed=$seed)"

  cat > "$job_dir/job_$i.fcl" <<FCL
#include "$BASE_FCL"
source.firstEvent: $first
source.maxEvents:  $n
services.SeedService.policy: "autoIncrement"
services.SeedService.baseSeed: $seed
FCL

  (
    cd "$job_dir"
    PRODROOT="/cvmfs/mu2e.opensciencegrid.org/Musings/Production/v00_07_00"
export FHICL_FILE_PATH="/work:${PRODROOT}:${FHICL_FILE_PATH:-}"
    export MU2E_SEARCH_PATH="/work:${MU2E_SEARCH_PATH:-}"
    mu2e -c "job_$i.fcl" --nthreads 1 --nschedules 1 \
      --TFileName "data_part_$i.root" > "run_$i.log" 2>&1
  ) &

  pids+=( "$!" )
  first=$(( first + n ))
done

tail -f "$WORKDIR"/job_*/run_*.log &
TAIL_PID=$!

fail=0
for pid in "${pids[@]}"; do
  if ! wait "$pid"; then fail=1; fi
done

kill "$TAIL_PID" 2>/dev/null || true
wait "$TAIL_PID" 2>/dev/null || true

if [ "$fail" -ne 0 ]; then
  echo "ERROR: at least one core failed; not merging."
  exit 1
fi


echo "=================================================="
echo "Geometry lines from logs:"
grep -h "Geometry file:" "$WORKDIR"/job_*/run_*.log | sort -u || true
echo "=================================================="

echo "Merging $NPROC data parts into $FINAL_COMBINED..."
hadd -f "$FINAL_COMBINED" "$WORKDIR"/job_*/data_part_*.root
echo "SUCCESS: $FINAL_COMBINED created."
