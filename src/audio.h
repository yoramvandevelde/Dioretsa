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
    SND_COUNT
} SoundId;

void audio_init(void);          // after InitAudioDevice
void audio_shutdown(void);

// Once per frame: keeps the engine loop fed and slides its volume and pitch
// towards whatever the game last asked for.
void audio_update(float dt);

// The engine is one loop that never stops, only fades. Starting and stopping a
// sound on every tap of the thruster clicks and rattles; a ramp does not.
// speed is 0 at a standstill and 1 at top speed, and nudges the pitch.
void audio_set_engine(bool thrusting, float speed);

float audio_engine_level(void);     // 0 to 1, where the ramp currently sits

// Plays one voice of the sound. Repeats overlap instead of cutting each other
// off, and pitchJitter varies the pitch a little so a burst does not turn into
// one flat machine gun. Missing files are silent, never fatal.
void audio_play(SoundId id);
void audio_play_varied(SoundId id, float pitchJitter);

// Plays at an exact pitch, for when the pitch itself carries meaning.
void audio_play_pitched(SoundId id, float pitch);

// Cuts a sound off, for the long ones that should not carry into a fresh game.
void audio_stop(SoundId id);

#endif // AUDIO_H
