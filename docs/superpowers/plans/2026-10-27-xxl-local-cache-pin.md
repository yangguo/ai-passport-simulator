# Temporary xiaoxiaole local-cache pin Implementation Plan

> **For agentic workers:** Implementation executed inline on the requested branch.

**Goal:** Pin xiaoxiaole font-fix firmware locally with verified provenance and allow CI to report an explicit skip until an upstream run or release exists.

**Architecture:** The fetcher verifies pinned bytes in the private store, seeding from supported env paths or an optional TOML seed directory. The gate detects absent verified bytes before simulation and records a visible skip. Golden and text-quality metadata are updated only after local smoke evidence.

**Tech Stack:** Python 3, TOML, Bash, GitHub Actions.

**Spec:** User request in this task.

## Global Constraints

- Never call `gh` for local-cache sources.
- Never commit firmware binaries or xiaoxiaole PNGs.
- Preserve corpus id `xxlaba64c1` and commit on `pin/xxl-local-57a23fc` only.
- CI skips only an unavailable local-cache pin and must explain the reason.

## Review Focus

- Existing but corrupt cache bytes must be reseeded or fail with install guidance.
- Partial environment overrides must not result in a silent CI skip.
- A skipped corpus must not trigger the known-tofu report step.

### Task 1: Fetch and pin

**Files:** `fetch-firmware.py`, `firmware-sources.toml`, `tests/test_fetch.py`

- [ ] Add local-cache tests for verified seed copy, hashes, permissions, provenance, and no `gh` calls.
- [ ] Implement local-cache resolution and temporary lock values.

### Task 2: CI skip and text-quality behavior

**Files:** `ci-gate.sh`, `.github/workflows/firmware-regression.yml`, `tests/test_ci_gate.py`, golden tools and metadata as justified by local smoke.

- [ ] Add a tested unavailable-cache skip path and explicit gate summary.
- [ ] Run local smoke; update private golden metadata and remove known-tofu labels only if evidence supports it.

### Task 3: Docs, verification, completion

**Files:** `tools/passportsim/README.md`, requested DONE report.

- [ ] Document cache setup, CI behavior, and observed text quality.
- [ ] Run requested tests and smoke; commit changes and record remaining risks.
