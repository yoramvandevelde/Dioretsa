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
- `src/fx.h` / `src/fx.c` - star field, particles and the glow helpers

The world is 1280x720 and wraps on every edge. `main.c` translates raw keys into
an `Input` struct, so `game.c` never has to know about key codes.

Enemies and bullets live in fixed-size pools; a slot is free as soon as
`base.alive` is off. That allows spawning in the middle of an update (an enemy
splitting apart) without the loop underneath shifting around.

## Enemy types

All balancing sits in `ENEMY_TYPES` at the top of `src/game.c`: side count,
radius, speed, colour, score, behaviour and what it splits into. A new type is
one line in the `EnemyTypeId` enum plus one row in that table, nothing else.

| Type     | Sides | Splits into  | Score |
| -------- | ----- | ------------ | ----- |
| hexagon  | 6     | 2x pentagon  | 20    |
| pentagon | 5     | 2x square    | 50    |
| square   | 4     | 2x triangle  | 100   |
| triangle | 3     | nothing      | 200   |

Shapes are regular polygons drawn with `DrawPolyLines`, so all sides are equal.
Behaviour lives in `enemy_behave()`: `DRIFT` (straight line), `SPINNER` (tumbles
faster) and `SEEK` (steers towards the ship, as the triangle does).

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
- **Vector glow**: every shape gets two soft additive halo passes with a crisp
  line on top, which is what gives it the arcade CRT look.

Tuning knobs sit together at the top of `src/fx.c`: `LAYER_PARALLAX` and
`LAYER_ALPHA` for depth, the `THRUST_*` and `DEBRIS_*` values for the particles,
and the four `GLOW_*` values for the halo. Setting `GLOW_WIDE_ALPHA` to zero
gives back plain lines.

## State

- Bullets with a cooldown and a lifetime, inheriting the ship's velocity
- Circle-circle collisions for bullet against enemy and ship against enemy
- Enemy types with a split chain from hexagon down to triangle
- 3 lives, respawn with 2 seconds of blinking invulnerability, game over
- Score per type, waves growing to 8 large enemies

Crashing into an enemy costs a life but leaves the enemy intact: no score, no
split. Only bullets break things apart.

Star field, thruster trail, debris bursts and vector glow, all in `fx.c`.

Not done yet: sound, impact particles, screen shake, high score, and collisions
that reach across the world edge.
