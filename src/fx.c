#include "fx.h"
#include "assets.h"
#include "game.h"

#include <math.h>
#include <stddef.h>

// How much of the reference velocity each star layer picks up. Far layers
// barely move, the near layer slides noticeably.
static const float LAYER_PARALLAX[STAR_LAYERS] = { 0.05f, 0.14f, 0.7f };
static const float LAYER_SIZE[STAR_LAYERS]     = { 0.1f,  1.2f,  2.1f  };
static const unsigned char LAYER_ALPHA[STAR_LAYERS] = { 70, 120, 185 };

#define THRUST_PER_STEP   3         // particles emitted per fixed update
#define THRUST_SPEED_MIN  70.0f
#define THRUST_SPEED_MAX  150.0f
#define THRUST_SPREAD     0.45f     // radians of cone around straight back
#define THRUST_LIFE_MIN   0.25f
#define THRUST_LIFE_MAX   0.60f

#define MUZZLE_SPARKS     6
#define MUZZLE_SPREAD     0.30f     // radians, tighter than the thruster cone
#define MUZZLE_LIFE_MIN   0.05f
#define MUZZLE_LIFE_MAX   0.14f

#define POP_LIFE          0.85f
#define POP_RISE          46.0f     // pixels per second the number drifts up

#define DEBRIS_LIFE_MIN   0.35f
#define DEBRIS_LIFE_MAX   0.90f

// Glow passes: width in pixels and how much of the colour each one keeps.
#define GLOW_WIDE         7.0f
#define GLOW_WIDE_ALPHA   0.10f
#define GLOW_TIGHT        3.5f
#define GLOW_TIGHT_ALPHA  0.20f

// Effects run on their own xorshift instead of raylib's GetRandomValue, so
// spawning particles never shifts the random stream the gameplay draws from.
// Same input, same run, effects on or off.
#define TITLE_FONT_SIZE   220       // atlas size at 1x; the banner scales down from here
// The atlas grows with the display, but only so far. The largest thing the
// banner ever sets is around 250 units, so twice the base covers a 4K set with
// room to spare, and a full 3x atlas would be a six thousand pixel texture
// carrying detail nothing on screen is large enough to show.
#define TITLE_FONT_MAX    (TITLE_FONT_SIZE * 2)

// The HUD gets its own cut of the same face. Setting twenty pixel labels out of
// the title atlas would mean shrinking a 440 pixel glyph by twenty times, and
// a font minified that far turns to mush and shimmers as it moves. This is the
// largest size the HUD sets, so everything smaller scales down a little rather
// than a lot.
#define HUD_FONT_SIZE     40

static Font  titleFont     = { 0 };
static bool  titleLoaded   = false;
static Font  hudFont       = { 0 };
static bool  hudLoaded     = false;

// Rasterises the face at the size it will actually be drawn at. Returns an
// unloaded font if the file is not there, which the getters read as "fall back".
static bool load_face(Font *out, int size)
{
    const char *path = asset_path("ArchivoBlack-Regular.ttf");
    if (!path) return false;

    *out = LoadFontEx(path, size, NULL, 0);
    if (out->texture.id == 0) return false;

    SetTextureFilter(out->texture, TEXTURE_FILTER_BILINEAR);
    return true;
}

void fx_load_fonts(float scale)
{
    if (scale < 1.0f) scale = 1.0f;

    int title = (int)(TITLE_FONT_SIZE * scale);
    if (title > TITLE_FONT_MAX) title = TITLE_FONT_MAX;

    titleLoaded = load_face(&titleFont, title);
    hudLoaded   = load_face(&hudFont, (int)(HUD_FONT_SIZE * scale));

    if (!titleLoaded || !hudLoaded)
        TraceLog(LOG_WARNING, "title font not found, falling back to the built-in one");
}

void fx_unload_fonts(void)
{
    if (titleLoaded) { UnloadFont(titleFont); titleLoaded = false; }
    if (hudLoaded)   { UnloadFont(hudFont);   hudLoaded   = false; }
}

Font fx_title_font(void)
{
    return titleLoaded ? titleFont : GetFontDefault();
}

Font fx_hud_font(void)
{
    return hudLoaded ? hudFont : GetFontDefault();
}

// The built-in font carries its own padding, so it needs far looser spacing
// than a real typeface does.
float fx_title_tracking(void)
{
    return titleLoaded ? 42.0f : 16.0f;
}

static float frand(Fx *fx, float lo, float hi)
{
    fx->rng ^= fx->rng << 13;
    fx->rng ^= fx->rng >> 17;
    fx->rng ^= fx->rng << 5;
    return lo + (hi - lo) * (float)(fx->rng >> 8) / (float)(1u << 24);
}

