#include "raylib.h"
#include "game.h"
#include "fx.h"
#include "audio.h"

#include <string.h>

#define FIXED_DT (1.0f / 60.0f)

static Input read_input(void)
{
    Input in = { 0 };
    if (IsKeyDown(KEY_LEFT)  || IsKeyDown(KEY_A)) in.turn -= 1.0f;
    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) in.turn += 1.0f;
    in.thrust = IsKeyDown(KEY_UP) || IsKeyDown(KEY_W);
    return in;
}

int main(int argc, char **argv)
{
    bool skipMenu = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--godmode") == 0)  game_set_god_mode(true);
        if (strcmp(argv[i], "--skip-menu") == 0) skipMenu = true;
    }

    InitWindow(WORLD_W, WORLD_H, "astroid v0.2");
    SetTargetFPS(60);
    InitAudioDevice();
    fx_load_title_font();
    audio_init();

    Game game;
    game_init(&game);
    if (!skipMenu) game_attract(&game);

    float accumulator = 0.0f;
    bool  pendingFire = false;      // held until an update step consumes it

    while (!WindowShouldClose()) {
        if (game.attract) {
            // Any key at all starts the run, and nothing else happens this
            // frame: handling the game keys as well would start it paused.
            if (GetKeyPressed() != 0) game_init(&game);
        } else {
            if (IsKeyPressed(KEY_P)) game.paused = !game.paused;
            if (IsKeyPressed(KEY_M)) audio_set_music(!audio_music_on());
            if (IsKeyPressed(KEY_R)) game_init(&game);
            if (IsKeyPressed(KEY_SPACE)) pendingFire = true;
        }

        Input in = read_input();
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
            ClearBackground(BLACK);
            game_draw(&game);
            DrawFPS(20, WORLD_H - 30);
        EndDrawing();
    }

    audio_shutdown();
    fx_unload_title_font();
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
