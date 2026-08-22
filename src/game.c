#include "game.h"

#include <math.h>
#include <stddef.h>

#define SHIP_TURN_SPEED   4.5f      // radians per second
#define SHIP_THRUST       340.0f    // pixels per second^2
#define SHIP_DRAG         0.6f      // per second
#define SHIP_RADIUS       14.0f
#define SHIP_INVULN       2.0f      // seconds of invulnerability after respawn
#define PENDING_PULSE     6.0f      // rad/s of the waiting-to-respawn pulse
#define SHIELD_RADIUS     78.0f     // how far out the shield starts
#define SHIELD_THICKNESS  7.0f      // band width at full strength

#define RESPAWN_DELAY     0.9f      // pause before trying to come back at all
#define RESPAWN_MAX_WAIT  1.2f      // never hold the player hostage longer
#define RESPAWN_CLEAR     55.0f     // only block on something practically on top

#define BULLET_SPEED      520.0f
#define BULLET_LIFE       1.2f
#define BULLET_RADIUS     2.0f
#define FIRE_COOLDOWN     0.18f
#define FIRE_RECOIL       45.0f     // pixels per second, kicked back on firing
#define MUZZLE_FLASH_TIME 0.07f     // must stay below FIRE_COOLDOWN

#define SEEK_ACCEL        140.0f    // pixels per second^2 for BEHAVIOR_SEEK
#define SAFE_SPAWN_DIST   180.0f    // keep spawns clear of the ship

// All enemy balancing lives here. The split chain runs large to small:
// hexagon -> pentagon -> square -> triangle -> gone.
static const EnemyType ENEMY_TYPES[ENEMY_TYPE_COUNT] = {
    [ENEMY_HEXA]     = { "hexagon",  6, 60.0f, 40.0f,  70.0f, 0.5f, LIGHTGRAY,  20,
                         BEHAVIOR_DRIFT,   ENEMY_PENTA,    2 },
    [ENEMY_PENTA]    = { "pentagon", 5, 44.0f, 55.0f,  90.0f, 0.7f, SKYBLUE,    50,
                         BEHAVIOR_DRIFT,   ENEMY_SQUARE,   2 },
    [ENEMY_SQUARE]   = { "square",   4, 32.0f, 75.0f, 115.0f, 1.0f, GOLD,      100,
                         BEHAVIOR_SPINNER, ENEMY_TRIANGLE, 2 },
    [ENEMY_TRIANGLE] = { "triangle", 3, 20.0f, 95.0f, 150.0f, 1.4f, RED,       200,
                         BEHAVIOR_SEEK,   -1,             0 },
};

const EnemyType *enemy_type(EnemyTypeId id)
{
    return &ENEMY_TYPES[id];
}

static float frand(float lo, float hi)
{
    return lo + (hi - lo) * (float)GetRandomValue(0, 10000) / 10000.0f;
}

static Vector2 rotate(Vector2 v, float a)
{
    float s = sinf(a), c = cosf(a);
    return (Vector2){ v.x * c - v.y * s, v.x * s + v.y * c };
}

// The world is a torus: whatever leaves one edge comes back in on the other.
// Wrap exactly one world across, never a fraction more. Drawing hands out the
// mirrored copies (see ghost_positions), so an object straddling an edge is
// already visible on both sides and any overshoot here shows up as a jump.
static void wrap(Vector2 *p)
{
    if (p->x < 0.0f)     p->x += WORLD_W;
    if (p->x >= WORLD_W) p->x -= WORLD_W;
    if (p->y < 0.0f)     p->y += WORLD_H;
    if (p->y >= WORLD_H) p->y -= WORLD_H;
}

static void entity_integrate(Entity *e, float dt)
{
    e->pos.x += e->vel.x * dt;
    e->pos.y += e->vel.y * dt;
    e->rot   += e->rotVel * dt;
    wrap(&e->pos);
}

// Collisions are not tested across the world edge: two objects on opposite
// sides of it never touch. At these speeds that goes unnoticed.
static bool entity_hit(const Entity *a, const Entity *b)
{
    float dx = a->pos.x - b->pos.x;
    float dy = a->pos.y - b->pos.y;
    float r  = a->radius + b->radius;
    return (dx * dx + dy * dy) < (r * r);
}

// Works on any pool whose elements start with an Entity.
static int find_free_slot(const Entity *first, int count, size_t stride)
{
    for (int i = 0; i < count; i++) {
        const Entity *e = (const Entity *)((const char *)first + (size_t)i * stride);
        if (!e->alive) return i;
    }
    return -1;
}

