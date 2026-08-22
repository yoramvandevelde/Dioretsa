#ifndef ASSETS_H
#define ASSETS_H

// Resolves a path relative to the assets directory, whether the game was
// started from the project root or from anywhere else. Returns NULL when the
// file is not there, so every caller can carry on without it.
const char *asset_path(const char *relative);

#endif // ASSETS_H
