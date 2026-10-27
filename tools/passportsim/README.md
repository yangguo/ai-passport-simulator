# Real firmware runs with passportsim (M1 source flow and M2 CI gate)

This directory pins and builds BlackHole1/passportsim for local headless firmware checks. The
runner fetches firmware from exact game-repository release/run identities in
`firmware-sources.toml`, verifies every consumed file against its pinned SHA-256, and writes source
provenance into each run manifest. Firmware bytes stay outside git.

## M2 CI gate

`.github/workflows/firmware-regression.yml` runs sequential two-repeat smoke
checks on Ubuntu 24.04 for `gov011` and `chessfd8fa19`; it includes
`xxlaba64c1` when the temporary local-cache files are present and otherwise
records a documented skip. The same
gate is available locally through `tools/passportsim/ci-gate.sh`; it compares
four checkpoint hashes against firmware-bound JSON goldens and rejects warning
signatures outside `warning-allowlist/`. Chess PNGs are public and checked in.
Boot, navigation, and determinism remain required for all three corpora. UI-tree
inspection is additionally required for `chessfd8fa19` and `xxlaba64c1`; Go
(`gov011`) remains a known skip while its firmware artifact has no ELF. Xiaoxiaole
`text_quality` is now recorded as `pass` after visual review of the font-fix
pin screenshots (CJK and digits render clearly at the navigation checkpoints).
The Go and xiaoxiaole repositories are private, so their expected PNGs remain
restricted; their JSON hashes still gate navigation. Each run uploads actual
screenshots, available expected/diff screenshots, session and UART logs, and
JSON results for 14 days. Reusable workflow callers can supply one artifact
source override with exact repository, workflow run, artifact, and expected
SHA-256 values. Pull requests from forks receive a skip notice because they do
not receive the private firmware read token.

Xiaoxiaole is temporarily pinned to the local font-fix build at source commit
`57a23fc5e093696cc0c226c1d265969a3fadc4e2`, with the merged binary and ELF
verified by SHA-256. No GitHub Actions run or release exists for that commit
yet. The `xxlaba64c1` cache files are therefore not available on a clean CI
runner: the gate records an explicit nonblocking `SKIP` for this corpus until
xiaoxiaole publishes a run or release. If CI already has matching files in
its verified store, or both firmware overrides are supplied, it runs normally.

The temporary pin uses the private store
`$PASSPORTSIM_FIRMWARE_STORE/files/xxlaba64c1/` (default
`~/.cache/apsim/firmware/files/xxlaba64c1/`) and expects `merged.bin` and
`ai-passport-xiaoxiaole.elf`. The fetcher checks both locked hashes, uses
matching store files first, then can seed from `PASSPORTSIM_XXLABA64C1_BIN` /
`PASSPORTSIM_XXLABA64C1_ELF`, then the optional lock field `local_seed_dir`.
Seeded copies are installed with mode `0600` and rechecked. If none are
available, fetch fails with installation guidance; CI's gate detects that
condition before the run and records why it skipped. To install manually:

```sh
install -d -m 700 ~/.cache/apsim/firmware/files/xxlaba64c1
install -m 600 /workspace/xxl-fw/digit-tofu/merged.bin ~/.cache/apsim/firmware/files/xxlaba64c1/merged.bin
install -m 600 /workspace/xxl-fw/digit-tofu/ai-passport-xiaoxiaole.elf ~/.cache/apsim/firmware/files/xxlaba64c1/ai-passport-xiaoxiaole.elf
sha256sum ~/.cache/apsim/firmware/files/xxlaba64c1/*
```

Local smoke against the temporary pin passed boot, navigation, UI-tree, and
determinism. Checkpoint PNG hashes matched the prior navigation baselines;
golden JSON was updated to the new `merged.bin` SHA-256 and `text_quality`
was set to `pass` after inspecting `01-boot` / `04-after-ok` (clear CJK and
digits such as 第1关 / 150分 / 12步). Private expected PNGs stay out of git.

## Pins

- Upstream: `https://github.com/BlackHole1/passportsim`
- Commit: `c94b793413336554283eeb9ca3888dc2a8d38c77` (`v1.0.3`)
- Rust toolchain: `1.93.0` from upstream `rust-toolchain.toml`; `setup.sh` selects the minimal host
  toolchain so macOS, Windows, and WASM targets are not downloaded on Linux.
- Local integration: sorted `patches/*.patch` files, applied with `git apply` after checking out
  the exact commit. `0001-batch-mode.patch` provides batch execution;
  `0002-lvgl-96-object-flags.patch` reconstructs LVGL 9.6 object flags from DWARF bitfields while
  preserving LVGL 9.5's packed `flags` word.

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
[xiaoxiaole](https://github.com/yangguo/ai-passport-xiaoxiaole) is temporarily pinned to local
firmware from `57a23fc5e093696cc0c226c1d265969a3fadc4e2`; it has no published Actions run yet.
Chess's artifact currently expires 2027-01-10. Go's release and inspected CI zip contain no ELF, so Go `ui` tree inspection
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

The Go missing-ELF `ui` error is recorded as a known limitation with `ui_tree_ok: false` and a
reason. Chess and xiaoxiaole UI-tree errors fail the run. The summary checks that the boot
screenshot is nonblank and that each navigation screenshot differs from the preceding screen.
With `--repeat`, `summary.json` also compares all four screenshot hashes across the two runs and
sets the final exit code to zero exactly when boot, navigation, and determinism pass. Xiaoxiaole
`text_quality` is recorded as `pass` on the temporary font-fix pin after visual review.

## M1 execution evidence

The 2026-10-22 clean-box runs for all three locked sources are recorded in
`/workspace/m1-runs/REPORT.md`. Two fresh runs per game passed boot, screenshot-based navigation,
and screenshot determinism. Go `ui` inspection remains unavailable without an ELF; the LVGL 9.6
limitation described by that historical report is addressed by patch 0002. xiaoxiaole `ui`
inspection passes. Historical tofu/digit notes apply to the pre-font-fix pin only.
Navigation verifies that the framebuffer changes after each key; it does not yet prove semantic
control activation. The ten-run acceptance and warning-policy work remain open, and this local
evidence does not create a firmware CI gate.

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
- `ui` tree inspection is unavailable for Go without an ELF. Chess LVGL 9.6 inspection is supported
  by the second tracked patch and is required to pass when an ELF is present.
- xiaoxiaole's temporary font-fix pin (`57a23fc…`) is recorded with `text_quality: pass` after
  visual review; replace this pin with a published Actions/release artifact when available.
- Linux is not listed in upstream `host_parity`; a successful Linux build/run remains an explicit
  integration result, not an upstream support claim.
- These game images are ESP-IDF 5.5.3 coverage. XiaoZhi's IDF 5.5.3 compatibility build is interim
  evidence; production IDF 6.1 coverage remains open. WebSocket is first, and MQTT+UDP is
  unsupported until its own checks pass.
- Diagnostic budgets and 14-day retention are provisional per the approved design decisions.

## Verification status for this checkout

Verified on 2026-10-24 with Rust 1.93.0. `setup.sh` applied or recognized both patches and built the
Linux release binary (SHA-256 `e8a42a3685b8034f5cd494c7126b49496c66a4acd447cea32d131c6318c0413d`).
Two fresh runs each for chess and xiaoxiaole passed boot, navigation, determinism, and all four
required UI-tree commands. Go remains a known missing-ELF skip. Xiaoxiaole's temporary font-fix
pin later recorded `text_quality: pass` after visual review (see golden JSON). Per-run summaries
and screenshots for the M3 verification session are under `/tmp/m3-final/`.