static Vector2 rotate(Vector2 v, float a)
{
    float s = sinf(a), c = cosf(a);
    return (Vector2){ v.x * c - v.y * s, v.x * s + v.y * c };
}

// Stars and particles live in the same wrapping world as the ship does.
static void wrap_world(Vector2 *p)
{
    if (p->x < 0.0f)    p->x += WORLD_W;
    if (p->x > WORLD_W) p->x -= WORLD_W;
    if (p->y < 0.0f)    p->y += WORLD_H;
    if (p->y > WORLD_H) p->y -= WORLD_H;
}

void fx_init(Fx *fx)
{
    *fx = (Fx){ 0 };
    fx->rng = 2463534242u;      // any non-zero seed will do

    for (int layer = 0; layer < STAR_LAYERS; layer++) {
        for (int i = 0; i < STARS_PER_LAYER; i++) {
            Star *s = &fx->stars[layer * STARS_PER_LAYER + i];
            s->pos  = (Vector2){ frand(fx, 0.0f, WORLD_W), frand(fx, 0.0f, WORLD_H) };
            s->size = LAYER_SIZE[layer] * frand(fx, 0.75f, 1.25f);

            // A few stars lean blue or amber so the field is not flat grey.
            unsigned char a = LAYER_ALPHA[layer];
            float tint = frand(fx, 0.0f, 1.0f);
            if      (tint < 0.15f) s->color = (Color){ 170, 200, 255, a };
            else if (tint < 0.25f) s->color = (Color){ 255, 220, 170, a };
            else                   s->color = (Color){ 255, 255, 255, a };
        }
    }
}

void fx_update(Fx *fx, Vector2 referenceVel, float dt)
{
    for (int layer = 0; layer < STAR_LAYERS; layer++) {
        float f = LAYER_PARALLAX[layer];
        for (int i = 0; i < STARS_PER_LAYER; i++) {
            Star *s = &fx->stars[layer * STARS_PER_LAYER + i];
            s->pos.x -= referenceVel.x * f * dt;
            s->pos.y -= referenceVel.y * f * dt;
            wrap_world(&s->pos);
        }
    }

    for (int i = 0; i < MAX_SCORE_POPS; i++) {
        ScorePop *sp = &fx->pops[i];
        if (sp->life <= 0.0f) continue;

        sp->life  -= dt;
        sp->pos.y -= POP_RISE * dt;
    }

    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle *p = &fx->particles[i];
        if (p->life <= 0.0f) continue;

        p->life -= dt;
        p->pos.x += p->vel.x * dt;
        p->pos.y += p->vel.y * dt;

        // Slow down as they cool off.
        float damp = 1.0f - 1.8f * dt;
        p->vel.x *= damp;
        p->vel.y *= damp;

        wrap_world(&p->pos);    // a trail must not get cut off at the edge
    }
}

static Particle *free_particle(Fx *fx)
{
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (fx->particles[i].life <= 0.0f) return &fx->particles[i];
    }
    return NULL;    // pool full: drop the particle, it is only decoration
}

void fx_emit_thrust(Fx *fx, Vector2 tail, Vector2 dir, Vector2 shipVel)
{
    for (int i = 0; i < THRUST_PER_STEP; i++) {
        Particle *p = free_particle(fx);
        if (!p) return;

        Vector2 back = rotate((Vector2){ -dir.x, -dir.y }, frand(fx, -THRUST_SPREAD, THRUST_SPREAD));
        float   sp   = frand(fx, THRUST_SPEED_MIN, THRUST_SPEED_MAX);

        p->pos     = (Vector2){ tail.x + frand(fx, -2.0f, 2.0f), tail.y + frand(fx, -2.0f, 2.0f) };
        p->vel     = (Vector2){ shipVel.x * 0.35f + back.x * sp,
                                shipVel.y * 0.35f + back.y * sp };
        p->maxLife = frand(fx, THRUST_LIFE_MIN, THRUST_LIFE_MAX);
        p->life    = p->maxLife;
        p->size    = frand(fx, 1.6f, 3.2f);
        p->color   = (Color){ 255, 225, 140, 255 };     // hot at the nozzle
        p->fadeTo  = (Color){ 0, 245,  0,   255 };     // cooled down at the tail
    }
}

