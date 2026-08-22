#include "audio.h"
#include "assets.h"

#include "raylib.h"

#define VOICES 4        // simultaneous copies of the same sound

#define ENGINE_FILE     "SOUND/ENGINE.wav"
#define ENGINE_VOLUME   0.28f   // it plays constantly, so it sits well under the rest
#define ENGINE_UP       10.0f   // volume units per second while thrusting
#define ENGINE_DOWN     4.0f    // slower on release, so it winds down
#define ENGINE_PITCH_LO 0.92f   // standing still
#define ENGINE_PITCH_HI 1.12f   // flat out

static const struct {
    const char *file;
    float       volume;
} SOUNDS[SND_COUNT] = {
    [SND_CRASH]       = { "SOUND/CRASH.wav",       1.0f },
    [SND_EXPLOSION_1] = { "SOUND/EXPLOSION_1.wav", 0.7f },
    [SND_EXPLOSION_2] = { "SOUND/EXPLOSION_2.wav", 0.75f },
    [SND_EXPLOSION_3] = { "SOUND/EXPLOSION_3.wav", 0.8f },
    [SND_EXPLOSION_4] = { "SOUND/EXPLOSION_4.wav", 0.85f },
    [SND_EXPLOSION_5] = { "SOUND/EXPLOSION_5.wav", 0.9f },
    [SND_PICKUP_1UP]  = { "SOUND/PICKUP_1UP.wav",  1.0f },
    [SND_PICKUP_COIN] = { "SOUND/PICKUP_COIN.wav", 0.9f },
    [SND_SHOT]        = { "SOUND/SHOT.wav",        0.55f },
    [SND_GRAZE]       = { "SOUND/GRAZE.wav",       0.4f },
    [SND_GAMEOVER]    = { "SOUND/GAMEOVER.wav",    1.0f },
};

static Music engine;
static bool  engineLoaded  = false;
static float engineLevel   = 0.0f;      // where the ramp is now
static float engineTarget  = 0.0f;      // where it is heading
static float engineSpeed   = 0.0f;

static Sound voices[SND_COUNT][VOICES];
static int   next[SND_COUNT];
static bool  loaded[SND_COUNT];

void audio_init(void)
{
    for (int i = 0; i < SND_COUNT; i++) {
        const char *path = asset_path(SOUNDS[i].file);
        if (!path) {
            TraceLog(LOG_WARNING, "sound missing: %s", SOUNDS[i].file);
            continue;
        }

        voices[i][0] = LoadSound(path);
        if (voices[i][0].frameCount == 0) continue;

        // The aliases share the sample data, so extra voices are nearly free.
        for (int v = 1; v < VOICES; v++) voices[i][v] = LoadSoundAlias(voices[i][0]);
        for (int v = 0; v < VOICES; v++) SetSoundVolume(voices[i][v], SOUNDS[i].volume);

        loaded[i] = true;
    }

    // The engine never stops once it starts; only its volume moves.
    const char *enginePath = asset_path(ENGINE_FILE);
    if (!enginePath) {
        TraceLog(LOG_WARNING, "sound missing: %s", ENGINE_FILE);
        return;
    }

    engine = LoadMusicStream(enginePath);
    if (engine.frameCount == 0) return;

    engine.looping = true;
    SetMusicVolume(engine, 0.0f);
    PlayMusicStream(engine);
    engineLoaded = true;
}

void audio_set_engine(bool thrusting, float speed)
{
    engineTarget = thrusting ? 1.0f : 0.0f;
    engineSpeed  = (speed < 0.0f) ? 0.0f : (speed > 1.0f) ? 1.0f : speed;
}

float audio_engine_level(void)
{
    return engineLevel;
}

void audio_update(float dt)
{
    if (!engineLoaded) return;

    UpdateMusicStream(engine);

    float rate = (engineTarget > engineLevel) ? ENGINE_UP : ENGINE_DOWN;
    float step = rate * dt;

    if (engineLevel < engineTarget) {
        engineLevel += step;
        if (engineLevel > engineTarget) engineLevel = engineTarget;
    } else {
        engineLevel -= step;
        if (engineLevel < engineTarget) engineLevel = engineTarget;
    }

    SetMusicVolume(engine, engineLevel * ENGINE_VOLUME);
    SetMusicPitch(engine, ENGINE_PITCH_LO + (ENGINE_PITCH_HI - ENGINE_PITCH_LO) * engineSpeed);
}

void audio_shutdown(void)
{
    if (engineLoaded) {
        StopMusicStream(engine);
        UnloadMusicStream(engine);
        engineLoaded = false;
    }

    for (int i = 0; i < SND_COUNT; i++) {
        if (!loaded[i]) continue;
        for (int v = 1; v < VOICES; v++) UnloadSoundAlias(voices[i][v]);
        UnloadSound(voices[i][0]);
        loaded[i] = false;
    }
}

static void play_at(SoundId id, float pitch)
{
    if (id < 0 || id >= SND_COUNT || !loaded[id]) return;

    Sound s = voices[id][next[id]];
    next[id] = (next[id] + 1) % VOICES;

    SetSoundPitch(s, pitch);
    PlaySound(s);
}

void audio_play_varied(SoundId id, float pitchJitter)
{
    float j = pitchJitter * (float)GetRandomValue(-1000, 1000) / 1000.0f;
    play_at(id, 1.0f + j);
}

void audio_play_pitched(SoundId id, float pitch)
{
    play_at(id, pitch);
}

void audio_stop(SoundId id)
{
    if (id < 0 || id >= SND_COUNT || !loaded[id]) return;
    for (int v = 0; v < VOICES; v++) StopSound(voices[id][v]);
}

void audio_play(SoundId id)
{
    play_at(id, 1.0f);
}
