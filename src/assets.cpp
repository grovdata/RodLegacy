#include "assets.h"
#include "bitmap.h"
#include "parsers/libp.h"

AssetManager::AssetManager() {}

AssetManager::~AssetManager() {
    cleanup();
}

bool AssetManager::loadImage(const std::string& filePath) {
    SDL_Surface* surface = loadSurface(filePath);
    if (surface != nullptr) {
        assets.push_back(surface);
        return true;
    }
    return false;
}

SDL_Surface* AssetManager::getImage(int index) const {
    if (index >= 0 && index < assets.size()) {
        return assets[index];
    }
    std::cerr << "Error: Index out of bounds in getImage()" << std::endl;
    return nullptr;
}

void AssetManager::cleanup() {
    for (auto surface : assets) {
        SDL_FreeSurface(surface);
    }
    assets.clear();
}

SDL_Surface* AssetManager::loadSurface(const std::string& filePath) {
    SDL_Surface* surface = SDL_LoadBMP(filePath.c_str());
    if (surface == nullptr) {
        std::cerr << "Unable to load image " << filePath << "! SDL Error: " << SDL_GetError() << std::endl;
    }
    return surface;
}
