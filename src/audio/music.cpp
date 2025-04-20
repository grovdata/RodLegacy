#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;
using string = std::string;
using vector = std::vector;

// Function to find MP3 files in a directory
vector<string> FindMusicFiles(const string& directory) {
    vector<string> musicFiles;
    for (const auto& entry : fs::directory_iterator(directory)) {
        if (entry.path().extension() == ".mp3" || entry.path().extension() == ".ogg" || entry.path().extension() == ".flac" || entry.path().extension() == ".wave") {
            musicFiles.push_back(entry.path().string());
        }
    }
    std::sort(musicFiles.begin(), musicFiles.end()); // Sort alphabetically
    return musicFiles;
}

// Function to play MP3 files in a game loop
void playMP3FilesGameLoop(const string& directory) {
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return;
    }
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        std::cerr << "SDL_mixer could not initialize! Mix_Error: " << Mix_GetError() << std::endl;
        SDL_Quit();
        return;
    }

    std::vector<std::string> mp3Files = FindMusicFiles(directory);
    if (mp3Files.empty()) {
        std::cerr << "No MP3 files found in directory." << std::endl;
    }

    bool running = true;
    size_t currentTrack = 0;

    while (running) {
        if (currentTrack >= mp3Files.size()) {
            currentTrack = 0; // Loop back to the first track
        }

        std::cout << "Playing: " << mp3Files[currentTrack] << std::endl;
        Mix_Music* music = Mix_LoadMUS(mp3Files[currentTrack].c_str());
        if (!music) {
            std::cerr << "Failed to load " << mp3Files[currentTrack] << ". Mix_Error: " << Mix_GetError() << std::endl;
            currentTrack++;
            continue;
        }

        if (Mix_PlayMusic(music, 1) == -1) {
            std::cerr << "Error playing " << mp3Files[currentTrack] << ". Mix_Error: " << Mix_GetError() << std::endl;
        }

        while (Mix_PlayingMusic()) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) {
                    running = false;
                    Mix_HaltMusic();
                }
            }
            SDL_Delay(100);
        }

        Mix_FreeMusic(music);
        currentTrack++;
    }

    Mix_CloseAudio();
    SDL_Quit();
}

int main(int argc, char* argv[]) {
    std::string directory = (argc > 1) ? argv[1] : "."; // Default to current directory
    playMP3FilesGameLoop(directory);
    return 0;
}
