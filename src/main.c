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

// Fits the world into a space of the given size without changing its shape.
static float fit_scale(float w, float h)
{
    float sx = w / (float)WORLD_W;
    float sy = h / (float)WORLD_H;
    return (sx < sy) ? sx : sy;
}

// The world is 1280x720 and stays that way whatever the display is: every rule,
// distance and speed is written in those units, so nothing in the simulation
// may learn how many pixels there happen to be. Only the drawing scales, which
// is what lets a 4K television rasterise these vectors at its own resolution
// instead of stretching a 720p picture over nine times the pixels.
//
// A display that is not 16:9 gets bars rather than a stretched world.
static Camera2D world_view(void)
{
    float w = (float)GetScreenWidth();
    float h = (float)GetScreenHeight();
    float s = fit_scale(w, h);

    Camera2D cam = { 0 };
    cam.offset = (Vector2){ (w - WORLD_W * s) / 2.0f, (h - WORLD_H * s) / 2.0f };
    cam.zoom   = s;
    return cam;
}

// World units to real pixels, which is what a font atlas has to be baked for.
// The view scale is in whatever unit the platform measures the window in, and a
// desktop high-density display quietly multiplies that again; Android reports a
// density that is not a framebuffer multiplier, so there it must not count.
static float pixel_scale(void)
{
#if defined(__ANDROID__)
    return world_view().zoom;
#else
    return world_view().zoom * GetWindowScaleDPI().x;
#endif
}

int main(int argc, char **argv)
{
    bool skipMenu = false;
    bool showFPS = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--godmode") == 0)  game_set_god_mode(true);
        if (strcmp(argv[i], "--skip-menu") == 0) skipMenu = true;
        if (strcmp(argv[i], "--fps") == 0) showFPS = true;
    }

    // raylib calls main() with nothing but a program name on Android, so none
    // of those flags can ever arrive on a television. The frame counter is the
    // one worth having there, now that the panel decides how many pixels get
    // drawn, and a debug build is the honest place to switch it on: a release
    // compiles with NDEBUG and never shows it.
#if defined(__ANDROID__) && !defined(NDEBUG)
    showFPS = true;
#endif

#if defined(__ANDROID__)
    // A television has no window to size. Zero asks raylib for the panel's own
    // resolution, so the framebuffer is the screen rather than a 720p buffer
    // the set has to stretch back up to it.
    InitWindow(0, 0, "Dioretsa");
#else
    // HIGHDPI so a retina panel draws at its real pixel count instead of
    // upscaling, RESIZABLE so the window can be dragged to any size and the
    // picture follows it.
    SetConfigFlags(FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_RESIZABLE);
    InitWindow(WORLD_W, WORLD_H, "Dioretsa");
#endif
    SetExitKey(KEY_NULL);       // Esc is ours: it asks rather than quits
    SetTargetFPS(60);
    InitAudioDevice();
    // The atlases are baked once, at the scale the window opens on. Dragging a
    // window far past that leaves the text leaning on bilinear filtering,
    // which is a fair trade for not rebuilding a font mid-game.
    fx_load_fonts(pixel_scale());
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

        Camera2D view = world_view();

        BeginDrawing();
            ClearBackground(SPACE_BLACK);
            // Glows and wrapped ghosts reach past the world edge, and the wave
            // banner zooms straight through it, so the bars are fenced off.
            BeginScissorMode((int)view.offset.x, (int)view.offset.y,
                             (int)(WORLD_W * view.zoom), (int)(WORLD_H * view.zoom));
                BeginMode2D(view);
                    game_draw(&game);
                    if (showFPS)
                        DrawFPS(20, WORLD_H - 30);
                EndMode2D();
            EndScissorMode();
        EndDrawing();
    }

    audio_shutdown();
    fx_unload_fonts();
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
