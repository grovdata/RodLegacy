#include "paula_emulator.h"
#include "music_sequencer.h"
#include <SDL2/SDL.h>
#include <iostream>
#include <memory>

// Hardcoded paths for testing - adjust these to your ADF mount points
const char* NOTE_FILE = "/home/user/kod/privat/streetrod-bins/amiga/SR_Disk1/msr.note";
const char* WAVE_FILE = "/home/user/kod/privat/streetrod-bins/amiga/SR_Disk1/srl1.wave";
const char* INS_FILE = "/home/user/kod/privat/streetrod-bins/amiga/SR_Disk1/srl.ins";
const char* ENV_FILE = "/home/user/kod/privat/streetrod-bins/amiga/SR_Disk1/srl.env";

// Alternative: Test with simple generated samples
bool TestWithGeneratedSample(PaulaEmulator& paula) {
    SDL_Log("Testing with generated sawtooth wave...");
    
    // Generate a simple sawtooth wave
    std::vector<int8_t> sawtooth(1000);
    for (int i = 0; i < 1000; i++) {
        sawtooth[i] = static_cast<int8_t>((i % 256) - 128);
    }
    
    // Play on channel 0 - C note (period 428)
    paula.SetChannelSample(0, sawtooth);
    paula.SetChannelPeriod(0, 428);
    paula.SetChannelVolume(0, 48);
    paula.SetChannelLoop(0, 0, sawtooth.size());
    paula.EnableChannel(0, true);
    
    // Play on channel 1 - E note (period 340)
    paula.SetChannelSample(1, sawtooth);
    paula.SetChannelPeriod(1, 340);
    paula.SetChannelVolume(1, 48);
    paula.SetChannelLoop(1, 0, sawtooth.size());
    paula.EnableChannel(1, true);
    
    // Play on channel 2 - G note (period 286)
    paula.SetChannelSample(2, sawtooth);
    paula.SetChannelPeriod(2, 286);
    paula.SetChannelVolume(2, 48);
    paula.SetChannelLoop(2, 0, sawtooth.size());
    paula.EnableChannel(2, true);
    
    SDL_Log("Playing C-E-G chord...");
    return true;
}

int main(int argc, char* argv[]) {
    SDL_Log("Street Rod Music Player Test");
    SDL_Log("============================");
    
    // Initialize SDL
    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO) < 0) {
        SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
        return 1;
    }
    
    // Create Paula emulator
    PaulaEmulator paula;
    if (!paula.Initialize(44100)) {
        SDL_Log("Failed to initialize Paula emulator");
        SDL_Quit();
        return 1;
    }
    
    // Create music sequencer
    MusicSequencer sequencer;
    if (!sequencer.Initialize(&paula)) {
        SDL_Log("Failed to initialize music sequencer");
        SDL_Quit();
        return 1;
    }
    
    // Try to load music files
    bool musicLoaded = sequencer.LoadMusicFiles(NOTE_FILE, WAVE_FILE, INS_FILE, ENV_FILE);
    
    if (musicLoaded) {
        SDL_Log("Music files loaded, starting playback...");
        sequencer.StartMusic();
        paula.Start();
    } else {
        SDL_Log("Could not load music files, testing with generated samples instead...");
        TestWithGeneratedSample(paula);
        paula.Start();
    }
    
    // Create a simple window for event handling
    SDL_Window* window = SDL_CreateWindow(
        "Street Rod Music Player",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        400, 200,
        SDL_WINDOW_SHOWN
    );
    
    if (!window) {
        SDL_Log("Failed to create window: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    
    // Main loop
    bool quit = false;
    SDL_Event event;
    Uint32 lastTime = SDL_GetTicks();
    
    SDL_Log("");
    SDL_Log("Controls:");
    SDL_Log("  ESC - Quit");
    SDL_Log("  SPACE - Pause/resume");
    SDL_Log("  1-4 - Toggle channels 0-3");
    SDL_Log("  S - Switch to next song (0->1->2->0)");
    SDL_Log("  R - Restart current song");
    SDL_Log("");
    
    while (!quit) {
        // Handle events
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                quit = true;
            } else if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                case SDLK_ESCAPE:
                    quit = true;
                    break;
                case SDLK_SPACE:
                    if (musicLoaded) {
                        static bool isPaused = false;
                        isPaused = !isPaused;
                        sequencer.PauseMusic(isPaused);
                        SDL_Log("Music %s", isPaused ? "paused" : "resumed");
                    }
                    break;
                case SDLK_s:
                    if (musicLoaded) {
                        static int currentSong = 0;
                        currentSong = (currentSong + 1) % 3;
                        sequencer.StopMusic();
                        sequencer.SelectSong(currentSong);
                        sequencer.StartMusic();
                        SDL_Log("Switched to song %d", currentSong);
                    }
                    break;
                case SDLK_r:
                    if (musicLoaded) {
                        sequencer.StopMusic();
                        sequencer.StartMusic();
                        SDL_Log("Restarted current song");
                    }
                    break;
                case SDLK_1:
                case SDLK_2:
                case SDLK_3:
                case SDLK_4: {
                    int ch = event.key.keysym.sym - SDLK_1;
                    bool enabled = paula.IsChannelPlaying(ch);
                    paula.EnableChannel(ch, !enabled);
                    SDL_Log("Channel %d: %s", ch, !enabled ? "enabled" : "disabled");
                    break;
                }
                }
            }
        }
        
        // Update music sequencer
        Uint32 currentTime = SDL_GetTicks();
        float deltaTime = (currentTime - lastTime) / 1000.0f;
        lastTime = currentTime;
        
        if (musicLoaded) {
            sequencer.Update(deltaTime);
        }
        
        // Render
        SDL_SetRenderDrawColor(renderer, 32, 32, 64, 255);
        SDL_RenderClear(renderer);
        
        // Draw channel status indicators
        for (int i = 0; i < PaulaEmulator::NUM_CHANNELS; i++) {
            SDL_Rect rect = { 50 + i * 80, 70, 60, 60 };
            
            if (paula.IsChannelPlaying(i)) {
                SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
            } else {
                SDL_SetRenderDrawColor(renderer, 64, 64, 64, 255);
            }
            SDL_RenderFillRect(renderer, &rect);
            
            // Draw channel number
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderDrawRect(renderer, &rect);
        }
        
        SDL_RenderPresent(renderer);
        
        // Cap frame rate
        SDL_Delay(16);  // ~60 FPS
    }
    
    // Cleanup
    sequencer.StopMusic();
    paula.Stop();
    paula.Shutdown();
    
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    SDL_Log("Music player stopped");
    return 0;
}

