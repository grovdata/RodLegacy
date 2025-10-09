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
        "Street Rod Music Player - Song 0",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        500, 300,
        SDL_WINDOW_SHOWN
    );
    
    if (!window) {
        SDL_Log("Failed to create window: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    
    // Function to update window title
    auto updateWindowTitle = [&]() {
        if (musicLoaded) {
            char title[100];
            snprintf(title, sizeof(title), "Street Rod Music Player - Song %d", sequencer.GetCurrentSong());
            SDL_SetWindowTitle(window, title);
        }
    };
    
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
        SDL_Log("  CLICK GREEN BOXES - Cycle instruments for each channel");
        SDL_Log("  Q/W/E/T - Change instrument for channels 0/1/2/3");
        SDL_Log("  A/D/F/G - Change instrument for channels 0/1/2/3 (alt)");
        SDL_Log("  Z/X/C/V - Change instrument for channels 0/1/2/3 (alt2)");
        SDL_Log("");
    
    while (!quit) {
        // Handle events
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                quit = true;
            } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                // Handle mouse clicks on channel boxes
                if (event.button.button == SDL_BUTTON_LEFT) {
                    int x = event.button.x;
                    int y = event.button.y;
                    
                    // Check if click is within channel boxes (50 + i * 80, 70, 60, 60)
                    for (int i = 0; i < 4; i++) {
                        int boxX = 50 + i * 80;
                        int boxY = 70;
                        if (x >= boxX && x < boxX + 60 && y >= boxY && y < boxY + 60) {
                            if (musicLoaded) {
                                // Cycle through instruments for this channel
                                int currentInst = sequencer.GetChannelInstrument(i);
                                int nextInst = (currentInst + 1) % 5; // 5 instruments (0-4)
                                sequencer.SetChannelInstrument(i, nextInst);
                                
                                const char* instNames[] = {"lead melody", "electric bass", "bassdrum", "hihat", "electric guitar"};
                                SDL_Log("Channel %d: switched to instrument %d (%s)", i, nextInst, instNames[nextInst]);
                            }
                            break;
                        }
                    }
                }
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
                        updateWindowTitle();
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
                
                // Instrument selection for channels 0-3
                case SDLK_q: // Channel 0, instrument set 1
                    if (musicLoaded) {
                        sequencer.SetChannelInstrument(0, 0); // tone
                        SDL_Log("Channel 0: Instrument 0 (tone)");
                    }
                    break;
                case SDLK_w: // Channel 1, instrument set 1
                    if (musicLoaded) {
                        sequencer.SetChannelInstrument(1, 1); // bassdrum
                        SDL_Log("Channel 1: Instrument 1 (bassdrum)");
                    }
                    break;
                case SDLK_e: // Channel 2, instrument set 1
                    if (musicLoaded) {
                        sequencer.SetChannelInstrument(2, 2); // hihat
                        SDL_Log("Channel 2: Instrument 2 (hihat)");
                    }
                    break;
                case SDLK_t: // Channel 3, instrument set 1
                    if (musicLoaded) {
                        sequencer.SetChannelInstrument(3, 3); // buzz
                        SDL_Log("Channel 3: Instrument 3 (buzz)");
                    }
                    break;
                
                // Instrument selection for channels 0-3 (alt set)
                case SDLK_a: // Channel 0, instrument set 2
                    if (musicLoaded) {
                        sequencer.SetChannelInstrument(0, 1); // bassdrum
                        SDL_Log("Channel 0: Instrument 1 (bassdrum)");
                    }
                    break;
                case SDLK_d: // Channel 1, instrument set 2
                    if (musicLoaded) {
                        sequencer.SetChannelInstrument(1, 2); // hihat
                        SDL_Log("Channel 1: Instrument 2 (hihat)");
                    }
                    break;
                case SDLK_f: // Channel 2, instrument set 2
                    if (musicLoaded) {
                        sequencer.SetChannelInstrument(2, 3); // buzz
                        SDL_Log("Channel 2: Instrument 3 (buzz)");
                    }
                    break;
                case SDLK_g: // Channel 3, instrument set 2
                    if (musicLoaded) {
                        sequencer.SetChannelInstrument(3, 0); // tone
                        SDL_Log("Channel 3: Instrument 0 (tone)");
                    }
                    break;
                
                // Instrument selection for channels 0-3 (alt2 set)
                case SDLK_z: // Channel 0, instrument set 3
                    if (musicLoaded) {
                        sequencer.SetChannelInstrument(0, 2); // hihat
                        SDL_Log("Channel 0: Instrument 2 (hihat)");
                    }
                    break;
                case SDLK_x: // Channel 1, instrument set 3
                    if (musicLoaded) {
                        sequencer.SetChannelInstrument(1, 3); // buzz
                        SDL_Log("Channel 1: Instrument 3 (buzz)");
                    }
                    break;
                case SDLK_c: // Channel 2, instrument set 3
                    if (musicLoaded) {
                        sequencer.SetChannelInstrument(2, 0); // tone
                        SDL_Log("Channel 2: Instrument 0 (tone)");
                    }
                    break;
                case SDLK_v: // Channel 3, instrument set 3
                    if (musicLoaded) {
                        sequencer.SetChannelInstrument(3, 1); // bassdrum
                        SDL_Log("Channel 3: Instrument 1 (bassdrum)");
                    }
                    break;
                case SDLK_1:
                case SDLK_2:
                case SDLK_3:
                case SDLK_4: {
                    int ch = event.key.keysym.sym - SDLK_1;
                    if (musicLoaded) {
                        sequencer.ToggleChannelMute(ch);
                    }
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
            
            // Draw instrument number if music is loaded
            if (musicLoaded) {
                int inst = sequencer.GetChannelInstrument(i);
                char instText[10];
                snprintf(instText, sizeof(instText), "I%d", inst);
                
                // Simple text rendering (you could use SDL_ttf for better text)
                // For now, just draw a small rectangle to indicate instrument
                SDL_Rect instRect = { 50 + i * 80 + 5, 70 + 5, 10, 10 };
                SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
                SDL_RenderFillRect(renderer, &instRect);
            }
        }
        
        // Draw instructions
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        // Simple text rendering would go here - for now just draw some lines
        for (int i = 0; i < 4; i++) {
            SDL_Rect textRect = { 50 + i * 80, 140, 60, 20 };
            SDL_RenderDrawRect(renderer, &textRect);
        }
        
        SDL_RenderPresent(renderer);
        
        // Cap frame rate
        SDL_Delay(16);  // ~60 FPS
    }
    
    // Cleanup
    sequencer.StopMusic();
    paula.Stop();
    paula.Shutdown();
    
    // Log final arrangement before exit
    if (musicLoaded) {
        sequencer.LogCurrentArrangement();
    }
    
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    SDL_Log("Music player stopped");
    return 0;
}