void fx_emit_muzzle(Fx *fx, Vector2 nose, Vector2 dir, Vector2 shipVel)
{
    for (int i = 0; i < MUZZLE_SPARKS; i++) {
        Particle *p = free_particle(fx);
        if (!p) return;

        Vector2 out = rotate(dir, frand(fx, -MUZZLE_SPREAD, MUZZLE_SPREAD));
        float   sp  = frand(fx, 130.0f, 280.0f);

        p->pos     = nose;
        p->vel     = (Vector2){ shipVel.x + out.x * sp, shipVel.y + out.y * sp };
        p->maxLife = frand(fx, MUZZLE_LIFE_MIN, MUZZLE_LIFE_MAX);
        p->life    = p->maxLife;
        p->size    = frand(fx, 1.2f, 2.4f);
        p->color   = (Color){ 255, 255, 225, 255 };
        p->fadeTo  = (Color){ 255, 140, 40,  255 };
    }
}

static Color dim(Color c, float f)
{
    return (Color){ (unsigned char)(c.r * f), (unsigned char)(c.g * f),
                    (unsigned char)(c.b * f), c.a };
}

void fx_emit_burst(Fx *fx, Vector2 pos, Vector2 inheritVel, Color color,
                   int count, float speed)
{
    for (int i = 0; i < count; i++) {
        Particle *p = free_particle(fx);
        if (!p) return;

        // Spread evenly around the circle, then jitter so it does not look
        // like a clock face.
        float ang = (2.0f * PI * (float)i) / (float)count + frand(fx, -0.35f, 0.35f);
        float sp  = speed * frand(fx, 0.35f, 1.0f);

        p->pos     = (Vector2){ pos.x + cosf(ang) * frand(fx, 0.0f, 6.0f),
                                pos.y + sinf(ang) * frand(fx, 0.0f, 6.0f) };
        p->vel     = (Vector2){ inheritVel.x * 0.5f + cosf(ang) * sp,
                                inheritVel.y * 0.5f + sinf(ang) * sp };
        p->maxLife = frand(fx, DEBRIS_LIFE_MIN, DEBRIS_LIFE_MAX);
        p->life    = p->maxLife;
        p->size    = frand(fx, 1.4f, 2.8f);
        p->color   = color;
        p->fadeTo  = dim(color, 0.35f);
    }
}

void fx_emit_score(Fx *fx, Vector2 pos, int value)
{
    // Reuse the shortest-lived slot when they all sit busy, so the newest
    // number always gets shown.
    ScorePop *sp = &fx->pops[0];
    for (int i = 1; i < MAX_SCORE_POPS; i++) {
        if (fx->pops[i].life < sp->life) sp = &fx->pops[i];
    }

    sp->pos     = (Vector2){ pos.x, pos.y - 24.0f };
    sp->maxLife = POP_LIFE;
    sp->life    = POP_LIFE;
    sp->value   = value;
}

void fx_draw_scores(const Fx *fx)
{
    for (int i = 0; i < MAX_SCORE_POPS; i++) {
        const ScorePop *sp = &fx->pops[i];
        if (sp->life <= 0.0f) continue;

        // Bigger rewards land bigger, and everything fades as it rises.
        float t    = sp->life / sp->maxLife;
        int   size = (sp->value >= 200) ? 40 : (sp->value >= 60) ? 30 : 20;

        const char *txt = TextFormat("+%i", sp->value);
        DrawText(txt, (int)sp->pos.x - MeasureText(txt, size) / 2, (int)sp->pos.y,
                 size, Fade(GOLD, t));
    }
}

// raylib gives every circle 36 segments and every ring whatever it is asked
// for, which was 64 here. That is the right budget for a shape the size of a
// face and wildly wrong for a star two pixels across. The cost is not the
// triangles, it is the room they take: everything drawn between two state
// changes is batched into one vertex buffer, and on OpenGL ES 2 that buffer
// holds 2048 quads against the desktop's 8192. At 18 quads a circle the star
// field on its own overran it every frame, and an overrun is an upload and a
// draw call in the middle of the picture.
//
// So the count comes from the radius, and stops where a silhouette is round
// enough that more segments change nothing anyone can see.
static int arc_segments(float radius, float sweepDeg)
{
    int n = 6 + (int)(radius * 0.5f);
    if (n > 20) n = 20;

    n = (int)(n * sweepDeg / 360.0f);
    return (n < 4) ? 4 : n;
}

static void dot(Vector2 pos, float radius, Color color)
{
    DrawCircleSector(pos, radius, 0.0f, 360.0f, arc_segments(radius, 360.0f), color);
}

void fx_draw_stars(const Fx *fx)
{
    for (int i = 0; i < STAR_LAYERS * STARS_PER_LAYER; i++) {
        const Star *s = &fx->stars[i];
        dot(s->pos, s->size, s->color);
    }
}

