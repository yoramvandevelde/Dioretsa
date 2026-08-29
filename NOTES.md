# Notes

*** This file is used as a scratchpad for ideas and choices. Some might spoil things you could find out on your own ***

How Dioretsa is put together, and why. The README is for playing it; this is for
working on it.

## Building and bundling

```sh
brew install raylib cmake just
just            # lists every recipe
just run        # build and play
```

`just build` links against the raylib brew installed, which takes seconds and is
what you want while working. `just bundle-mac` does the opposite: it compiles
raylib from source, links it statically, builds for both architectures, and
lays out `dist/dioretsa-macos` with the executable and its assets, plus a zip of
it. That runs on a Mac that has never seen Homebrew, Intel included.

There is one assets directory, the one in the repository. Nothing is copied into
a build directory, because copies mean several files with the same name and only
one of them counting. The bundle copies from `git ls-files`, so scratch files
sitting in `assets/` never travel with a release.

At startup the executable tries, in order: `assets/` in the working directory,
`assets/` beside itself, one level up, and inside a macOS bundle. Nothing is
linked or baked in; whichever exists first wins.

Releases are cut by pushing a date tag, `YYYY.MM.DD`, with `-2` and up for a
second release on the same day. That fires `.github/workflows/release.yml`,
which builds macOS, Linux, Windows and Android, packages the first three with
the tracked assets, signs the APK, and publishes all four to a GitHub release.
Every pull request and every push to main builds the same four, the APK unsigned
because a check has no business holding the key, so a platform cannot quietly
rot.

Signing wants four repository secrets: `SIGNING_JKS_FILE_BASE64`, which is the
keystore itself (`base64 -i android/app/upload-keystore.jks`), plus
`SIGNING_KEYSTORE_PASSWORD`, `SIGNING_KEY_PASSWORD` and `SIGNING_KEY_ALIAS`.
Locally those three come from `.github/signing.env`, which is git-ignored;
`.github/signing.env.example` explains how to make the keystore and what to put
in it.

## Layout

- `src/main.c` - window, input and the game loop on a fixed timestep (60 Hz)
- `src/game.h` - data: `Entity`, `Ship`, `Bullet`, `Enemy`, `EnemyType`, `Game`, `Input`
- `src/game.c` - update, collisions and drawing
- `src/fx.h` / `src/fx.c` - star field, particles, glow helpers and the title font
- `assets/` - Archivo Black, used for every word on screen

The world is 1280x720 and wraps on every edge, exactly one world across.
Anything straddling an edge is drawn on the far side as well (`ghost_positions`),
corners included, so objects slide across instead of popping in and out, and
distances are measured the short way round (`torus_offset`) so what overlaps on
screen also overlaps in the maths. `main.c` translates raw keys into
an `Input` struct, so `game.c` never has to know about key codes.

Those 1280x720 are world units, not pixels. Every rule, distance and speed is
written in them and nothing in the simulation is allowed to ask how large the
display is, so the game plays identically on a laptop window and on a
television. Only the drawing scales: `world_view()` in `main.c` fits the world
into whatever the window turns out to be and hands `game_draw` a `Camera2D`,
which means the vectors are rasterised at the panel's own resolution rather
than drawn once at 720p and stretched. A display that is not 16:9 gets bars,
and a scissor keeps the glows and the wave banner from spilling into them. The
one thing that cannot follow along for free is text, which is baked into a
texture at load: `fx_load_fonts` takes the scale so the glyphs are cut for the
size they will be drawn at. There are two cuts of Archivo Black rather than
one, because the banner and the HUD are an order of magnitude apart and a font
minified by twenty times shimmers as it moves.

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

## Wave composition

`WAVES` near the top of `spawn_wave` holds one row per wave, and the last row
repeats forever. The mix walks from four hexagons and nothing else to five
pentagons and five squares, so the field turns from big and slow into small and
fast while the total number of kills stays roughly level: harder, rather than
more of the same. Nothing mutates along the way, so the enemies stay learnable.

Triangles are never spawned directly. Red is something you make by taking the
chain apart, which keeps the dance a choice.

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

## Pickups

A destroyed triangle drops one about one time in ten. Nothing else drops: the
reward sits on the hardest kill in the game, which is also the one you took a
risk for. It leaves with the velocity of whatever dropped it and bleeds that off over its
lifetime, coasting to a halt: a hunting triangle can be doing 225 px/s when it
dies, and a pickup carrying that speed is gone before you can turn around. As it
stands it covers about 560 px, well under half the world, so it stays reachable
while still pulling you back into the fight you just came out of. A draining arc
shows the five seconds you have to decide.

