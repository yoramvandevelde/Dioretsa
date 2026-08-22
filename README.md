# astroid

Asteroids clone in C using [raylib](https://www.raylib.com/).

## Build

```sh
brew install raylib cmake          # if raylib is missing, CMake fetches it itself
cmake -B build
cmake --build build
./build/astroid
```

## Controls

| Key               | Action        |
| ----------------- | ------------- |
| Left/Right, A/D   | turn          |
| Up, W             | thrust        |
| Space             | fire          |
| P                 | pause         |
| R                 | restart       |
| Esc               | quit          |

## Layout

- `src/main.c` - window, input and the game loop on a fixed timestep (60 Hz)
- `src/game.h` - data: `Entity`, `Ship`, `Bullet`, `Enemy`, `EnemyType`, `Game`, `Input`
- `src/game.c` - update, collisions and drawing
- `src/fx.h` / `src/fx.c` - star field, particles, glow helpers and the title font
- `assets/` - Archivo Black, used for the wave title only

The world is 1280x720 and wraps on every edge, exactly one world across.
Anything straddling an edge is drawn on the far side as well (`ghost_positions`),
corners included, so objects slide across instead of popping in and out, and
distances are measured the short way round (`torus_offset`) so what overlaps on
screen also overlaps in the maths. `main.c` translates raw keys into
an `Input` struct, so `game.c` never has to know about key codes.

Enemies and bullets live in fixed-size pools; a slot is free as soon as
`base.alive` is off. That allows spawning in the middle of an update (an enemy
splitting apart) without the loop underneath shifting around.

## Enemy types

All balancing sits in `ENEMY_TYPES` at the top of `src/game.c`: side count,
radius, speed, colour, score, behaviour and what it splits into. A new type is
one line in the `EnemyTypeId` enum plus one row in that table, nothing else.

| Type     | Sides | Splits into  | Score | Graze |
| -------- | ----- | ------------ | ----- | ----- |
| hexagon  | 6     | 2x pentagon  | 20    | 1     |
| pentagon | 5     | 2x square    | 50    | 4     |
| square   | 4     | 2x triangle  | 100   | 12    |
| triangle | 3     | nothing      | 200   | 40    |

Shapes are regular polygons drawn with `DrawPolyLines`, so all sides are equal.
Behaviour lives in `enemy_behave()`: `DRIFT` (straight line), `SPINNER` (tumbles
faster) and `SEEK` (steers towards the ship, as the triangle does).

## Waves

Clearing a wave puts its title on screen: one second standing still at 85% of
the screen width so it can be read, then one second growing to 260% while it
fades, which puts it over the game rather than on it. The growth is squared, so
it starts from a standstill and the hold flows into the rush without a kink.

The title is set in Archivo Black, loaded once after the window opens and used
for nothing else; the HUD keeps the built-in font. The loader looks next to the
working directory and next to the executable, and falls back to the built-in
font if neither turns up, so a missing file costs you the typeface and nothing
more.

The next wave spawns the moment the zoom starts, and the ship is untouchable
until the letters clear. Nobody dies to an enemy that came out from behind a
letter, and grazing is dead in that window because it already keys off the
shield.

## Grazing

Flying close pays. Every 0.25s spent inside a band 26 px beyond the hulls pays
the sum of the graze values in range, multiplied by how many are in range at
once (capped at four). The timer resets the moment you leave, so partial ticks
cannot be banked across passes.

| what | points per second |
| ---- | ----------------- |
| one hexagon | 4 |
| one pentagon | 16 |
| one square | 48 |
| one triangle | 160 |
| four triangles at once | 2560 |

Two conditions, and no others: you are inside the band, and your shield is
down. Invulnerability means no risk, so it pays nothing.

Staying alongside on purpose counts as well. An earlier rule required relative
movement to stop players parking next to something, but from the cockpit that
was invisible: the same gap paid while flying past and stopped paying the
moment you turned to follow. The real cost of chasing is attention, not the
rule. You are steering at one object while the rest of the field keeps moving,
and a triangle accelerates into you the moment you try.

## Effects

`Fx` is cosmetic state, held inside `Game` so it freezes on pause and resets on
restart. Nothing in the rules ever reads it, and it draws from its own xorshift
generator rather than raylib's, so particles never shift the random stream the
gameplay uses. Same input, same run, effects on or off.

- **Star field**: three parallax layers drifting against the ship's velocity.
  Because the camera is fixed, the sense of motion comes from your own speed.
- **Thruster trail**: particles out of the ship's tail while thrusting, cooling
  from hot yellow into the tail colour.
- **Debris**: a destroyed enemy bursts into `6 + sides * 3` particles in its own
  colour, inheriting half its velocity. Losing a life bursts white.
- **Muzzle flash**: a glow on the nose plus a tight spray of sparks when firing.
  The flash is derived from the fire cooldown, so it needs no timer of its own.
- **Shield**: one ring that shrinks onto the hull, thins out and fades to nothing
  over the invulnerable window. Radius, thickness and alpha all run off the same
  remaining time, so there are no phases to jump between and no blinking. The
  alpha reaches zero exactly when the protection does, so the ship can look
  unprotected a moment early but never the other way round.
- **Vector glow**: every shape gets two soft additive halo passes with a crisp
  line on top, which is what gives it the arcade CRT look.

Particles wrap along with the world, so a trail is not cut off at the edge.

Tuning knobs sit together at the top of `src/fx.c`: `LAYER_PARALLAX` and
`LAYER_ALPHA` for depth, the `THRUST_*` and `DEBRIS_*` values for the particles,
and the four `GLOW_*` values for the halo. Setting `GLOW_WIDE_ALPHA` to zero
gives back plain lines.

## State

- Bullets with a cooldown and a lifetime, inheriting the ship's velocity
- Circle-circle collisions for bullet against enemy and ship against enemy
- Enemy types with a split chain from hexagon down to triangle
- 3 lives, game over when the last one is gone
- Dying takes you off the board for 0.9s, after which you materialise once the
  spawn point is clear, capped at 1.2s more so a busy middle cannot lock you
  out. While held up, a red hull pulses on the spawn point so the wait reads as
  the game holding rather than hanging.
- Score per type, waves growing to 8 large enemies

Crashing into an enemy costs a life but leaves the enemy intact: no score, no
split. Only bullets break things apart.

Firing kicks the ship back by 45 px/s. At one shot per 0.18s that is roughly
250 px/s2 against your heading, close to three quarters of the engine, so
shooting along your course doubles as a brake.

Star field, thruster trail, debris bursts and vector glow, all in `fx.c`.

Distance is measured the short way round the torus (`torus_offset`), so two
objects that visibly overlap across an edge actually touch. Everything that
measures goes through it: collisions, bullets, grazing, the respawn check and
wave spawning.

Not done yet: sound, screen shake and a high score.

## Licences

`assets/ArchivoBlack-Regular.ttf` is Archivo Black by Omnibus-Type, under the
SIL Open Font License 1.1. The licence text sits next to it in `assets/OFL.txt`.
