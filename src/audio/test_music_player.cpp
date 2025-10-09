#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <memory>

// ImGui includes
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_sdlrenderer2.h"

#include "paula_emulator.h"
#include "music_sequencer.h"

class MusicPlayerGUI {
private:
    SDL_Window* window;
    SDL_Renderer* renderer;
    std::unique_ptr<PaulaEmulator> paula;
    std::unique_ptr<MusicSequencer> sequencer;
    
    bool musicLoaded = false;
    bool showDemo = false;
    bool showTracker = true;
    bool showControls = true;
    
    // Tempo control
    float tempoMultiplier = 1.0f;
    
    // Pattern display
    std::vector<std::string> patternData;
    
    // Channel controls
    bool channelMuted[4] = {false, false, false, false};
    int channelInstruments[4] = {0, 0, 1, 2}; // Default instruments
    
    // Sample names for display
    const char* sampleNames[5] = {
        "lead tone",
        "electric bass", 
        "bassdrum",
        "hihat",
        "electric guitar"
    };

public:
    MusicPlayerGUI() : window(nullptr), renderer(nullptr) {
        // Initialize SDL
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
            std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
            return;
        }

        // Create window
        window = SDL_CreateWindow("Street Rod Music Player - ImGui",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            1200, 800, SDL_WINDOW_RESIZABLE);
        
        if (!window) {
            std::cerr << "Failed to create window: " << SDL_GetError() << std::endl;
            return;
        }

