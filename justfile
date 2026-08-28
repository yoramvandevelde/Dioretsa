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

# The Android build. Everything Gradle needs lives under android/, and the C it
# compiles is the same C the desktop builds use, through the CMakeLists above it.
#
# JDK and SDK are resolved inside these recipes rather than at the top of this
# file, because just evaluates a backtick variable on every run: a `just build`
# on a machine without mise should not fail over a toolchain only Android wants.
# Override JAVA_HOME_25 or ANDROID_HOME if either sits somewhere unusual.
#
# `adb devices` lists what is connected; a Google TV normally needs
# `adb connect <ip>:5555` first. Both builds stamp the version as today's date
# + "-local", which is the release tag format with a marker on it, and the
# version code as the current unix timestamp: always higher than the last one,
# so Android never refuses an install as a downgrade and nothing has to be
# bumped by hand. The release build additionally needs the keystore in place at
# android/app/upload-keystore.jks, and SIGNING_KEYSTORE_PASSWORD,
# SIGNING_KEY_PASSWORD and SIGNING_KEY_ALIAS exported.

# Build a debug APK and install it on a device.
build-install device:
    #!/usr/bin/env bash
    set -euo pipefail
    export JAVA_HOME="${JAVA_HOME_25:-$(mise where java@temurin-25)}"
    export ANDROID_HOME="${ANDROID_HOME:-$HOME/Library/Android/sdk}"
    export PATH="$JAVA_HOME/bin:$PATH"
    ./android/gradlew -p android assembleDebug \
        -PversionName="$(date +%Y.%m.%d)-local" -PversionCode="$(date +%s)"
    adb -s {{device}} install -r android/app/build/outputs/apk/debug/app-debug.apk

# Build a signed release APK and install it, for trying one before tagging.
build-install-release device:
    #!/usr/bin/env bash
    set -euo pipefail
    export JAVA_HOME="${JAVA_HOME_25:-$(mise where java@temurin-25)}"
    export ANDROID_HOME="${ANDROID_HOME:-$HOME/Library/Android/sdk}"
    export PATH="$JAVA_HOME/bin:$PATH"
    ./android/gradlew -p android assembleRelease \
        -PversionName="$(date +%Y.%m.%d)-local" -PversionCode="$(date +%s)"
    adb -s {{device}} install -r android/app/build/outputs/apk/release/app-release.apk

# Uninstall: the way between a debug and a release build, which sign differently.
uninstall device:
    adb -s {{device}} uninstall io.sifft.dioretsa || true

clean:
    rm -rf build build-bundle dist android/app/build android/app/.cxx android/build android/.gradle
