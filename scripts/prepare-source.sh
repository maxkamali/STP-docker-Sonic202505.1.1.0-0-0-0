#!/bin/bash
set -euo pipefail
repo_dir=$(cd "$(dirname "$0")/.." && pwd)
source_commit=0a74023f3a1bac67e61e2568687aaba78d4a78fc
target_dir=${1:?Usage: prepare-source.sh NEW_SOURCE_DIRECTORY}
if [[ -e "$target_dir" ]]; then
    echo 'Refusing to overwrite an existing source directory.' >&2
    exit 1
fi
mkdir -p "$(dirname "$target_dir")"
if [[ -n ${STP_UPSTREAM_SOURCE:-} ]]; then
    # Export the pinned commit, never copy uncommitted local changes.
    git -C "$STP_UPSTREAM_SOURCE" cat-file -e "$source_commit^{commit}"
    mkdir "$target_dir"
    git -C "$STP_UPSTREAM_SOURCE" archive "$source_commit" | tar -x -C "$target_dir"
    git -C "$target_dir" init -q
else
    git clone --no-checkout https://github.com/sonic-net/sonic-stp.git "$target_dir"
    git -C "$target_dir" checkout --detach "$source_commit"
fi
for patch_file in "$repo_dir"/patches/*.patch; do
    (cd "$target_dir" && git apply --check "$patch_file" && git apply "$patch_file")
done
mkdir -p "$target_dir/tests"
cp "$repo_dir/tests/wire_regression.c" "$repo_dir/tests/run-wire-tests.sh" "$target_dir/tests/"
printf 'Prepared source at %s (upstream %s)\n' "$target_dir" "$source_commit"
