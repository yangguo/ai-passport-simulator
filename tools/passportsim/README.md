# Real firmware runs with passportsim (M0)

This directory pins and builds BlackHole1/passportsim for local headless firmware checks. M0
supports the three game images below through a single-process command batch. Firmware and ELF files
remain external inputs; this repository stores only their resolved SHA-256 values in each run's
manifest.

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

`setup.sh` prints the release binary path and its SHA-256. To resolve local firmware, set
`FIRMWARE_DIR` to a directory with `gov011/`, `chess93dd170/`, and `xxlcac7eb1/` subdirectories,
or set all five explicit `PASSPORTSIM_*_BIN` / `PASSPORTSIM_*_ELF` variables. The explicit names
are shown in `configure-corpus.py`. For the locally supplied artifacts used by the M0 walkthrough:

```sh
export FIRMWARE_DIR=/workspace
export PASSPORTSIM_GOV011_BIN=/workspace/pub-go/fw/ai-passport-go-v0.1.1-release-merged.bin
export PASSPORTSIM_CHESS93DD170_BIN=/workspace/pub/fw/firmware-93dd17078ac5647260ccd3d0cd3191bd0a4e52dc/merged.bin
export PASSPORTSIM_CHESS93DD170_ELF=/workspace/pub/fw/firmware-93dd17078ac5647260ccd3d0cd3191bd0a4e52dc/ai-passport-chess.elf
export PASSPORTSIM_XXLCAC7EB1_BIN=/workspace/xxl-fw/firmware-cac7eb164905d34a55318308fc574167e26920a5/merged.bin
export PASSPORTSIM_XXLCAC7EB1_ELF=/workspace/xxl-fw/firmware-cac7eb164905d34a55318308fc574167e26920a5/ai-passport-xiaoxiaole.elf
tools/passportsim/run-game.sh --repeat gov011 /workspace/m0-runs/gov011
tools/passportsim/run-game.sh --repeat chess93dd170 /workspace/m0-runs/chess93dd170
tools/passportsim/run-game.sh --repeat xxlcac7eb1 /workspace/m0-runs/xxlcac7eb1
```

Each run creates a fresh `PASSPORTSIM_HOME`, resolves `config/corpus.toml`, hashes all available
firmware and ELF payloads into `firmware-manifest.json`, and runs one simulator process. `--repeat`
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
`xxlcac7eb1`, `text_quality` records `failing (known font/tofu issue, separately tracked)`; that
known text-quality issue is separate from the navigation and determinism gates.

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

- This is local M0 evidence, not yet a firmware CI gate. CI only builds passportsim; it does not
  receive private firmware artifacts.
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
