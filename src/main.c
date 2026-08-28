#include "raylib.h"
#include "game.h"
#include "fx.h"
#include "audio.h"
#include "input.h"

#include <string.h>

#define FIXED_DT (1.0f / 60.0f)

// Not quite black. An OLED television dims its whole picture when what it is
// shown is this close to empty, and four levels of grey is the least that stops
// it: below what an eye picks out on any screen, and the look loses nothing.
static const Color SPACE_BLACK = { 4, 4, 4, 255 };

int main(int argc, char **argv)
{
    bool skipMenu = false;
    bool showFPS = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--godmode") == 0)  game_set_god_mode(true);
        if (strcmp(argv[i], "--skip-menu") == 0) skipMenu = true;
        if (strcmp(argv[i], "--fps") == 0) showFPS = true;
    }

    InitWindow(WORLD_W, WORLD_H, "Dioretsa");
    SetExitKey(KEY_NULL);       // Esc is ours: it asks rather than quits
    SetTargetFPS(60);
    InitAudioDevice();
    fx_load_title_font();
    audio_init();

    Game game;
    game_init(&game);
    if (!skipMenu) game_attract(&game);

    float accumulator = 0.0f;
    bool  pendingFire = false;      // held until an update step consumes it
    bool  quit        = false;

    // The close button still means what it says; only Esc and B, which are
    // easy to hit by accident, are worth a question.
    while (!quit && !WindowShouldClose()) {
        input_poll();

        if (game.confirm != CONFIRM_NONE) {
            // Nothing else is read while a question is up, so answering it
            // cannot also fire a shot or start a run.
            if (game.confirm == CONFIRM_QUIT) {
                if (input_confirm_quit())    quit = true;
                else if (input_cancel())     game.confirm = CONFIRM_NONE;
            } else {
                if (input_confirm_restart()) game_init(&game);
                else if (input_cancel())     game.confirm = CONFIRM_NONE;
            }
        } else if (input_quit()) {
            game.confirm = CONFIRM_QUIT;
        } else if (game.attract) {
            // Anything at all starts the run, and nothing else happens this
            // frame: handling the game keys as well would start it paused.
            if (input_any()) game_init(&game);
        } else {
            if (input_pause())   game.paused = !game.paused;
            if (input_music())   audio_set_music(!audio_music_on());
            // Once the run is over there is nothing left to lose, so the
            // question is only worth asking mid-flight.
            if (input_restart()) {
                if (game.gameOver) game_init(&game);
                else               game.confirm = CONFIRM_RESTART;
            }
            if (input_fire())    pendingFire = true;
        }

        Input in = input_read();
        in.fire  = pendingFire;

        // Fixed timestep: physics stays identical whatever the framerate does.
        accumulator += GetFrameTime();
        if (accumulator > 0.25f) accumulator = 0.25f;   // do not catch up after a stall
        while (accumulator >= FIXED_DT) {
            game_update(&game, &in, FIXED_DT);
            // A key press counts for exactly one step, and is never lost in a
            // frame that happens to run no steps at all.
            in.fire     = false;
            pendingFire = false;
            accumulator -= FIXED_DT;
        }

        audio_update(GetFrameTime());

        BeginDrawing();
            ClearBackground(SPACE_BLACK);
            game_draw(&game);
            if (showFPS)
                DrawFPS(20, WORLD_H - 30);
        EndDrawing();
    }

    audio_shutdown();
    fx_unload_title_font();
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
