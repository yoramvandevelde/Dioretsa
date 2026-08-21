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

## State

- Bullets with a cooldown and a lifetime, inheriting the ship's velocity
- Circle-circle collisions for bullet against enemy and ship against enemy
- Enemy types with a split chain from hexagon down to triangle
- 3 lives, respawn with 2 seconds of blinking invulnerability, game over
- Score per type, waves growing to 8 large enemies

Crashing into an enemy costs a life but leaves the enemy intact: no score, no
split. Only bullets break things apart.

Not done yet: sound, impact particles, screen shake, high score, and collisions
that reach across the world edge.