static int enemies_alive(const Game *g)
{
    int n = 0;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (g->enemies[i].base.alive) n++;
    }
    return n;
}

static Enemy *spawn_enemy(Game *g, Vector2 pos, EnemyTypeId type)
{
    int slot = find_free_slot(&g->enemies[0].base, MAX_ENEMIES, sizeof(Enemy));
    if (slot < 0) return NULL;

    const EnemyType *t = &ENEMY_TYPES[type];
    Enemy *e = &g->enemies[slot];
    float spin = (t->behavior == BEHAVIOR_SPINNER) ? t->spin * 2.5f : t->spin;

    e->type        = type;
    e->hp          = 1;
    e->base.pos    = pos;
    e->base.vel    = rotate((Vector2){ frand(t->speedMin, t->speedMax), 0.0f },
                            frand(0.0f, 2.0f * PI));
    e->base.rot    = frand(0.0f, 2.0f * PI);
    e->base.rotVel = frand(-spin, spin);
    e->base.radius = t->radius;
    e->base.alive  = true;
    return e;
}

static void split_enemy(Game *g, const Enemy *parent)
{
    const EnemyType *t = &ENEMY_TYPES[parent->type];
    if (t->splitInto < 0 || t->splitCount <= 0) return;

    // The parent is already marked dead here, so spawn_enemy() may reuse its
    // slot and overwrite it. Copy what we need before spawning anything.
    EnemyTypeId childType = (EnemyTypeId)t->splitInto;
    int         count     = t->splitCount;
    Vector2     pos       = parent->base.pos;
    Vector2     vel       = parent->base.vel;

    for (int i = 0; i < count; i++) {
        Enemy *child = spawn_enemy(g, pos, childType);
        if (!child) return;

        // Inherit the parent's heading, with a sideways kick outwards.
        float kick = 0.6f - 1.2f * ((float)i / (float)(count > 1 ? count - 1 : 1));
        Vector2 v  = rotate(vel, kick);
        child->base.vel = (Vector2){ v.x * 1.25f, v.y * 1.25f };
    }
}

static Vector2 safe_spawn_pos(const Game *g)
{
    Vector2 p;
    do {
        p = (Vector2){ frand(0.0f, WORLD_W), frand(0.0f, WORLD_H) };
    } while (fabsf(p.x - g->ship.base.pos.x) < SAFE_SPAWN_DIST &&
             fabsf(p.y - g->ship.base.pos.y) < SAFE_SPAWN_DIST);
    return p;
}

static void spawn_wave(Game *g)
{
    int big = 3 + g->wave;
    if (big > 8) big = 8;
    for (int i = 0; i < big; i++) spawn_enemy(g, safe_spawn_pos(g), ENEMY_HEXA);

    // From wave 3 on, add loose small ones that did not come from a split.
    int extra = g->wave - 2;
    if (extra > 3) extra = 3;
    for (int i = 0; i < extra; i++) spawn_enemy(g, safe_spawn_pos(g), ENEMY_SQUARE);
}

static void reset_ship(Ship *s)
{
    s->base.pos     = (Vector2){ WORLD_W / 2.0f, WORLD_H / 2.0f };
    s->base.vel     = (Vector2){ 0.0f, 0.0f };
    s->base.rot     = -PI / 2.0f;   // nose up
    s->base.rotVel  = 0.0f;
    s->base.radius  = SHIP_RADIUS;
    s->base.alive   = true;
    s->thrusting    = false;
    s->invuln       = SHIP_INVULN;
    s->fireCooldown = 0.0f;
    s->respawnIn    = 0.0f;
}

void game_init(Game *g)
{
    *g = (Game){ 0 };

    reset_ship(&g->ship);
    fx_init(&g->fx);
    g->lives = START_LIVES;
    g->wave  = 1;
    spawn_wave(g);
}

static void fire_bullet(Game *g)
{
    int slot = find_free_slot(&g->bullets[0].base, MAX_BULLETS, sizeof(Bullet));
    if (slot < 0) return;

    const Ship *s = &g->ship;
    Vector2 dir   = rotate((Vector2){ 1.0f, 0.0f }, s->base.rot);
    Bullet *b     = &g->bullets[slot];

    b->base.pos    = (Vector2){ s->base.pos.x + dir.x * 18.0f,
                                s->base.pos.y + dir.y * 18.0f };
    b->base.vel    = (Vector2){ s->base.vel.x + dir.x * BULLET_SPEED,
                                s->base.vel.y + dir.y * BULLET_SPEED };
    b->base.rot    = s->base.rot;
    b->base.rotVel = 0.0f;
    b->base.radius = BULLET_RADIUS;
    b->base.alive  = true;
    b->life        = BULLET_LIFE;

    // Kick the ship back, then spit sparks out of the barrel.
    g->ship.base.vel.x -= dir.x * FIRE_RECOIL;
    g->ship.base.vel.y -= dir.y * FIRE_RECOIL;
    fx_emit_muzzle(&g->fx, b->base.pos, dir, s->base.vel);

    g->ship.fireCooldown = FIRE_COOLDOWN;
}

