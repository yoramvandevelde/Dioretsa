#include "game.h"

#include <math.h>
#include <stddef.h>

#define SHIP_TURN_SPEED   4.5f      // radians per second
#define SHIP_THRUST       340.0f    // pixels per second^2
#define SHIP_DRAG         0.6f      // per second
#define SHIP_RADIUS       14.0f
#define SHIP_INVULN       2.0f      // seconds of invulnerability after respawn

#define BULLET_SPEED      520.0f
#define BULLET_LIFE       1.2f
#define BULLET_RADIUS     2.0f
#define FIRE_COOLDOWN     0.18f

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
static void wrap(Vector2 *p, float r)
{
    if (p->x < -r)          p->x = WORLD_W + r;
    if (p->x > WORLD_W + r) p->x = -r;
    if (p->y < -r)          p->y = WORLD_H + r;
    if (p->y > WORLD_H + r) p->y = -r;
}

static void entity_integrate(Entity *e, float dt)
{
    e->pos.x += e->vel.x * dt;
    e->pos.y += e->vel.y * dt;
    e->rot   += e->rotVel * dt;
    wrap(&e->pos, e->radius);
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
}

void game_init(Game *g)
{
    *g = (Game){ 0 };

    reset_ship(&g->ship);
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

    g->ship.fireCooldown = FIRE_COOLDOWN;
}

static void kill_ship(Game *g)
{
    g->lives--;
    if (g->lives <= 0) {
        g->lives           = 0;
        g->gameOver        = true;
        g->ship.base.alive = false;
        return;
    }
    reset_ship(&g->ship);
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

    if (s->invuln > 0.0f) {
        s->invuln -= dt;
        if (s->invuln < 0.0f) s->invuln = 0.0f;
    }
    if (s->fireCooldown > 0.0f) s->fireCooldown -= dt;

    if (in->fire && s->fireCooldown <= 0.0f) fire_bullet(g);

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
    DrawLineStrip(p, 4, color);
}

static void draw_ship(const Ship *s)
{
    // Blink while invulnerable.
    if (s->invuln > 0.0f && fmodf(s->invuln, 0.24f) < 0.12f) return;

    draw_ship_shape(s->base.pos, s->base.rot, 1.0f, RAYWHITE);

    if (s->thrusting) {
        Vector2 flame = rotate((Vector2){ -20.0f, 0.0f }, s->base.rot);
        Vector2 back1 = rotate((Vector2){ -11.0f,  10.0f }, s->base.rot);
        Vector2 back2 = rotate((Vector2){ -11.0f, -10.0f }, s->base.rot);
        Vector2 f[3] = {
            { s->base.pos.x + back1.x, s->base.pos.y + back1.y },
            { s->base.pos.x + flame.x, s->base.pos.y + flame.y },
            { s->base.pos.x + back2.x, s->base.pos.y + back2.y },
        };
        DrawLineStrip(f, 3, ORANGE);
    }
}

static void draw_enemy(const Enemy *e)
{
    const EnemyType *t = &ENEMY_TYPES[e->type];
    DrawPolyLines(e->base.pos, t->sides, e->base.radius, e->base.rot * RAD2DEG, t->color);
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
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (g->enemies[i].base.alive) draw_enemy(&g->enemies[i]);
    }
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (g->bullets[i].base.alive) DrawCircleV(g->bullets[i].base.pos, BULLET_RADIUS, RAYWHITE);
    }
    if (g->ship.base.alive) draw_ship(&g->ship);

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