        // Create renderer
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!renderer) {
            std::cerr << "Failed to create renderer: " << SDL_GetError() << std::endl;
            return;
        }

        // Setup ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        // Setup ImGui style
        ImGui::StyleColorsDark();

        // Setup Platform/Renderer backends
        ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
        ImGui_ImplSDLRenderer2_Init(renderer);

        // Initialize audio components
        paula = std::make_unique<PaulaEmulator>();
        sequencer = std::make_unique<MusicSequencer>();
        
        // Load music files
        LoadMusicFiles();
    }

    ~MusicPlayerGUI() {
        // Cleanup
        ImGui_ImplSDLRenderer2_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();

        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
    }

    void LoadMusicFiles() {
        const char* NOTE_FILE = "../streetrod-bins/amiga/SR_disk1/msr.note";
        const char* WAVE_FILE = "../streetrod-bins/amiga/SR_disk1/srl1.wave";
        const char* INS_FILE = "../streetrod-bins/amiga/SR_disk1/srl.ins";
        const char* ENV_FILE = "../streetrod-bins/amiga/SR_disk1/srl.env";

        // Initialize Paula emulator first
        if (!paula->Initialize(44100)) {
            std::cerr << "Failed to initialize Paula emulator!" << std::endl;
            return;
        }

        // Initialize sequencer with Paula emulator
        if (!sequencer->Initialize(paula.get())) {
            std::cerr << "Failed to initialize music sequencer!" << std::endl;
            return;
        }

        if (sequencer->LoadMusicFiles(NOTE_FILE, WAVE_FILE, INS_FILE, ENV_FILE)) {
            musicLoaded = true;
            sequencer->SelectSong(0);
            sequencer->StartMusic();
            paula->Start();  // Start the audio device
            
            // Sync channel instruments with sequencer
            for (int i = 0; i < 4; i++) {
                channelInstruments[i] = sequencer->GetChannelInstrument(i);
            }
            
            std::cout << "Music files loaded successfully!" << std::endl;
        } else {
            std::cerr << "Failed to load music files!" << std::endl;
        }
    }

    void Run() {
        bool running = true;
        SDL_Event event;

        while (running) {
            // Handle events
            while (SDL_PollEvent(&event)) {
                ImGui_ImplSDL2_ProcessEvent(&event);
                
                if (event.type == SDL_QUIT) {
                    running = false;
                }
                
                if (event.type == SDL_KEYDOWN) {
                    HandleKeyPress(event.key.keysym.sym);
                }
            }

            // Start ImGui frame
            ImGui_ImplSDLRenderer2_NewFrame();
            ImGui_ImplSDL2_NewFrame();
            ImGui::NewFrame();

            // Update music
            if (musicLoaded) {
                sequencer->Update(1.0f / 60.0f * tempoMultiplier);
            }

            // Render GUI
            RenderGUI();

            // Render
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            
            ImGui::Render();
            ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
            
            SDL_RenderPresent(renderer);
        }
    }

    void HandleKeyPress(SDL_Keycode key) {
        if (!musicLoaded) return;

        switch (key) {
            case SDLK_ESCAPE:
                // Quit handled by SDL_QUIT event
                break;
            case SDLK_SPACE:
                if (sequencer->IsPlaying()) {
                    sequencer->PauseMusic();
                } else {
                    sequencer->ResumeMusic();
                }
                break;
            case SDLK_s:
                sequencer->NextPattern();
                break;
            case SDLK_r:
                sequencer->StopMusic();
                sequencer->SelectSong(sequencer->GetCurrentSong());
                sequencer->StartMusic();
                break;
            case SDLK_1:
            case SDLK_2:
            case SDLK_3:
            case SDLK_4:
                {
                    int channel = key - SDLK_1;
                    sequencer->ToggleChannelMute(channel);
                    channelMuted[channel] = !channelMuted[channel];
                }
                break;
        }
    }

    void RenderGUI() {
        // Main menu bar
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Windows")) {
                ImGui::MenuItem("Controls", nullptr, &showControls);
                ImGui::MenuItem("Tracker", nullptr, &showTracker);
                ImGui::MenuItem("Demo Window", nullptr, &showDemo);
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        // Controls window
        if (showControls) {
            RenderControlsWindow();
        }

        // Tracker window
        if (showTracker) {
            RenderTrackerWindow();
        }

        // Demo window
        if (showDemo) {
            ImGui::ShowDemoWindow(&showDemo);
        }
    }

    void RenderControlsWindow() {
        ImGui::Begin("Music Controls", &showControls);
        
        if (!musicLoaded) {
            ImGui::Text("Music not loaded!");
            ImGui::End();
            return;
        }

        // Playback controls
        ImGui::Text("Playback Controls");
        ImGui::Separator();
        
        if (ImGui::Button(sequencer->IsPlaying() ? "Pause" : "Play")) {
            if (sequencer->IsPlaying()) {
                sequencer->PauseMusic();
            } else {
                sequencer->ResumeMusic();
            }
        }
        ImGui::SameLine();
        
        if (ImGui::Button("Stop")) {
            sequencer->StopMusic();
        }
        ImGui::SameLine();
        
        if (ImGui::Button("Restart")) {
            sequencer->StopMusic();
            sequencer->SelectSong(sequencer->GetCurrentSong());
            sequencer->StartMusic();
        }
        ImGui::SameLine();
        
        if (ImGui::Button("Next Pattern")) {
            sequencer->NextPattern();
        }

        // Tempo control
        ImGui::Spacing();
        ImGui::Text("Tempo Control");
        ImGui::Separator();
        
        if (ImGui::SliderFloat("Tempo Multiplier", &tempoMultiplier, 0.1f, 3.0f, "%.2fx")) {
            // Tempo is applied in the update loop
        }

        // Song info
        ImGui::Spacing();
        ImGui::Text("Song Information");
        ImGui::Separator();
        
        ImGui::Text("Current Song: %d", sequencer->GetCurrentSong());
        ImGui::Text("Current Pattern: %d", sequencer->GetCurrentPattern());
        
        // Channel controls
        ImGui::Spacing();
        ImGui::Text("Channel Controls");
        ImGui::Separator();
        
        for (int i = 0; i < 4; i++) {
            ImGui::PushID(i);
            
            ImGui::Text("Channel %d:", i);
            ImGui::SameLine();
            
            // Mute button
            if (ImGui::Button(channelMuted[i] ? "Muted" : "Active")) {
                sequencer->ToggleChannelMute(i);
                channelMuted[i] = !channelMuted[i];
            }
            ImGui::SameLine();
            
            // Instrument selector
            ImGui::Text("Sample:");
            ImGui::SameLine();
            if (ImGui::Combo("##sample", &channelInstruments[i], sampleNames, 5)) {
                // Update instrument for this channel
                sequencer->SetChannelInstrument(i, channelInstruments[i]);
            }
            
            ImGui::PopID();
        }

        ImGui::End();
    }

    void RenderTrackerWindow() {
        ImGui::Begin("Music Tracker", &showTracker);
        
        if (!musicLoaded) {
            ImGui::Text("Music not loaded!");
            ImGui::End();
            return;
        }

        // Pattern display header
        ImGui::Text("Pattern: %d | Song: %d", sequencer->GetCurrentPattern(), sequencer->GetCurrentSong());
        ImGui::Separator();

        // Real-time channel data display
        if (ImGui::BeginTable("ChannelData", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            // Header
            ImGui::TableSetupColumn("Channel");
            ImGui::TableSetupColumn("Status");
            ImGui::TableSetupColumn("Instrument");
            ImGui::TableSetupColumn("Period");
            ImGui::TableSetupColumn("Volume");
            ImGui::TableSetupColumn("Note");
            ImGui::TableHeadersRow();

            // Show real-time data for each channel
            for (int ch = 0; ch < 4; ch++) {
                ImGui::TableNextRow();
                
                auto channelData = sequencer->GetChannelData(ch);
                
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("Ch%d", ch);
                
                ImGui::TableSetColumnIndex(1);
                if (channelData.active) {
                    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "ACTIVE");
                } else {
                    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "MUTED");
                }
                
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%s", sampleNames[channelData.instrument]);
                
                ImGui::TableSetColumnIndex(3);
                if (channelData.period > 0) {
                    ImGui::Text("%d", channelData.period);
                } else {
                    ImGui::Text("---");
                }
                
                ImGui::TableSetColumnIndex(4);
                if (channelData.volume > 0) {
                    ImGui::Text("%d", channelData.volume);
                } else {
                    ImGui::Text("---");
                }
                
                ImGui::TableSetColumnIndex(5);
                if (channelData.note > 0) {
                    ImGui::Text("%d", channelData.note);
                } else {
                    ImGui::Text("---");
                }
            }
            
            ImGui::EndTable();
        }

        // Current tick display
        ImGui::Spacing();
        ImGui::Text("Current Tick: %d", sequencer->GetCurrentTick());
        
        // Tempo info
        ImGui::Text("Tempo Multiplier: %.2fx", tempoMultiplier);

        ImGui::End();
    }
};

int main(int /*argc*/, char* /*argv*/[]) {
    std::cout << "Street Rod Music Player - ImGui Version" << std::endl;
    std::cout << "======================================" << std::endl;

    try {
        MusicPlayerGUI player;
        player.Run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}