static void kill_ship(Game *g)
{
    fx_emit_burst(&g->fx, g->ship.base.pos, g->ship.base.vel, RAYWHITE, 28, 200.0f);

    g->lives--;
    if (g->lives <= 0) {
        g->lives           = 0;
        g->gameOver        = true;
        g->ship.base.alive = false;
        return;
    }

    // Stay gone for a moment; game_update decides when it is safe to return.
    g->ship.base.alive = false;
    g->ship.respawnIn  = RESPAWN_DELAY;
}

// Only whatever is practically sitting on the spawn point counts. You come
// back with a shield anyway, so the point is not to materialise inside
// something, not to wait for an empty screen.
static bool centre_is_clear(const Game *g)
{
    Vector2 c = { WORLD_W / 2.0f, WORLD_H / 2.0f };

    for (int i = 0; i < MAX_ENEMIES; i++) {
        const Enemy *e = &g->enemies[i];
        if (!e->base.alive) continue;

        float dx = c.x - e->base.pos.x, dy = c.y - e->base.pos.y;
        float r  = e->base.radius + RESPAWN_CLEAR;
        if (dx * dx + dy * dy >= r * r) continue;

        // Close, but already on its way out: it will not be here any more.
        if (e->base.vel.x * dx + e->base.vel.y * dy < 0.0f) continue;

        return false;
    }
    return true;
}

static void enemy_behave(Enemy *e, const Game *g, float dt)
{
    const EnemyType *t = &ENEMY_TYPES[e->type];

    switch (t->behavior) {
        case BEHAVIOR_DRIFT:
        case BEHAVIOR_SPINNER:
            break;                  // heading is fixed, just integrate

        case BEHAVIOR_SEEK: {
            Vector2 d = { g->ship.base.pos.x - e->base.pos.x,
                          g->ship.base.pos.y - e->base.pos.y };
            float len = sqrtf(d.x * d.x + d.y * d.y);
            if (len < 0.001f) break;

            e->base.vel.x += (d.x / len) * SEEK_ACCEL * dt;
            e->base.vel.y += (d.y / len) * SEEK_ACCEL * dt;

            // Do not let it accelerate forever.
            float max = t->speedMax * 1.5f;
            float sp  = sqrtf(e->base.vel.x * e->base.vel.x + e->base.vel.y * e->base.vel.y);
            if (sp > max) {
                e->base.vel.x *= max / sp;
                e->base.vel.y *= max / sp;
            }
        } break;
    }
}

