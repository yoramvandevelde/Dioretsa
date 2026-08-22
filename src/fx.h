#ifndef FX_H
#define FX_H

#include "raylib.h"

// Purely cosmetic state. Nothing in here may affect gameplay, so the whole
// module can be ripped out without touching a single rule.
#define MAX_PARTICLES   512
#define STAR_LAYERS     3
#define STARS_PER_LAYER 55

typedef struct {
    Vector2 pos;
    float   size;
    Color   color;
} Star;

typedef struct {
    Vector2 pos;
    Vector2 vel;
    float   life;       // seconds left
    float   maxLife;
    float   size;
    Color   color;      // colour at birth
    Color   fadeTo;     // colour it cools down to before vanishing
} Particle;

typedef struct {
    Star     stars[STAR_LAYERS * STARS_PER_LAYER];
    Particle particles[MAX_PARTICLES];
    unsigned int rng;   // effects roll their own dice, see fx.c
} Fx;

void fx_init(Fx *fx);

// referenceVel is what the stars drift against: pass the ship's velocity and
// the layers slide the other way, the near ones faster than the far ones.
void fx_update(Fx *fx, Vector2 referenceVel, float dt);

// Emits a puff out of the ship's tail. dir is the ship's facing direction.
void fx_emit_thrust(Fx *fx, Vector2 tail, Vector2 dir, Vector2 shipVel);

// Debris flying outwards from a destroyed object, in its own colour.
void fx_emit_burst(Fx *fx, Vector2 pos, Vector2 inheritVel, Color color,
                   int count, float speed);

void fx_draw_stars(const Fx *fx);       // behind everything
void fx_draw_particles(const Fx *fx);   // on top of the world, under the HUD

// Vector-arcade glow: a soft additive halo with a crisp bright core on top.
// These take plain geometry, so they work for any shape the game draws.
void fx_glow_poly(Vector2 center, int sides, float radius, float rotationDeg, Color color);
void fx_glow_strip(const Vector2 *points, int count, Color color);
void fx_glow_dot(Vector2 pos, float radius, Color color);

#endif // FX_H
