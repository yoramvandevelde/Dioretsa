#ifndef GAME_H
#define GAME_H

#include "raylib.h"

#include "fx.h"

#define WORLD_W 1280
#define WORLD_H 720

// Headroom for the full split chain: 8 hexagons breaking all the way down to
// triangles is 64 enemies, plus the extra squares later waves throw in.
#define MAX_ENEMIES     128
#define MAX_BULLETS     32
#define MAX_PICKUPS     4
#define START_LIVES     3

// Everything drifting through the world shares this base. It sits first in
// every struct so find_free_slot() can walk any pool.
typedef struct {
    Vector2 pos;
    Vector2 vel;
    float   rot;        // radians
    float   rotVel;
    float   radius;
    bool    alive;
} Entity;

typedef struct {
    Entity base;
    bool   thrusting;
    float  invuln;          // seconds of invulnerability left after respawn
    float  fireCooldown;
    float  respawnIn;       // counts down while dead, then waits for a clear centre
    float  grazeTimer;      // time spent inside a graze band, reset on leaving
} Ship;

typedef struct {
    Entity base;
    float  life;            // seconds until it expires
} Bullet;

// Adding an enemy type: one line here, one row in ENEMY_TYPES. Nothing else.
typedef enum {
    ENEMY_TRIANGLE,
    ENEMY_SQUARE,
    ENEMY_PENTA,
    ENEMY_HEXA,
    ENEMY_TYPE_COUNT
} EnemyTypeId;

typedef enum {
    BEHAVIOR_DRIFT,         // straight line, classic asteroid
    BEHAVIOR_SPINNER,       // same, but tumbles a lot faster
    BEHAVIOR_SEEK           // steers towards the ship
} Behavior;

typedef struct {
    const char *name;
    int         sides;              // regular polygon, so all sides are equal
    float       radius;
    float       speedMin, speedMax;
    float       spin;               // rad/s, sign randomised on spawn
    Color       color;
    int         score;
    int         graze;              // payout per graze tick, steeply per type
    Behavior    behavior;
    int         splitInto;          // EnemyTypeId, or -1 for no remains
    int         splitCount;
} EnemyType;

// Dropped by a dying triangle. What it hands over is decided when you touch
// it, not when it falls: a life if you are down one, points if you are not.
typedef struct {
    Entity  base;
    Vector2 drift;          // speed it was dropped with, bled off over its life
    float   life;           // seconds before it fades away
} Pickup;

typedef struct {
    Entity      base;
    EnemyTypeId type;
    int         hp;                 // room for tougher types later
} Enemy;

typedef struct {
    Ship     ship;
    Enemy    enemies[MAX_ENEMIES];  // slots: use base.alive, not a count
    Bullet   bullets[MAX_BULLETS];
    Pickup   pickups[MAX_PICKUPS];
    int      score;
    int      lives;
    int      wave;
    float    banner;                // counts down while the wave title is up
    float    waveDelay;             // breather before the next wave spawns
    bool     paused;
    bool     gameOver;
    Fx       fx;                    // cosmetic only, never read by the rules
} Game;

// Input sampled once per frame, keeping the game logic free of raylib.
typedef struct {
    float turn;     // -1 left, +1 right
    bool  thrust;
    bool  fire;     // edge: consumed by exactly one update step
} Input;

const EnemyType *enemy_type(EnemyTypeId id);

// Debug switch for testing later waves without grinding through the early ones.
// It lives outside Game on purpose, so restarting with R keeps it on.
void game_set_god_mode(bool on);
bool game_god_mode(void);

void game_init(Game *g);
void game_update(Game *g, const Input *in, float dt);   // fixed timestep
void game_draw(const Game *g);

#endif // GAME_H
