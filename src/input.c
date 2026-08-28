#include "input.h"

#include "raylib.h"

#include <math.h>

// raylib fills exactly one gamepad slot on Android, and on desktop the first
// controller connected lands in that same slot, so there is only ever one to
// read. Two players would need raylib itself to change first.
#define PAD 0

// XInput's own figure for how far an Xbox stick wanders while resting. Below
// this the stick is centred and the ship holds its heading.
#define STICK_DEADZONE 0.24f

// Triggers rest at -1 and reach +1 held flat, on desktop and Android alike.
// Half way is a deliberate pull rather than a finger resting on the trigger.
#define TRIGGER_PULLED -0.5f

// What the press queue held this frame, filled by input_poll().
static bool sawAnything;
static bool sawQuit;

// Past the deadzone the stick starts again at nothing instead of jumping
// straight to a quarter turn, so a small push stays a small correction.
static float stick(float raw)
{
    float magnitude = fabsf(raw);
    if (magnitude < STICK_DEADZONE) return 0.0f;

    float scaled = (magnitude - STICK_DEADZONE) / (1.0f - STICK_DEADZONE);
    return (raw < 0.0f) ? -scaled : scaled;
}

static bool pulled(int axis)
{
    return GetGamepadAxisMovement(PAD, axis) > TRIGGER_PULLED;
}

static bool pressed(int button)
{
    return IsGamepadButtonPressed(PAD, button);
}

static bool held(int button)
{
    return IsGamepadButtonDown(PAD, button);
}

// Which words the prompts should speak. A television is always a controller. On
// a desktop it follows whether one is plugged in, and raylib only knows that
// once the pad has actually sent something, which is the same moment the words
// start mattering.
static bool on_gamepad(void)
{
#if defined(__ANDROID__)
    return true;
#else
    return IsGamepadAvailable(PAD);
#endif
}

// The press queue empties as it is read, so it is read exactly once a frame,
// here, and every question below is answered out of that one reading. Asking
// the queue in two places would let one press count for one question and go
// missing from the other.
//
// It also has to be the queue rather than the held-key state: a press that
// begins and ends inside a single frame never shows up as held, but it always
// lands in the queue. Answering "was it Back" from one and "was it anything at
// all" from the other is how a quick tap ends up counting as neither.
void input_poll(void)
{
    sawAnything = false;
    sawQuit = false;

    for (int key = GetKeyPressed(); key != 0; key = GetKeyPressed()) {
        sawAnything = true;
        // Esc on a desk, Back on a remote: the same door.
        if ((key == KEY_ESCAPE) || (key == KEY_BACK)) sawQuit = true;
    }

    // Buttons are asked for one at a time instead of through
    // GetGamepadButtonPressed(), which raylib's Android backend clears every
    // frame and never fills in, so on a television it would answer that nothing
    // was ever pressed.
    for (int button = GAMEPAD_BUTTON_LEFT_FACE_UP; button <= GAMEPAD_BUTTON_RIGHT_THUMB; button++) {
        if (!pressed(button)) continue;

        sawAnything = true;
        if (button == GAMEPAD_BUTTON_RIGHT_FACE_RIGHT) sawQuit = true;   // B
    }
}

Input input_read(void)
{
    Input in = { 0 };

    // Keys and the D-pad are all or nothing and mean it, so they answer first;
    // the stick is only consulted when neither is saying anything. Left and
    // right together still cancel out, exactly as they did on the keyboard.
    if (IsKeyDown(KEY_LEFT)  || IsKeyDown(KEY_A) || held(GAMEPAD_BUTTON_LEFT_FACE_LEFT))  in.turn -= 1.0f;
    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D) || held(GAMEPAD_BUTTON_LEFT_FACE_RIGHT)) in.turn += 1.0f;
    if (in.turn == 0.0f) in.turn = stick(GetGamepadAxisMovement(PAD, GAMEPAD_AXIS_LEFT_X));

    in.thrust = IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)
             || held(GAMEPAD_BUTTON_LEFT_FACE_UP)       // D-pad up
             || held(GAMEPAD_BUTTON_RIGHT_FACE_DOWN)    // A
             || pulled(GAMEPAD_AXIS_RIGHT_TRIGGER);     // RT

    return in;
}

bool input_fire(void)
{
    return IsKeyPressed(KEY_SPACE)
        || pressed(GAMEPAD_BUTTON_RIGHT_FACE_LEFT)      // X
        || pressed(GAMEPAD_BUTTON_RIGHT_TRIGGER_1);     // RB
}

bool input_pause(void)   { return IsKeyPressed(KEY_P) || pressed(GAMEPAD_BUTTON_MIDDLE_RIGHT); }    // Start
bool input_restart(void) { return IsKeyPressed(KEY_R) || pressed(GAMEPAD_BUTTON_RIGHT_FACE_UP); }   // Y
bool input_music(void)   { return IsKeyPressed(KEY_M) || pressed(GAMEPAD_BUTTON_MIDDLE_LEFT); }     // View

// Esc, B, and a remote's back button all raise the question, and one of them
// answers it below. The press that raises it is spent doing so: a press is only
// in the queue for the frame it happened, so it cannot also answer itself.
bool input_quit(void)
{
    return sawQuit;
}

// A television answers a question the way Android answers all of them: by doing
// the thing again. There is no keyboard to press Y on, and a remote's OK button
// does not reach raylib at all, so the button that raised the question is the
// only one certain to be there. On a desktop, Y and Enter mean yes as they
// always have.
bool input_confirm_quit(void)
{
#if defined(__ANDROID__)
    return sawQuit;
#else
    return IsKeyPressed(KEY_Y) || IsKeyPressed(KEY_ENTER)
        || pressed(GAMEPAD_BUTTON_RIGHT_FACE_DOWN);     // A
#endif
}

bool input_confirm_restart(void)
{
#if defined(__ANDROID__)
    return input_restart();
#else
    return IsKeyPressed(KEY_Y) || IsKeyPressed(KEY_ENTER)
        || pressed(GAMEPAD_BUTTON_RIGHT_FACE_DOWN);     // A
#endif
}

bool input_cancel(void)
{
#if defined(__ANDROID__)
    // Anything that is not the answer. Naming one key to say no would mean
    // naming one that a remote might not have.
    return sawAnything;
#else
    // N, or the same way back out that raised the question.
    return IsKeyPressed(KEY_N) || sawQuit;
#endif
}

bool input_any(void)
{
    return sawAnything;
}

// The prompts the game draws. They live here, next to the mapping they describe,
// so that a button moving cannot leave the words behind pointing at the old one.
const char *input_start_hint(void)
{
    return on_gamepad() ? "ANY BUTTON..." : "ANY KEY...";
}

const char *input_restart_hint(void)
{
    return on_gamepad() ? "press Y" : "press R";
}

const char *input_quit_question_hint(void)
{
#if defined(__ANDROID__)
    return "BACK  quit          ANYTHING ELSE  keep playing";
#else
    return on_gamepad() ? "A  quit          B  keep playing"
                        : "Y  quit          N  keep playing";
#endif
}

const char *input_restart_question_hint(void)
{
#if defined(__ANDROID__)
    return "Y  start over          ANYTHING ELSE  keep playing";
#else
    return on_gamepad() ? "A  start over          B  keep playing"
                        : "Y  start over          N  keep playing";
#endif
}
