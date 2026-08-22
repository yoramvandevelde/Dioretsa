#ifndef AUDIO_H
#define AUDIO_H

#include <stdbool.h>

// One line here and one row in SOUNDS is all a new sound takes.
typedef enum {
    SND_CRASH,
    SND_EXPLOSION_1,        // highest, 512 Hz
    SND_EXPLOSION_2,
    SND_EXPLOSION_3,
    SND_EXPLOSION_4,
    SND_EXPLOSION_5,        // deepest, 114 Hz
    SND_PICKUP_1UP,         // the pickup handed a life back
    SND_PICKUP_COIN,        // the pickup paid out points instead
    SND_SHOT,
    SND_GRAZE,
    SND_GAMEOVER,
    SND_WAVE,
    SND_COUNT
} SoundId;

void audio_init(void);          // after InitAudioDevice
void audio_shutdown(void);

// Once per frame: keeps the engine loop fed and slides its volume and pitch
// towards whatever the game last asked for.
void audio_update(float dt);

// The engine is one loop that never stops, only fades. Starting and stopping a
// sound on every tap of the thruster clicks and rattles; a ramp does not.
typedef struct {
    bool  thrusting;
    float speed;    // 0 standing still, 1 flat out; nudges the pitch
    float pan;      // -1 to 1, narrowed again inside: it plays continuously
    float seam;     // 1 in open space, 0 at a wrapping edge
} EngineState;

// The seam value ducks the engine while the ship crosses an edge. The world
// wraps, so its pan flips sides there, and a flip you can hear is a flip that
// sounds broken. Fading through the crossing hides it.
void audio_set_engine(EngineState state);

// The soundscape runs from launch to exit, straight through pause and game
// over: it is the room you are in, not a reaction to anything. Toggling fades
// rather than cuts, so A/B-ing it is not itself an event.
void audio_set_music(bool on);
bool audio_music_on(void);

// A hint of room around everything, on the final mix. raylib has no send bus,
// so this is all or nothing: keep it subtle, or it flattens the difference
// between near and far. Toggling ramps rather than switches.
void audio_set_reverb(bool on);
bool audio_reverb_on(void);

// Runs the reverb over interleaved stereo floats. The audio thread calls this;
// it is exposed so it can also be measured offline.
void audio_reverb_process(float *samples, unsigned int frames);

float audio_engine_level(void);     // 0 to 1, where the ramp currently sits

// Plays one voice of the sound. Repeats overlap instead of cutting each other
// off, and pitchJitter varies the pitch a little so a burst does not turn into
// one flat machine gun. Missing files are silent, never fatal.
void audio_play(SoundId id);

// Plays at an exact pitch, for when the pitch itself carries meaning.
void audio_play_pitched(SoundId id, float pitch);

// pan runs -1 hard left to +1 hard right. The world is one screen wide, so
// where a thing happens is where you should hear it.
void audio_play_at(SoundId id, float pan, float pitch);

// A pitch of 1 give or take `amount`, drawn from audio's own generator so that
// making noise never shifts the random stream the gameplay draws from.
float audio_pitch_jitter(float amount);

// Cuts a sound off, for the long ones that should not carry into a fresh game.
void audio_stop(SoundId id);

#endif // AUDIO_H
