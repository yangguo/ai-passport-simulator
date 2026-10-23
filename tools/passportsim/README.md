# Real firmware runs with passportsim (M1 source flow and M2 CI gate)

This directory pins and builds BlackHole1/passportsim for local headless firmware checks. The
runner fetches firmware from exact game-repository release/run identities in
`firmware-sources.toml`, verifies every consumed file against its pinned SHA-256, and writes source
provenance into each run manifest. Firmware bytes stay outside git.

## M2 CI gate

`.github/workflows/firmware-regression.yml` runs sequential two-repeat smoke
checks on Ubuntu 24.04 for `gov011`, `chessfd8fa19`, and `xxlaba64c1`. The same
gate is available locally through `tools/passportsim/ci-gate.sh`; it compares
four checkpoint hashes against firmware-bound JSON goldens and rejects warning
signatures outside `warning-allowlist/`. Chess PNGs are public and checked in.
The Go and xiaoxiaole repositories are private, so their expected PNGs remain
restricted; their JSON hashes still gate navigation. Each run uploads actual
screenshots, available expected/diff screenshots, session and UART logs, and
JSON results for 14 days. Reusable workflow callers can supply one artifact
source override with exact repository, workflow run, artifact, and expected
SHA-256 values. Pull requests from forks receive a skip notice because they do
not receive the private firmware read token.

The current xiaoxiaole checkpoints are labeled `failing-known-tofu`. Their
pixel baselines test navigation only. A separate non-blocking workflow step
reports the text-quality failure; it does not turn the known tofu into a text
pass.

## Pins

- Upstream: `https://github.com/BlackHole1/passportsim`
- Commit: `c94b793413336554283eeb9ca3888dc2a8d38c77` (`v1.0.3`)
- Rust toolchain: `1.93.0` from upstream `rust-toolchain.toml`; `setup.sh` selects the minimal host
  toolchain so macOS, Windows, and WASM targets are not downloaded on Linux.
- Local integration: `patches/0001-batch-mode.patch`, applied with `git apply` after checking out
  the exact commit.

## Build and run

Install `rustup`, then build the pinned CLI:

```sh
PASSPORTSIM_CACHE=/workspace/m0-cache tools/passportsim/setup.sh
```

`setup.sh` prints the release binary path and its SHA-256. By default, `run-game.sh` uses `gh`
authentication to fetch only the requested corpus into `$PASSPORTSIM_FIRMWARE_STORE` or
`~/.cache/apsim/firmware` (created with mode `0700`). It skips downloads when cached files pass
their locked hashes. `--no-fetch` disables this behavior. Existing `FIRMWARE_DIR` and
`PASSPORTSIM_*_BIN` / `PASSPORTSIM_*_ELF` overrides remain available:

```sh
tools/passportsim/run-game.sh --repeat gov011 /workspace/m1-runs/gov011
tools/passportsim/run-game.sh --repeat chessfd8fa19 /workspace/m1-runs/chessfd8fa19
tools/passportsim/run-game.sh --repeat xxlaba64c1 /workspace/m1-runs/xxlaba64c1
```

