#include "assets.h"

#include "raylib.h"

#include <stddef.h>

const char *asset_path(const char *relative)
{
    // In order: the working directory, next to the executable (how a release is
    // laid out), one level up (how the build directory is laid out), and inside
    // a macOS bundle.
    const char *app = GetApplicationDirectory();
    const char *candidates[] = {
        TextFormat("assets/%s", relative),
        TextFormat("%sassets/%s", app, relative),
        TextFormat("%s../assets/%s", app, relative),
        TextFormat("%s../Resources/assets/%s", app, relative),
    };

    for (int i = 0; i < 4; i++) {
        if (FileExists(candidates[i])) return candidates[i];
    }
    return NULL;
}
