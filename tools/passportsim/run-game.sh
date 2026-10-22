#!/usr/bin/env bash
set -euo pipefail

usage() {
  echo "usage: run-game.sh [--repeat] [--no-fetch] <corpus-id> <outdir>" >&2
  echo "corpus ids: gov011 chessfd8fa19 xxlaba64c1" >&2
}

repeat=0
no_fetch=0
while [[ "${1:-}" == --* ]]; do
  case "$1" in
    --repeat) repeat=1 ;;
    --no-fetch) no_fetch=1 ;;
    *) usage; exit 2 ;;
  esac
  shift
done
if [[ $# -ne 2 ]]; then
  usage
  exit 2
fi
corpus_id="$1"
outdir="$(mkdir -p -- "$2" && cd -- "$2" && pwd -P)"
case "$corpus_id" in
  gov011|chessfd8fa19|xxlaba64c1) ;;
  *) usage; exit 2 ;;
esac

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
binary="${PASSPORTSIM_BIN:-${PASSPORTSIM_CACHE:-${HOME}/.cache/apsim/passportsim}/src/target/release/passportsim}"
if [[ ! -x "$binary" ]]; then
  echo "run-game: simulator binary is missing or not executable: $binary" >&2
  echo "Run tools/passportsim/setup.sh or set PASSPORTSIM_BIN." >&2
  exit 1
fi

run_once() {
  local run_name="$1"
  local run_dir="$outdir/$run_name"
  local state_dir="$run_dir/home"
  local manifest="$state_dir/firmware-manifest.json"
  local batch="$run_dir/commands.txt"
  mkdir -p -- "$run_dir/artifacts"
  local configured=1
  case "$corpus_id" in
    gov011) [[ -n "${PASSPORTSIM_GOV011_BIN:-}" ]] && configured=0 ;;
    chessfd8fa19) [[ -n "${PASSPORTSIM_CHESSFD8FA19_BIN:-}" && -n "${PASSPORTSIM_CHESSFD8FA19_ELF:-}" ]] && configured=0 ;;
    xxlaba64c1) [[ -n "${PASSPORTSIM_XXLABA64C1_BIN:-}" && -n "${PASSPORTSIM_XXLABA64C1_ELF:-}" ]] && configured=0 ;;
  esac
  [[ -n "${FIRMWARE_DIR:-}" ]] && configured=0
  if (( configured )) && (( ! no_fetch )); then
    python3 "$script_dir/fetch-firmware.py" "$corpus_id" --home "$state_dir" > "$run_dir/configure.log"
  else
    if (( configured )) && (( no_fetch )); then
      echo "run-game: firmware is not configured and --no-fetch was supplied" >&2
      return 2
    fi
    PASSPORTSIM_HOME="$state_dir" python3 "$script_dir/configure-corpus.py" "$state_dir" --corpus-id "$corpus_id" > "$run_dir/configure.log"
  fi
  cat > "$batch" <<COMMANDS
start $corpus_id --boot until_ui_settled --boot-timeout 20s --artifacts $run_dir/artifacts
screenshot --save-as 01-boot --artifacts $run_dir/artifacts
ui --max-nodes 300 --artifacts $run_dir/artifacts
input down click --artifacts $run_dir/artifacts
run --for 1s --timeout 20s --wall-budget-ms 60000 --artifacts $run_dir/artifacts
screenshot --save-as 02-after-down --artifacts $run_dir/artifacts
ui --max-nodes 300 --artifacts $run_dir/artifacts
input up click --artifacts $run_dir/artifacts
run --for 1s --timeout 20s --wall-budget-ms 60000 --artifacts $run_dir/artifacts
screenshot --save-as 03-after-up --artifacts $run_dir/artifacts
ui --max-nodes 300 --artifacts $run_dir/artifacts
input ok click --artifacts $run_dir/artifacts
run --for 1s --timeout 20s --wall-budget-ms 60000 --artifacts $run_dir/artifacts
screenshot --save-as 04-after-ok --artifacts $run_dir/artifacts
ui --max-nodes 300 --artifacts $run_dir/artifacts
serial read --cursor 0 --max-bytes 65536 --artifacts $run_dir/artifacts
status --output json --artifacts $run_dir/artifacts
COMMANDS
  set +e
  PASSPORTSIM_HOME="$state_dir" \
    PASSPORTSIM_BATCH="$batch" \
    PASSPORTSIM_BATCH_ARTIFACTS="$run_dir/artifacts" \
    PASSPORTSIM_BATCH_KEEP_GOING=1 \
    timeout 900 "$binary" status > "$run_dir/session.log" 2>&1
  local cli_exit_code=$?
  set -e
  python3 "$script_dir/summarize-run.py" \
    --run-dir "$run_dir" --manifest "$manifest" --corpus-id "$corpus_id" --cli-exit-code "$cli_exit_code"
  python3 - "$run_dir/summary.json" <<'PY'
import json
import sys
with open(sys.argv[1], encoding="utf-8") as stream:
    raise SystemExit(json.load(stream)["exit_code"])
PY
}

first_status=0
run_once run1 || first_status=$?
if (( repeat )); then
  second_status=0
  run_once run2 || second_status=$?
  python3 "$script_dir/compare-runs.py" "$outdir"
else
  cp "$outdir/run1/summary.json" "$outdir/summary.json"
  python3 - "$outdir/summary.json" <<'PY'
import json
import sys
with open(sys.argv[1], encoding="utf-8") as stream:
    summary = json.load(stream)
summary["determinism_pass"] = False
summary["exit_code"] = 1
with open(sys.argv[1], "w", encoding="utf-8") as stream:
    json.dump(summary, stream, indent=2)
    stream.write("\n")
PY
fi
if (( repeat )); then
  python3 - "$outdir/summary.json" <<'PY'
import json
import sys
with open(sys.argv[1], encoding="utf-8") as stream:
    raise SystemExit(json.load(stream)["exit_code"])
PY
else
  exit 1
fi
