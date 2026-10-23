#!/usr/bin/env bash
set -euo pipefail

readonly upstream='https://github.com/BlackHole1/passportsim'
readonly revision='c94b793413336554283eeb9ca3888dc2a8d38c77'
readonly toolchain='1.93.0'
readonly script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
cache_root="${PASSPORTSIM_CACHE:-${HOME}/.cache/apsim/passportsim}"
checkout="$cache_root/src"
origin_marker="$cache_root/.source-url"

if ! command -v git >/dev/null 2>&1; then
  echo 'setup: git is required' >&2
  exit 1
fi
if ! command -v rustup >/dev/null 2>&1; then
  echo 'setup: rustup is required to install the pinned Rust 1.93.0 toolchain' >&2
  echo 'Install rustup, then rerun tools/passportsim/setup.sh.' >&2
  exit 1
fi
if ! command -v sha256sum >/dev/null 2>&1; then
  echo 'setup: sha256sum is required' >&2
  exit 1
fi

mkdir -p "$cache_root"
# A CI cache restore may create only "$checkout/target"; keep it and clone around it.
restored_target=""
if [[ -d "$checkout" && ! -f "$origin_marker" && ! -e "$checkout/.git" \
      && "$(ls -A "$checkout")" == "target" ]]; then
  restored_target="$cache_root/.restored-target"
  rm -rf -- "$restored_target"
  mv -- "$checkout/target" "$restored_target"
  rmdir -- "$checkout"
fi
if [[ -e "$checkout" && ! -f "$origin_marker" ]]; then
  echo "setup: refusing to replace unmarked cache path: $checkout" >&2
  exit 1
fi
if [[ ! -d "$checkout/.git" ]]; then
  rm -rf -- "$checkout"
  git clone --no-checkout "$upstream" "$checkout"
  printf '%s\n' "$upstream" > "$origin_marker"
fi
if [[ -n "$restored_target" && ! -e "$checkout/target" ]]; then
  mv -- "$restored_target" "$checkout/target"
fi
if [[ "$(cat "$origin_marker")" != "$upstream" ]]; then
  echo "setup: cache origin marker does not match $upstream" >&2
  exit 1
fi
git -C "$checkout" fetch --quiet origin "$revision"
git -C "$checkout" checkout --quiet --detach "$revision"
actual="$(git -C "$checkout" rev-parse HEAD)"
if [[ "$actual" != "$revision" ]]; then
  echo "setup: checkout mismatch: expected $revision, got $actual" >&2
  exit 1
fi
git -C "$checkout" clean -ffdqx -e target/
if git -C "$checkout" apply --reverse --check "$script_dir/patches/0001-batch-mode.patch"; then
  echo 'setup: batch mode patch is already applied'
else
  git -C "$checkout" apply --check "$script_dir/patches/0001-batch-mode.patch"
  git -C "$checkout" apply "$script_dir/patches/0001-batch-mode.patch"
fi

# RUSTUP_TOOLCHAIN takes precedence over the upstream rust-toolchain.toml, avoiding its
# macOS/Windows/WASM targets on Linux while retaining the pinned compiler and Cargo.
rustup toolchain install "$toolchain" --profile minimal
export CARGO_BUILD_JOBS=2
rustup run "$toolchain" cargo build --release --locked -j2 -p pemu-cli --manifest-path "$checkout/Cargo.toml"

binary="$checkout/target/release/passportsim"
if [[ ! -x "$binary" ]]; then
  echo "setup: successful build did not produce executable $binary" >&2
  exit 1
fi
printf 'binary: %s\n' "$binary"
printf 'sha256: %s\n' "$(sha256sum "$binary" | awk '{print $1}')"
