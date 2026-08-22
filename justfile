# Everything this project does, in one place.
# `just` on its own lists the recipes.

_default:
    @just --list --unsorted

# Fast build for working: links against the raylib installed by brew.
build:
    cmake -B build
    cmake --build build

# Build and play.
run: build
    ./build/dioretsa

# Build and drop straight into wave 1, unkillable, for looking at later waves.
test-run: build
    ./build/dioretsa --godmode --skip-menu

# Self-contained universal build: raylib compiled from source and linked in, so
# the result runs on a Mac that has never seen Homebrew. Minutes, not seconds.
build-bundle:
    cmake -B build-bundle -DSELF_CONTAINED=ON
    cmake --build build-bundle

# A folder you can hand to someone, plus the zip of it, in dist/.
bundle-mac: build-bundle
    #!/usr/bin/env bash
    set -euo pipefail
    out="dist/dioretsa-macos"
    rm -rf "$out" && mkdir -p "$out"
    cp build-bundle/dioretsa "$out/"
    # Only tracked files travel, so scratch files in assets/ never ride along.
    git ls-files assets | while read -r f; do
        mkdir -p "$out/$(dirname "$f")"
        cp "$f" "$out/$f"
    done
    (cd dist && rm -f dioretsa-macos.zip && zip -qr dioretsa-macos.zip dioretsa-macos)
    echo "dist/dioretsa-macos.zip  $(du -h dist/dioretsa-macos.zip | cut -f1)"

# What ends up in a bundle, without building anything.
bundle-contents:
    @git ls-files assets | sed 's|^|  |'

clean:
    rm -rf build build-bundle dist