void game_update(Game *g, const Input *in, float dt)
{
    if (g->paused || g->gameOver) return;

    Ship *s = &g->ship;

    if (s->base.alive) {
        s->base.rot += in->turn * SHIP_TURN_SPEED * dt;
        s->thrusting = in->thrust;

        if (in->thrust) {
            Vector2 dir = rotate((Vector2){ 1.0f, 0.0f }, s->base.rot);
            s->base.vel.x += dir.x * SHIP_THRUST * dt;
            s->base.vel.y += dir.y * SHIP_THRUST * dt;
        }

        float damp = 1.0f - SHIP_DRAG * dt;
        s->base.vel.x *= damp;
        s->base.vel.y *= damp;

        entity_integrate(&s->base, dt);

        // The trail comes out of the tail.
        if (s->thrusting) {
            Vector2 dir  = rotate((Vector2){ 1.0f, 0.0f }, s->base.rot);
            Vector2 tail = { s->base.pos.x - dir.x * 13.0f, s->base.pos.y - dir.y * 13.0f };
            fx_emit_thrust(&g->fx, tail, dir, s->base.vel);
        }

        if (s->invuln > 0.0f) {
            s->invuln -= dt;
            if (s->invuln < 0.0f) s->invuln = 0.0f;
        }
        if (s->fireCooldown > 0.0f) s->fireCooldown -= dt;

        if (in->fire && s->fireCooldown <= 0.0f) fire_bullet(g);
    } else {
        // Dead with lives left: wait out the pause, then hold until the centre
        // is clear. The hard cap keeps a crowded middle from locking you out.
        s->respawnIn -= dt;
        s->thrusting  = false;

        if (s->respawnIn <= 0.0f &&
            (centre_is_clear(g) || s->respawnIn <= -RESPAWN_MAX_WAIT)) {
            reset_ship(s);
        }
    }

    // Stars drift against the ship's motion, and stand still while it is gone.
    fx_update(&g->fx, s->base.alive ? s->base.vel : (Vector2){ 0.0f, 0.0f }, dt);

    for (int i = 0; i < MAX_BULLETS; i++) {
        Bullet *b = &g->bullets[i];
        if (!b->base.alive) continue;

        b->life -= dt;
        if (b->life <= 0.0f) { b->base.alive = false; continue; }
        entity_integrate(&b->base, dt);
    }

    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &g->enemies[i];
        if (!e->base.alive) continue;

        enemy_behave(e, g, dt);
        entity_integrate(&e->base, dt);
    }

    // Bullet against enemy. split_enemy() fills free slots that may sit later
    // in the array; those children join in this same frame, which is fine.
    for (int bi = 0; bi < MAX_BULLETS; bi++) {
        Bullet *b = &g->bullets[bi];
        if (!b->base.alive) continue;

        for (int ei = 0; ei < MAX_ENEMIES; ei++) {
            Enemy *e = &g->enemies[ei];
            if (!e->base.alive) continue;
            if (!entity_hit(&b->base, &e->base)) continue;

            b->base.alive = false;
            if (--e->hp > 0) break;

            e->base.alive = false;
            g->score += ENEMY_TYPES[e->type].score;

            const EnemyType *t = &ENEMY_TYPES[e->type];
            fx_emit_burst(&g->fx, e->base.pos, e->base.vel, t->color,
                          6 + t->sides * 3, 40.0f + e->base.radius * 2.2f);

            split_enemy(g, e);
            break;
        }
    }

    // Ship against enemy. The enemy survives the crash: no score, no split.
    // Only the ship pays, otherwise dying would reward you with points.
    if (s->base.alive && s->invuln <= 0.0f) {
        for (int i = 0; i < MAX_ENEMIES; i++) {
            Enemy *e = &g->enemies[i];
            if (!e->base.alive) continue;
            if (!entity_hit(&s->base, &e->base)) continue;

            kill_ship(g);
            break;
        }
    }

    if (enemies_alive(g) == 0) {
        g->wave++;
        spawn_wave(g);
    }
}

// An object near an edge has to be drawn on the far side as well, otherwise it
// pops in and out instead of sliding across. Corners need all four copies. The
// margin covers the glow halo, which reaches past the shape itself.
#define GHOST_MARGIN 10.0f

static int ghost_positions(Vector2 pos, float radius, Vector2 out[4])
{
    float r = radius + GHOST_MARGIN;
    float dx[2] = { 0.0f, 0.0f }, dy[2] = { 0.0f, 0.0f };
    int   nx = 1, ny = 1, n = 0;

    if      (pos.x < r)             dx[nx++] =  WORLD_W;
    else if (pos.x > WORLD_W - r)   dx[nx++] = -WORLD_W;
    if      (pos.y < r)             dy[ny++] =  WORLD_H;
    else if (pos.y > WORLD_H - r)   dy[ny++] = -WORLD_H;

    for (int i = 0; i < nx; i++) {
        for (int j = 0; j < ny; j++) {
            out[n++] = (Vector2){ pos.x + dx[i], pos.y + dy[j] };
        }
    }
    return n;
}

static void draw_ship_shape(Vector2 pos, float rot, float scale, Color color)
{
    const Vector2 local[3] = {
        {  16.0f,   0.0f },
        { -11.0f,  10.0f },
        { -11.0f, -10.0f },
    };

    Vector2 p[4];
    for (int i = 0; i < 3; i++) {
        Vector2 r = rotate((Vector2){ local[i].x * scale, local[i].y * scale }, rot);
        p[i] = (Vector2){ pos.x + r.x, pos.y + r.y };
    }
    p[3] = p[0];
    fx_glow_strip(p, 4, color);
}