The source inventory points to [Go](https://github.com/yangguo/ai-passport-go) release `v0.1.1`
(source `0762e467c8b1759294aa78694fa918f81683930c`), [chess](https://github.com/yangguo/ai-passport-chess)
run `38233745799` (source `fd8fa19718acb60971bcb33ef9738c1d959d4914`; the PR artifact name uses merge commit `09ab758…`, not this head commit), and
[xiaoxiaole](https://github.com/yangguo/ai-passport-xiaoxiaole) run `38265044174` (source
`aba64c1a41613e8cbc60289dd9c57d1f431bed5a`). Chess and xiaoxiaole artifacts currently expire
2027-01-10; xiaoxiaole's selected run is from `feat/level-variety`, because its firmware workflow
is not yet on `main`. Go's release and inspected CI zip contain no ELF, so Go `ui` tree inspection
is unavailable until its workflow publishes one. Expired or missing artifacts fail with an
instruction to pin a reviewed replacement; the fetcher never falls back to a moving branch.

Each run creates a fresh `PASSPORTSIM_HOME`, resolves `config/corpus.toml`, hashes all available
firmware and ELF payloads into `firmware-manifest.json`, records source repo/run-or-tag/commit, and
runs one simulator process. `--repeat`
creates two fresh states and compares the ordered screenshot SHA-256 lists in `summary.json`.
Artifacts, command files, logs, config, and manifests stay under the requested output directory.

The batch file has one CLI command per non-empty line; a line beginning with `#` is ignored. Words
are split on whitespace, so paths and arguments containing spaces are not supported. Commands in a
batch share one process and one instance pool, use `--ephemeral`, and do not start or connect to a
daemon. Every command prints `>>> <line>: <command>` followed by `<<< exit N`. The first nonzero
command stops the batch; set `PASSPORTSIM_BATCH_KEEP_GOING=1` to continue. The process returns the
last nonzero command status (or zero when every command succeeds).

`run-game.sh` sets `PASSPORTSIM_BATCH_KEEP_GOING=1` so UI tree queries do not prevent later button
navigation from running. Exit 10 is `PASS_WITH_CAVEATS` in the pinned CLI; for these timed
`run --for 1s` commands, the log also confirms that the requested virtual duration elapsed. The
runner accepts exit 10 only for those timed run commands. It records the process exit code and each
command outcome in `summary.json`.

The Go missing-ELF `ui` error and chess LVGL 9.6 `ui` error are recorded as known limitations with
`ui_tree_ok: false` and a reason. They do not fail the run. The summary checks that the boot
screenshot is nonblank and that each navigation screenshot differs from the preceding screen.
With `--repeat`, `summary.json` also compares all four screenshot hashes across the two runs and
sets the final exit code to zero exactly when boot, navigation, and determinism pass. For
`xxlaba64c1`, `text_quality` records `failing (known font/tofu issue, separately tracked)`; that
known text-quality issue is separate from the navigation and determinism gates.

## M1 execution evidence

The 2026-10-22 clean-box runs for all three locked sources are recorded in
`/workspace/m1-runs/REPORT.md`. Two fresh runs per game passed boot, screenshot-based navigation,
and screenshot determinism. Go `ui` inspection remains unavailable without an ELF; chess `ui`
inspection remains unsupported for its LVGL 9.6 layout; xiaoxiaole `ui` inspection passes but its
known tofu/digit text issue remains. Navigation verifies that the framebuffer changes after each
key; it does not yet prove semantic control activation. The ten-run acceptance and warning-policy
work remain open, and this local evidence does not create a firmware CI gate.

The M0 game sequence is:

```text
start <corpus-id> --boot until_ui_settled --boot-timeout 20s
screenshot
ui --max-nodes 300
input down click
run --for 1s
screenshot
ui --max-nodes 300
input up click
run --for 1s
screenshot
ui --max-nodes 300
input ok click
run --for 1s
screenshot
ui --max-nodes 300
serial read --cursor 0 --max-bytes 65536
status --output json
```

The tracked runner writes one command per line and passes `--artifacts` to each command. The pinned
CLI's command docs are in its checkout under `docs/commands/`.

## Limitations

- These are local M1 results, not a firmware CI gate. CI only builds passportsim; it does not
  receive private firmware artifacts.
- The firmware cache is private and outside the repository (Decision 2 in the design document).
  Do not copy its firmware bytes into git. `--update-lock` is an explicit reviewed operation for
  updating expected hashes after inspecting a newly selected artifact.
- `ui` tree inspection is unavailable for Go without an ELF and unsupported for chess on LVGL
  9.6. These outcomes stay visible as known limitations in the summary.
- xiaoxiaole currently has a font/tofu rendering issue. Navigation can be checked, but tofu is not
  an approved text-quality golden.
- Linux is not listed in upstream `host_parity`; a successful Linux build/run remains an explicit
  integration result, not an upstream support claim.
- These game images are ESP-IDF 5.5.3 coverage. XiaoZhi's IDF 5.5.3 compatibility build is interim
  evidence; production IDF 6.1 coverage remains open. WebSocket is first, and MQTT+UDP is
  unsupported until its own checks pass.
- Diagnostic budgets and 14-day retention are provisional per the approved design decisions.

## Verification status for this checkout

Verified on 2026-10-22 with Rust 1.93.0 and the Linux release binary
`/workspace/m0-cache/src/target/release/passportsim` (SHA-256
`1e46e382e30d8ba967f7f728b340764c208ba2c959a9a849814ebb49fadc8bd2`). Each game booted, passed
the three screenshot navigation checks, and produced matching four-screenshot hashes across two
independently started runs. The setup script recognizes its already-applied batch patch and can be
rerun safely. The Go and chess UI tree checks are known limitations; timed runs exit 10
(`PASS_WITH_CAVEATS`) and are accepted by the runner. xiaoxiaole navigation passed while its known
font/tofu text-quality issue remains failing. Per-game command outcomes, firmware SHA-256 pins, and
screenshot paths are in `/workspace/m0-runs/REPORT.md` and each game's `summary.json`.
