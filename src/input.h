#ifndef INPUT_H
#define INPUT_H

#include "game.h"

// Keyboard and controller behind one door. The loop asks for actions and never
// for keys, so adding a device, or moving a button, is a change in input.c and
// nowhere else.

// Reads the devices for this frame. Call it once, at the top of the loop and
// before anything below is asked: the press queue empties as it is read, and
// this is the one place that reads it.
void input_poll(void);

// Held state: what the ship is being told to do this frame.
Input input_read(void);

// One-shot actions, true only on the frame the key or button goes down.
bool input_fire(void);
bool input_pause(void);
bool input_restart(void);
bool input_music(void);
bool input_quit(void);      // asks the question; it does not answer it
bool input_confirm_quit(void);
bool input_confirm_restart(void);
bool input_cancel(void);    // waves either question away
bool input_any(void);       // any sign of life, for the attract screen

// The prompts the game draws, which name buttons and so belong next to the
// mapping rather than next to the drawing.
const char *input_start_hint(void);
const char *input_restart_hint(void);
const char *input_quit_question_hint(void);
const char *input_restart_question_hint(void);

#endif