void fx_draw_particles(const Fx *fx)
{
    for (int i = 0; i < MAX_PARTICLES; i++) {
        const Particle *p = &fx->particles[i];
        if (p->life <= 0.0f) continue;

        // Slide from the birth colour to the cooled one, fading out as it goes.
        float t = p->life / p->maxLife;
        Color c = {
            (unsigned char)(p->fadeTo.r + (p->color.r - p->fadeTo.r) * t),
            (unsigned char)(p->fadeTo.g + (p->color.g - p->fadeTo.g) * t),
            (unsigned char)(p->fadeTo.b + (p->color.b - p->fadeTo.b) * t),
            (unsigned char)(255.0f * t)
        };
        dot(p->pos, p->size * (0.45f + 0.55f * t), c);
    }
}

// Which half of a glow the calls below draw, see fx.h. Both by default, so a
// caller that has nothing to group gets whole objects without asking.
static int glowPass = FX_GLOW_BOTH;

void fx_glow_pass(int pass)
{
    if (pass == glowPass) return;
    glowPass = pass;

    // Whole objects switch to additive and back around their own haloes, so
    // outside a grouped pass the mode must be left where it was found.
    if (pass == FX_GLOW_HALO) BeginBlendMode(BLEND_ADDITIVE);
    else                      EndBlendMode();
}

static bool draws_halo(void) { return glowPass != FX_GLOW_LINE; }
static bool draws_line(void) { return glowPass != FX_GLOW_HALO; }

// No-ops inside a grouped pass, where the mode is already set and holding it
// is the entire point.
static void halo_begin(void) { if (glowPass == FX_GLOW_BOTH) BeginBlendMode(BLEND_ADDITIVE); }
static void halo_end(void)   { if (glowPass == FX_GLOW_BOTH) EndBlendMode(); }

// Two soft additive passes underneath, then the crisp line on top. Additive on
// a black background is what gives it the CRT bloom look.
void fx_glow_poly(Vector2 center, int sides, float radius, float rotationDeg, Color color)
{
    float a = color.a / 255.0f;     // a fading shape must fade its halo too

    if (draws_halo()) {
        halo_begin();
            DrawPolyLinesEx(center, sides, radius, rotationDeg, GLOW_WIDE,
                            Fade(color, GLOW_WIDE_ALPHA * a));
            DrawPolyLinesEx(center, sides, radius, rotationDeg, GLOW_TIGHT,
                            Fade(color, GLOW_TIGHT_ALPHA * a));
        halo_end();
    }

    if (draws_line()) DrawPolyLines(center, sides, radius, rotationDeg, color);
}

void fx_glow_strip(const Vector2 *points, int count, Color color)
{
    if (count < 2) return;

    float a = color.a / 255.0f;     // a fading shape must fade its halo too

    if (draws_halo()) {
        halo_begin();
            for (int pass = 0; pass < 2; pass++) {
                float width = (pass == 0) ? GLOW_WIDE : GLOW_TIGHT;
                float alpha = ((pass == 0) ? GLOW_WIDE_ALPHA : GLOW_TIGHT_ALPHA) * a;
                for (int i = 0; i < count - 1; i++) {
                    DrawLineEx(points[i], points[i + 1], width, Fade(color, alpha));
                }
            }
        halo_end();
    }

    if (draws_line()) DrawLineStrip((Vector2 *)points, count, color);
}

void fx_glow_dot(Vector2 pos, float radius, Color color)
{
    float a = color.a / 255.0f;     // a fading shape must fade its halo too

    if (draws_halo()) {
        halo_begin();
            dot(pos, radius * 3.5f, Fade(color, GLOW_WIDE_ALPHA * a));
            dot(pos, radius * 2.0f, Fade(color, GLOW_TIGHT_ALPHA * a));
        halo_end();
    }

    if (draws_line()) dot(pos, radius, color);
}

void fx_glow_arc(Vector2 pos, float radius, float thickness, float sweepDeg, Color color)
{
    float a    = color.a / 255.0f;
    float half = thickness * 0.5f;
    if (half < 0.4f) half = 0.4f;

    // Start at the top and sweep clockwise, the way a clock empties.
    float from = -90.0f;
    float to   = from + sweepDeg;

    int seg = arc_segments(radius, sweepDeg);

    if (draws_halo()) {
        halo_begin();
            DrawRing(pos, radius - half * 3.0f, radius + half * 3.0f, from, to, seg,
                     Fade(color, GLOW_WIDE_ALPHA * a));
        halo_end();
    }

    if (draws_line()) DrawRing(pos, radius - half, radius + half, from, to, seg, color);
}

void fx_glow_ring(Vector2 pos, float radius, float thickness, Color color)
{
    fx_glow_arc(pos, radius, thickness, 360.0f, color);
}
