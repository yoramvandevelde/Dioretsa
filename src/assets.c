#include "assets.h"

#include "raylib.h"

#include <stddef.h>

const char *asset_path(const char *relative)
{
#if defined(__ANDROID__)
    // Inside an APK there is nothing to search for. raylib wraps fopen so that
    // a plain relative name is looked up in the packaged assets, and Gradle
    // packs the repository's assets directory as the root of those, so the name
    // it was given is already the answer. Searching would not work here in any
    // case: FileExists() asks the filesystem through access(), which knows
    // nothing about what is inside an APK.
    return relative;
#else
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
#endif
}
