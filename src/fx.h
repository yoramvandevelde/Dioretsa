#ifndef FX_H
#define FX_H

#include "raylib.h"

// Purely cosmetic state. Nothing in here may affect gameplay, so the whole
// module can be ripped out without touching a single rule.
#define MAX_PARTICLES   512
#define MAX_SCORE_POPS  8
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

// A number floating up off the ship, so a reward is visible where it is earned.
typedef struct {
    Vector2 pos;
    float   life;
    float   maxLife;
    int     value;
} ScorePop;

typedef struct {
    Star     stars[STAR_LAYERS * STARS_PER_LAYER];
    ScorePop pops[MAX_SCORE_POPS];
    Particle particles[MAX_PARTICLES];
    unsigned int rng;   // effects roll their own dice, see fx.c
} Fx;

// The title font is loaded once, after the window exists, and lives in fx
// rather than in any Fx instance: it is a GPU resource, not game state.
// Everything keeps working on the built-in font if the file is missing.
void fx_load_title_font(void);
void fx_unload_title_font(void);
Font fx_title_font(void);
float fx_title_tracking(void);   // font size over this is the letter spacing

void fx_init(Fx *fx);

// referenceVel is what the stars drift against: pass the ship's velocity and
// the layers slide the other way, the near ones faster than the far ones.
void fx_update(Fx *fx, Vector2 referenceVel, float dt);

// Emits a puff out of the ship's tail. dir is the ship's facing direction.
void fx_emit_thrust(Fx *fx, Vector2 tail, Vector2 dir, Vector2 shipVel);

// A short spray of sparks out of the gun barrel.
void fx_emit_muzzle(Fx *fx, Vector2 nose, Vector2 dir, Vector2 shipVel);

// Debris flying outwards from a destroyed object, in its own colour.
void fx_emit_burst(Fx *fx, Vector2 pos, Vector2 inheritVel, Color color,
                   int count, float speed);

void fx_emit_score(Fx *fx, Vector2 pos, int value);

void fx_draw_stars(const Fx *fx);       // behind everything
void fx_draw_particles(const Fx *fx);   // on top of the world, under the HUD
void fx_draw_scores(const Fx *fx);      // above the particles

// Vector-arcade glow: a soft additive halo with a crisp bright core on top.
// These take plain geometry, so they work for any shape the game draws.
void fx_glow_poly(Vector2 center, int sides, float radius, float rotationDeg, Color color);
void fx_glow_strip(const Vector2 *points, int count, Color color);
void fx_glow_dot(Vector2 pos, float radius, Color color);

// A band of the given thickness centred on radius, halo included.
void fx_glow_ring(Vector2 pos, float radius, float thickness, Color color);

#endif // FX_H