static void draw_ship_at(const Ship *s, Vector2 pos)
{
    // One continuous shield: it shrinks onto the hull, thins out and fades to
    // nothing over the invulnerable window. No phases, no blinking, so the
    // remaining time is readable at a glance.
    if (s->invuln > 0.0f) {
        float k = s->invuln / SHIP_INVULN;       // 1 on respawn, 0 when it lapses
        fx_glow_ring(pos,
                     SHIP_RADIUS + 6.0f + SHIELD_RADIUS * k,
                     1.2f + SHIELD_THICKNESS * k,
                     Fade(SKYBLUE, k));
    }

    draw_ship_shape(pos, s->base.rot, 1.0f, RAYWHITE);

    // Flash while the cooldown is still fresh: no extra timer needed.
    float flash = (s->fireCooldown - (FIRE_COOLDOWN - MUZZLE_FLASH_TIME)) / MUZZLE_FLASH_TIME;
    if (flash > 0.0f) {
        Vector2 dir  = rotate((Vector2){ 1.0f, 0.0f }, s->base.rot);
        Vector2 nose = { pos.x + dir.x * 17.0f, pos.y + dir.y * 17.0f };
        fx_glow_dot(nose, 1.5f + 4.5f * flash, Fade((Color){ 255, 245, 200, 255 }, flash));
    }

    if (s->thrusting) {
        Vector2 flame = rotate((Vector2){ -20.0f, 0.0f }, s->base.rot);
        Vector2 back1 = rotate((Vector2){ -11.0f,  10.0f }, s->base.rot);
        Vector2 back2 = rotate((Vector2){ -11.0f, -10.0f }, s->base.rot);
        Vector2 f[3] = {
            { pos.x + back1.x, pos.y + back1.y },
            { pos.x + flame.x, pos.y + flame.y },
            { pos.x + back2.x, pos.y + back2.y },
        };
        fx_glow_strip(f, 3, ORANGE);
    }
}

// Gone, but coming back: pulse a red hull on the spawn point. Without it the
// wait for a clear centre reads as the game hanging instead of holding.
static void draw_ship_pending(const Ship *s)
{
    Vector2 spawn = { WORLD_W / 2.0f, WORLD_H / 2.0f };
    float   pulse = 0.5f + 0.5f * sinf(s->respawnIn * PENDING_PULSE);

    draw_ship_shape(spawn, -PI / 2.0f, 1.0f, Fade(RED, 0.06f + 0.84f * pulse));
}

static void draw_ship(const Ship *s)
{

    Vector2 at[4];
    int n = ghost_positions(s->base.pos, s->base.radius, at);
    for (int i = 0; i < n; i++) draw_ship_at(s, at[i]);
}

static void draw_enemy(const Enemy *e)
{
    const EnemyType *t = &ENEMY_TYPES[e->type];

    Vector2 at[4];
    int n = ghost_positions(e->base.pos, e->base.radius, at);
    for (int i = 0; i < n; i++) {
        fx_glow_poly(at[i], t->sides, e->base.radius, e->base.rot * RAD2DEG, t->color);
    }
}

static void draw_hud(const Game *g)
{
    DrawText(TextFormat("%06i", g->score), 20, 18, 28, RAYWHITE);

    const char *wave = TextFormat("WAVE %i", g->wave);
    DrawText(wave, WORLD_W / 2 - MeasureText(wave, 20) / 2, 22, 20, GRAY);

    for (int i = 0; i < g->lives; i++) {
        Vector2 p = { (float)(WORLD_W - 34 - i * 30), 34.0f };
        draw_ship_shape(p, -PI / 2.0f, 0.9f, RAYWHITE);
    }
}

void game_draw(const Game *g)
{
    fx_draw_stars(&g->fx);

    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (g->enemies[i].base.alive) draw_enemy(&g->enemies[i]);
    }

    fx_draw_particles(&g->fx);      // behind the ship, on top of the world
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!g->bullets[i].base.alive) continue;

        Vector2 at[4];
        int n = ghost_positions(g->bullets[i].base.pos, BULLET_RADIUS, at);
        for (int k = 0; k < n; k++) fx_glow_dot(at[k], BULLET_RADIUS, RAYWHITE);
    }
    if (g->ship.base.alive)   draw_ship(&g->ship);
    // Only once the pause is over and something is actually in the way.
    else if (!g->gameOver && g->ship.respawnIn <= 0.0f) draw_ship_pending(&g->ship);

    draw_hud(g);

    if (g->paused) {
        DrawText("PAUSED", WORLD_W / 2 - MeasureText("PAUSED", 32) / 2, WORLD_H / 2 - 16, 32, RAYWHITE);
    }
    if (g->gameOver) {
        DrawText("GAME OVER", WORLD_W / 2 - MeasureText("GAME OVER", 48) / 2, WORLD_H / 2 - 40, 48, RAYWHITE);
        const char *hint = "press R";
        DrawText(hint, WORLD_W / 2 - MeasureText(hint, 20) / 2, WORLD_H / 2 + 20, 20, GRAY);
    }
}
