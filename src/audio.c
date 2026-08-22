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
#define ENGINE_PAN      0.30f   // narrower than the one-shots, see the header
#define ENGINE_SEAM_LOW 0.70f   // how far it ducks while crossing an edge (1 = not at all)

#define MUSIC_FILE      "SOUND/leberch-space-440026.mp3"
#define MUSIC_VOLUME    0.35f   // measured: puts it around -24 dB, under everything
#define MUSIC_RAMP      0.5f    // volume units per second, so two seconds either way

// The volume column corrects for how loud each file was recorded, not for how
// loud it should feel. Measured levels differ by more than 20 dB, so values
// above 1.0 are gain, not a mistake; each one still has peak headroom.
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
    [SND_GRAZE]       = { "SOUND/GRAZE.wav",       1.6f },   // file sits 18 dB low
    [SND_GAMEOVER]    = { "SOUND/GAMEOVER.wav",    1.0f },
    [SND_WAVE]        = { "SOUND/WAVE.wav",        1.7f },
};

static Music engine;
static bool  engineLoaded  = false;
static float engineLevel   = 0.0f;      // where the ramp is now
static float engineTarget  = 0.0f;      // where it is heading
static float engineSpeed   = 0.0f;
static float enginePan     = 0.0f;
static float engineSeam    = 1.0f;

static Music track;
static bool  trackLoaded  = false;
static float trackLevel   = 0.0f;
static float trackTarget  = 1.0f;

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

    const char *musicPath = asset_path(MUSIC_FILE);
    if (!musicPath) {
        TraceLog(LOG_WARNING, "music missing: %s", MUSIC_FILE);
        return;
    }

    track = LoadMusicStream(musicPath);
    if (track.frameCount == 0) return;

    track.looping = true;
    SetMusicVolume(track, 0.0f);        // faded in by audio_update
    PlayMusicStream(track);
    trackLoaded = true;
}

void audio_set_music(bool on)
{
    trackTarget = on ? 1.0f : 0.0f;
}

bool audio_music_on(void)
{
    return trackTarget > 0.5f;
}

void audio_set_engine(EngineState st)
{
    engineTarget = st.thrusting ? 1.0f : 0.0f;
    engineSpeed  = (st.speed < 0.0f) ? 0.0f : (st.speed > 1.0f) ? 1.0f : st.speed;
    enginePan    = (st.pan < -1.0f) ? -1.0f : (st.pan > 1.0f) ? 1.0f : st.pan;
    engineSeam   = (st.seam < 0.0f) ? 0.0f : (st.seam > 1.0f) ? 1.0f : st.seam;
}

float audio_engine_level(void)
{
    return engineLevel;
}

static float ramp_towards(float level, float target, float rate, float dt)
{
    float step = rate * dt;
    if (level < target) {
        level += step;
        if (level > target) level = target;
    } else {
        level -= step;
        if (level < target) level = target;
    }
    return level;
}

void audio_update(float dt)
{
    if (trackLoaded) {
        UpdateMusicStream(track);
        trackLevel = ramp_towards(trackLevel, trackTarget, MUSIC_RAMP, dt);
        SetMusicVolume(track, trackLevel * MUSIC_VOLUME);
    }

    if (!engineLoaded) return;

    UpdateMusicStream(engine);

    float rate = (engineTarget > engineLevel) ? ENGINE_UP : ENGINE_DOWN;
    engineLevel = ramp_towards(engineLevel, engineTarget, rate, dt);

    float seam = ENGINE_SEAM_LOW + (1.0f - ENGINE_SEAM_LOW) * engineSeam;
    SetMusicVolume(engine, engineLevel * ENGINE_VOLUME * seam);
    SetMusicPitch(engine, ENGINE_PITCH_LO + (ENGINE_PITCH_HI - ENGINE_PITCH_LO) * engineSpeed);
    SetMusicPan(engine, enginePan * ENGINE_PAN);
}

void audio_shutdown(void)
{
    if (trackLoaded) {
        StopMusicStream(track);
        UnloadMusicStream(track);
        trackLoaded = false;
    }

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

static void play_ex(SoundId id, float pitch, float pan)
{
    if (id < 0 || id >= SND_COUNT || !loaded[id]) return;

    Sound s = voices[id][next[id]];
    next[id] = (next[id] + 1) % VOICES;

    if (pan < -1.0f) pan = -1.0f;
    if (pan >  1.0f) pan =  1.0f;

    SetSoundPitch(s, pitch);
    SetSoundPan(s, pan);
    PlaySound(s);
}

// Its own xorshift, for the same reason fx has one: sound must not move the
// dice the gameplay rolls.
static unsigned int rng = 88675123u;

float audio_pitch_jitter(float amount)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;

    float unit = (float)(rng >> 8) / (float)(1u << 24) * 2.0f - 1.0f;   // -1 to 1
    return 1.0f + amount * unit;
}

void audio_play_pitched(SoundId id, float pitch)
{
    play_ex(id, pitch, 0.0f);
}

void audio_play_at(SoundId id, float pan, float pitch)
{
    play_ex(id, pitch, pan);
}

void audio_stop(SoundId id)
{
    if (id < 0 || id >= SND_COUNT || !loaded[id]) return;
    for (int v = 0; v < VOICES; v++) StopSound(voices[id][v]);
}

void audio_play(SoundId id)
{
    play_ex(id, 1.0f, 0.0f);
}
