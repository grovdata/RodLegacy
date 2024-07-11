#pragma once

#include <SDL2/SDL.h>
#include <vector>
#include <string>
#include <iostream>

class AssetManager {
public:
    // Constructor
    AssetManager();

    // Destructor
    ~AssetManager();

    // Load an image into the asset manager
    bool loadImage(const std::string& filePath);

    // Get a loaded image
    SDL_Surface* getImage(int index) const;

    // Cleanup all loaded assets
    void cleanup();

private:
    std::vector<SDL_Surface*> assets;

    // Load a single surface
    SDL_Surface* loadSurface(const std::string& filePath);
};