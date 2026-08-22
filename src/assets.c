#include "assets.h"

#include "raylib.h"

#include <stddef.h>

const char *asset_path(const char *relative)
{
    // Next to the working directory first, then next to the executable, which
    // covers running from the project root and from the build directory alike.
    const char *candidates[] = {
        TextFormat("assets/%s", relative),
        TextFormat("%s../assets/%s", GetApplicationDirectory(), relative),
    };

    for (int i = 0; i < 2; i++) {
        if (FileExists(candidates[i])) return candidates[i];
    }
    return NULL;
}
