#ifndef GAME_H
#define GAME_H

#include "raylib.h"

#include "audio.h"
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
    float  grazeTimer;      // time towards the next graze payout, reset on leaving
    float  grazeStreak;     // how long this pass has lasted, for the longest-dance stat
} Ship;

typedef struct {
    Entity base;
    float  life;            // seconds until it expires
    bool   hostile;         // fired by the intruder, so it takes rather than pays
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
    SoundId     sound;              // its own note: small is bright, big is deep
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

// The answer to standing still. Leave the field alone for long enough and this
// fades in, breaks something at random, and goes. It never shoots at the ship
// and it can never finish anything off: all it does is turn a slow field into
// a fast one, which is the field that comes looking for you. It is the only
// round thing in a world of polygons, and the only one that steers: everything
// else here is handed a heading and keeps it, while this eases onto the speed
// it wants and paces what it came for.
typedef struct {
    Entity  base;
    float   age;            // seconds since it arrived; it will not fire sooner
    float   leaveIn;        // counts down once it is done, and fades it out
    Vector2 leaveVel;       // the heading it bolts on, eased onto like any other
    int     target;         // enemy slot it came for, or -1 for none picked yet
} Intruder;

// Everything here is a counter on a line of code that already existed. The
// point is not completeness but what it says about how you played.
typedef struct {
    float time;                         // seconds survived
    float distance;                     // pixels flown
    int   kills[ENEMY_TYPE_COUNT];
    int   deathsBy[ENEMY_TYPE_COUNT];   // which type finished you off
    int   shots, hits;
    int   deaths;
    int   pickupLife, pickupBonus, pickupMissed;
    int   scoreShot, scoreGraze;        // where the points came from
    int   scoreLost;                    // and what the intruder took back
    int   bestMult;                     // most enemies grazed at once
    float bestGraze;                    // longest unbroken graze, seconds
    float closest;                      // smallest gap survived, pixels
} Stats;

// The questions the game stops to ask. Both hold the run still while they are
// up, and both are answered in the same place.
typedef enum {
    CONFIRM_NONE,
    CONFIRM_QUIT,
    CONFIRM_RESTART
} Confirm;

typedef struct {
    Ship     ship;
    Enemy    enemies[MAX_ENEMIES];  // slots: use base.alive, not a count
    Bullet   bullets[MAX_BULLETS];
    Pickup   pickups[MAX_PICKUPS];
    Intruder intruder;              // at most one at a time
    float    sinceHit;              // seconds since you last connected with anything
    int      score;
    int      lives;
    int      wave;
    float    banner;                // counts down while the wave title is up
    float    waveDelay;             // breather before the next wave spawns
    Stats    stats;
    bool     attract;               // title screen: the field drifts, nobody flies
    bool     paused;
    Confirm  confirm;               // holding still while a question is up
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

// Turns a freshly initialised game into the title screen: same field, no ship,
// no HUD. Any key runs game_init again and the run starts for real.
void game_attract(Game *g);
void game_update(Game *g, const Input *in, float dt);   // fixed timestep
void game_draw(const Game *g);

#endif // GAME_H
