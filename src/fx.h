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

// The fonts are loaded once, after the window exists, and live in fx rather
// than in any Fx instance: they are GPU resources, not game state. Everything
// keeps working on the built-in font if the file is missing.
//
// The scale says how many pixels a world unit covers, so the glyphs are cut
// for the size they are actually drawn at: 1.0 on a 1280x720 display, 3.0 on
// a 4K one. There are two cuts of the one face because the banner and the HUD
// are an order of magnitude apart in size, and a single atlas cannot serve
// both without one of them blurring.
void fx_load_fonts(float scale);
void fx_unload_fonts(void);

Font fx_title_font(void);        // the banner: huge, a handful of words
Font fx_hud_font(void);          // the score, the wave, the report
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

// The same band, but only part of the way round, starting at the top. Reads as
// a countdown when the sweep runs down to nothing.
void fx_glow_arc(Vector2 pos, float radius, float thickness, float sweepDeg, Color color);

#endif // FX_H
