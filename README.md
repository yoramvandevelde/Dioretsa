# Dioretsa

An asteroids game about flying badly. Wait... are _those_ following me?

![Dioretsa](docs/screenshot.png)

## Playing

| Key             | Controller        | Action              |
| --------------- | ----------------- | ------------------- |
| Left/Right, A/D | Left stick, D-pad | turn                |
| Up, W           | A, RT, D-pad up   | thrust              |
| Space           | X, RB             | fire                |
| P               | Start             | pause               |
| R               | Y                 | restart, asks first |
| M               | View              | music on/off        |
| Esc             | B                 | quit, asks first    |

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

## Credits

Music: [Dark Cinematic Ambient Tension v2](https://pixabay.com/music/chase-scene-dark-cinematic-ambient-tension-v2-461304/)
by RubyZephyr, from Pixabay.

Type: [Archivo Black](https://fonts.google.com/specimen/Archivo+Black) by
Omnibus-Type, under the SIL Open Font License 1.1. The licence sits next to the
font in `assets/OFL.txt`.

Built with [raylib](https://www.raylib.com/).

Sound effects made with [rfxgen](https://raylibtech.itch.io/rfxgen).

