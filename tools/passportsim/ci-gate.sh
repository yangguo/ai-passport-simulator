#!/usr/bin/env bash
set -uo pipefail

usage() {
  echo "usage: ci-gate.sh [--corpora id,id,...] [--runs-root DIR] [--update]" >&2
  exit 2
}

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
corpora="gov011,chessfd8fa19,xxlaba64c1"
runs_root="${M2_RUNS_ROOT:-${TMPDIR:-/tmp}/firmware-regression}"
update=0
while (($#)); do
  case "$1" in
    --corpora) (($# >= 2)) || usage; corpora="$2"; shift 2 ;;
    --runs-root) (($# >= 2)) || usage; runs_root="$2"; shift 2 ;;
    --update) update=1; shift ;;
    *) usage ;;
  esac
done
mkdir -p "$runs_root"
IFS=',' read -r -a corpus_list <<< "$corpora"
if [[ -z "${corpora//[[:space:]]/}" ]]; then
  echo "at least one corpus id is required" >&2
  exit 2
fi
override_id="${PASSPORTSIM_OVERRIDE_CORPUS_ID:-}"
if [[ -n "$override_id" ]]; then
  case "$override_id" in gov011|chessfd8fa19|xxlaba64c1) ;; *) echo "invalid source override corpus id: $override_id" >&2; exit 2 ;; esac
  case ",${corpora//[[:space:]]/}," in
    *",$override_id,"*) ;;
    *) echo "source override corpus $override_id must be included in --corpora" >&2; exit 2 ;;
  esac
fi
declare -A seen=()
overall=0
for corpus_id in "${corpus_list[@]}"; do
  corpus_id="${corpus_id//[[:space:]]/}"
  case "$corpus_id" in gov011|chessfd8fa19|xxlaba64c1) ;; *) echo "unknown corpus id: $corpus_id" >&2; overall=1; continue ;; esac
  [[ -n "$corpus_id" ]] || continue
  [[ -z "${seen[$corpus_id]:-}" ]] || { echo "duplicate corpus id: $corpus_id" >&2; overall=1; continue; }
  seen[$corpus_id]=1
  run_dir="$runs_root/$corpus_id"
  rm -rf -- "$run_dir"
  mkdir -p "$run_dir"
  echo "=== firmware regression: $corpus_id ==="
  set +e
  "$script_dir/run-game.sh" --repeat "$corpus_id" "$run_dir"
  run_rc=$?
  if (( update )); then
    python3 "$script_dir/compare-goldens.py" "$corpus_id" "$run_dir" --update
    golden_rc=$?
    python3 "$script_dir/check-warnings.py" "$corpus_id" "$run_dir" --update
    warning_rc=$?
  else
    python3 "$script_dir/compare-goldens.py" "$corpus_id" "$run_dir"
    golden_rc=$?
    python3 "$script_dir/check-warnings.py" "$corpus_id" "$run_dir"
    warning_rc=$?
  fi
  set -e
  text_quality="pass"
  [[ "$corpus_id" != xxlaba64c1 ]] || text_quality="failing-known-tofu (non-blocking)"
  python3 - "$run_dir" "$corpus_id" "$run_rc" "$golden_rc" "$warning_rc" "$text_quality" <<'PY'
import json
import sys
from pathlib import Path
directory = Path(sys.argv[1])
payload = {
    "corpus_id": sys.argv[2], "run_exit_code": int(sys.argv[3]),
    "golden_exit_code": int(sys.argv[4]), "warning_exit_code": int(sys.argv[5]),
    "text_quality": sys.argv[6],
    "passed": all(int(code) == 0 for code in sys.argv[3:6]),
}
(directory / "gate-result.json").write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
PY
  if (( run_rc != 0 || golden_rc != 0 || warning_rc != 0 )); then overall=1; fi
done
python3 - "$runs_root" "$overall" "${corpus_list[@]}" <<'PY'
import json
import sys
from pathlib import Path
root = Path(sys.argv[1])
results = []
for corpus_id in sys.argv[3:]:
    item = root / corpus_id.strip()
    result = item / "gate-result.json"
    if result.is_file():
        results.append(json.loads(result.read_text(encoding="utf-8")))
payload = {"passed": int(sys.argv[2]) == 0, "corpora": results}
(root / "summary.json").write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
print("firmware gate:", "PASS" if payload["passed"] else "FAIL")
for result in results:
    print(f"  {result['corpus_id']}: {'PASS' if result['passed'] else 'FAIL'}; text={result['text_quality']}")
PY
exit "$overall"
