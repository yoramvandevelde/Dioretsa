# Dioretsa

An asteroids game about flying badly. Wait... are _those_ following me?

![Dioretsa](docs/screenshot.png)

## Playing

| Key             | Controller        | Action           |
| --------------- | ----------------- | ---------------- |
| Left/Right, A/D | Left stick, D-pad | turn             |
| Up, W           | A, RT, D-pad up   | thrust           |
| Space           | X, RB             | fire             |
| P               | Start             | pause            |
| R               | Y                 | restart, asks first |
| M               | View              | music on/off     |
| Esc             | B, Back           | quit, asks first |

The stick turns as far as you push it; the D-pad and the keys turn flat out.

## Running it

Download the zip for your machine, unpack it, and run the program inside. The
folder holds the game and its assets; keep them together.

It is not signed, so macOS refuses it the first time. Right-click the program,
choose Open, and confirm; after that it starts normally.

## Building it

```sh
brew install raylib cmake just
just run
```

`just` on its own lists everything else, including `just bundle-mac`, which
builds a version that runs on any Mac without Homebrew.

## On a television

There is an Android build for Google TV, made from the same source as every
other one. It wants a game controller; the table above is the whole of it, with
the remote's back button asking the same question Esc does.

```sh
just build-install <device>
```

`adb devices` lists what is connected, and a Google TV usually needs an
`adb connect <ip>:5555` before it shows up there. `just build-install-release`
builds the signed one instead, and expects `SIGNING_KEYSTORE_PASSWORD`,
`SIGNING_KEY_PASSWORD` and `SIGNING_KEY_ALIAS` in the environment with the
keystore at `android/app/upload-keystore.jks`.

The Android toolchain is a JDK 25, the SDK, NDK 28 and CMake 3.31 or newer;
`sdkmanager "cmake;3.31.6"` if the SDK only has the 3.22 it installs by default,
which raylib is too new for.

Releasing works the same as for the other three: push a `YYYY.MM.DD` tag and the
workflow builds all four, signs the APK and hangs everything on the same release.
That wants four repository secrets: `SIGNING_JKS_FILE_BASE64`, which is the
keystore itself (`base64 -i android/app/upload-keystore.jks`), plus
`SIGNING_KEYSTORE_PASSWORD`, `SIGNING_KEY_PASSWORD` and `SIGNING_KEY_ALIAS`.
Locally those three come from `.github/signing.env`, which is git-ignored;
`.github/signing.env.example` explains how to make the keystore and what to put
in it.

## Credits

Music: [Dark Cinematic Ambient Tension v2](https://pixabay.com/music/chase-scene-dark-cinematic-ambient-tension-v2-461304/)
by RubyZephyr, from Pixabay.

Type: [Archivo Black](https://fonts.google.com/specimen/Archivo+Black) by
Omnibus-Type, under the SIL Open Font License 1.1. The licence sits next to the
font in `assets/OFL.txt`.

Built with [raylib](https://www.raylib.com/).

Sound effects made with [rfxgen](https://raylibtech.itch.io/rfxgen).

