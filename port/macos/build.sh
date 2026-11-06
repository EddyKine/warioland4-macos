#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
workspace=$(dirname "$repo")
mkdir -p "$workspace/.work/tmp" "$workspace/.work/cache/clang"
export TMPDIR="$workspace/.work/tmp"
export CLANG_MODULE_CACHE_PATH="$workspace/.work/cache/clang"
export XDG_CACHE_HOME="$workspace/.work/cache"
cmake -S "$repo" -B "$repo/build/macos-arm64" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_OSX_ARCHITECTURES=arm64 -DWL4_DARWIN_AUDIO_EXPERIMENT=OFF -DSDL3_DIR=/opt/homebrew/lib/cmake/SDL3
cmake --build "$repo/build/macos-arm64" -j 6
python3 "$repo/port/macos/package.py"