What it hands over is decided when you touch it, not when it falls, so losing a
life while it floats cannot leave you holding the wrong prize. Below three lives
it gives the life back; at three it pays 200 points instead. The colour follows
the same rule, green or gold, so you always see what it will do before you
commit to fetching it.

## The title screen

The game opens on its own field: stars panning slowly, enemies drifting, no ship
and no HUD, with ANY KEY... breathing in the middle. Any key at all starts a run,
and nothing else is read that frame, or the key you pressed would also pause the
game or mute the music on the way in. `--skip-menu` goes straight into wave 1.

None of the flags reach Android, where raylib calls `main()` with nothing but a
program name. The frame counter is the one that is missed there, now that the
panel decides how many pixels get drawn, so a debug build switches it on by
itself and a release never does.

## Quitting

Esc used to end the run on the spot, because raylib wires it to the exit key by
default. `SetExitKey(KEY_NULL)` hands it back, and the game asks instead: the
field holds where it is, dimmed, with QUIT? over it and Y or N as the answer.
Holding is the same early return as a pause, so a run comes back untouched when
the answer is no, and nothing else is read while the question is up: the key
that answers it cannot also fire a shot or restart the game.

The window close button still means what it says. Only Esc asks, because only
Esc sits next to the keys you are already using and ends a run by accident.

## The report

Dying opens a report rather than a dimmed field: four headline figures across
the top, the title across the middle, and four columns of detail underneath.
Every number is a counter on a line that already existed, so tracking them costs
the game nothing.

Kills and deaths are both broken down per enemy type, which is what makes them
worth reading side by side: two hundred triangles killed, and four deaths to
them. The dance column reports the best multiplier, the longest unbroken graze,
how much of the score came from grazing rather than shooting, and the closest
you came to something and lived.

Text is drawn at multiples of ten because the built-in font has a base size of
10. Anything else stretches its pixels unevenly and reads as slightly blurred,
which is easy to mistake for the font simply being wrong.

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

## Sound

`src/audio.c` holds an enum and a table, the same shape as the enemy table: a
new sound is one line in `SoundId` and one row naming the file and its volume.
Every sound gets four voices through `LoadSoundAlias`, which share the sample
data, so a chain reaction overlaps instead of cutting itself off. A missing file
logs a warning and stays silent; it is never fatal.

Enemies carry their own note in `ENEMY_TYPES`, pitched by size: a triangle pops
at 512 Hz and a hexagon thuds at 191 Hz, so you can hear what you hit without
looking. Shots vary by 6% at random, because a fixed sample five times a second
turns into a machine gun. The graze tick pitches up with the multiplier, so
threading four reds sounds different from brushing one grey.

Sounds are panned to where they happen, at 0.75 of the field rather than hard
left and right: an explosion still in view should not sound like it left the
screen. The engine pans too, but narrower and with a dip as it crosses a
wrapping edge, because its pan flips sides there and a flip you can hear is a
flip that sounds broken.

The volume column corrects for how loud each file was recorded, not for how
loud it should feel. The measured levels of the samples differ by more than
20 dB, so values above 1.0 are gain rather than a mistake, and each still has
peak headroom.

A soundscape runs from launch to exit, straight through pause and game over: it
is the room you are in, not a reaction to anything. M fades it in and out, so
comparing it against silence is not itself an event.

A Schroeder reverb sits on the final mix, four combs into two allpasses.
raylib has no send bus, so it is all or nothing: the delays are
long, so it reads as space rather than a room, and the wet level is deliberately
low. A loud one puts everything in the same room and flattens the difference
between near and far.

The engine is a music stream that never stops. Thrust moves a target and
`audio_update` ramps the volume towards it, up in 0.1s and down in 0.25s;
starting and stopping a sound on every tap clicks and rattles, a ramp does not.
Its pitch follows your speed against terminal velocity, and it sits well under
everything else because it plays constantly.

## Where it stands

- Bullets with a cooldown and a lifetime, inheriting the ship's velocity
- Circle-circle collisions for bullet against enemy and ship against enemy
- Enemy types with a split chain from hexagon down to triangle
- 3 lives, game over when the last one is gone
- Dying takes you off the board for 0.9s, after which you materialise once the
  spawn point is clear, capped at 1.2s more so a busy middle cannot lock you
  out. While held up, a red hull pulses on the spawn point so the wait reads as
  the game holding rather than hanging.
- Score per type, with wave composition hand-authored in a table

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
