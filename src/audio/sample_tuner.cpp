#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <cmath>

class SampleTuner {
private:
    SDL_AudioDeviceID audioDevice;
    SDL_AudioSpec audioSpec;
    SDL_Window* window;
    SDL_Renderer* renderer;
    std::vector<int8_t> waveData;
    std::vector<int8_t> currentSample;
    
    // Visual parameters
    static const int WINDOW_WIDTH = 1200;
    static const int WINDOW_HEIGHT = 600;
    static const int WAVEFORM_HEIGHT = 200;
    static const int WAVEFORM_Y = 50;
    static const int HANDLE_SIZE = 8;
    static const int SAMPLE_BOX_HEIGHT = 100;
    static const int SAMPLE_BOX_Y = 300;
    
    // Current tuning parameters
    uint32_t currentOffset = 0;
    uint32_t currentLength = 1000;
    uint32_t currentChannel = 0;
    bool playing = false;
    uint32_t playPosition = 0;
    uint32_t loopDelaySamples = 0;
    uint32_t loopDelayRemaining = 0;
    
    // Dragging state
    bool dragging = false;
    int dragHandle = -1; // -1 = none, 0-4 = sample start, 5-9 = sample end
    int dragStartX = 0;
    uint32_t dragStartOffset = 0;
    
    // Sample parameters for each channel
    struct SampleParams {
        uint32_t offset;
        uint32_t length;
        std::string name;
        SDL_Color color;
    };
    
    std::vector<SampleParams> samples = {
        {0x0000, 6700, "Sample 0 (lead melody)", {255, 100, 100, 255}},
        {0x1da6, 5700, "Sample 1 (electric bass)", {100, 255, 100, 255}},
        {0x396c, 3800, "Sample 2 (bassdrum)", {100, 100, 255, 255}},
        {0x4012, 4090, "Sample 3 (hihat)", {255, 255, 100, 255}},
        {0x4b46, 4500, "Sample 4 (electric guitar)", {255, 100, 255, 255}}
    };

public:
    SampleTuner() {
        if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO) < 0) {
            std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
            exit(1);
        }
        
        // Load wave data
        std::ifstream file("/home/user/kod/privat/streetrod-bins/amiga/SR_disk1/srl1.wave", std::ios::binary);
        if (!file) {
            std::cerr << "Failed to open srl1.wave" << std::endl;
            exit(1);
        }
        
        file.seekg(0, std::ios::end);
        size_t fileSize = file.tellg();
        file.seekg(0, std::ios::beg);
        
        waveData.resize(fileSize);
        file.read(reinterpret_cast<char*>(waveData.data()), fileSize);
        
        std::cout << "Loaded " << fileSize << " bytes from srl1.wave" << std::endl;
        
        // Set up audio
        SDL_AudioSpec desiredSpec;
        desiredSpec.freq = 22050;  // Lower frequency for better compatibility
        desiredSpec.format = AUDIO_S8;
        desiredSpec.channels = 1;
        desiredSpec.samples = 1024;
        desiredSpec.callback = audioCallback;
        desiredSpec.userdata = this;
        
        audioDevice = SDL_OpenAudioDevice(nullptr, 0, &desiredSpec, &audioSpec, 0);
        if (audioDevice == 0) {
            std::cerr << "Failed to open audio device: " << SDL_GetError() << std::endl;
            exit(1);
        }
        
        std::cout << "Audio initialized: " << audioSpec.freq << " Hz, " 
                  << (int)audioSpec.channels << " channels" << std::endl;
        
        // Calculate 0.5 second delay in samples
        loopDelaySamples = audioSpec.freq / 2; // 0.5 seconds
        
        // Create a window for visual editing
        window = SDL_CreateWindow("Sample Tuner - Visual Waveform Editor", 
                                 SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                 WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
        if (!window) {
            std::cerr << "Failed to create window: " << SDL_GetError() << std::endl;
            exit(1);
        }
        
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        if (!renderer) {
            std::cerr << "Failed to create renderer: " << SDL_GetError() << std::endl;
            exit(1);
        }
        
        // Start with first sample
        loadCurrentSample();
    }
    
    ~SampleTuner() {
        SDL_CloseAudioDevice(audioDevice);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
    }
    
    static void audioCallback(void* userdata, Uint8* stream, int len) {
        SampleTuner* tuner = static_cast<SampleTuner*>(userdata);
        tuner->fillAudioBuffer(stream, len);
    }
    
    void fillAudioBuffer(Uint8* stream, int len) {
        if (!playing || currentSample.empty()) {
            memset(stream, 0, len);
            return;
        }
        
        int8_t* output = reinterpret_cast<int8_t*>(stream);
        int samplesToWrite = len;
        
        for (int i = 0; i < samplesToWrite; i++) {
            if (loopDelayRemaining > 0) {
                // In delay period - output silence
                output[i] = 0;
                loopDelayRemaining--;
            } else if (playPosition < currentSample.size()) {
                // Playing sample
                output[i] = currentSample[playPosition];
                playPosition++;
            } else {
                // End of sample - start delay period
                playPosition = 0;
                loopDelayRemaining = loopDelaySamples;
                output[i] = 0;
            }
        }
    }
    
    void loadCurrentSample() {
        if (currentOffset + currentLength > waveData.size()) {
            currentLength = waveData.size() - currentOffset;
        }
        
        currentSample.resize(currentLength);
        for (uint32_t i = 0; i < currentLength; i++) {
            currentSample[i] = waveData[currentOffset + i];
        }
        
        playPosition = 0;
    }
    
    void play() {
        playPosition = 0;
        loopDelayRemaining = 0; // Reset delay when starting
        playing = true;
        SDL_PauseAudioDevice(audioDevice, 0);
    }
    
    void stop() {
        playing = false;
        SDL_PauseAudioDevice(audioDevice, 1);
    }
    
    void printStatus() {
        std::cout << "\n=== SAMPLE TUNER ===" << std::endl;
        std::cout << "Current: " << samples[currentChannel].name << std::endl;
        std::cout << "Offset: 0x" << std::hex << currentOffset << " (" << std::dec << currentOffset << ")" << std::endl;
        std::cout << "Length: " << currentLength << " bytes" << std::endl;
        std::cout << "Status: " << (playing ? "PLAYING" : "STOPPED") << std::endl;
        std::cout << "\nControls:" << std::endl;
        std::cout << "  SPACE - Play/Stop" << std::endl;
        std::cout << "  1-5   - Select sample (0-4)" << std::endl;
        std::cout << "  A/D   - Adjust offset (-/+100)" << std::endl;
        std::cout << "  S/W   - Adjust offset (-/+10)" << std::endl;
        std::cout << "  Q/E   - Adjust offset (-/+1)" << std::endl;
        std::cout << "  Z/X   - Adjust length (-/+100)" << std::endl;
        std::cout << "  C/V   - Adjust length (-/+10)" << std::endl;
        std::cout << "  F/G   - Adjust length (-/+1)" << std::endl;
        std::cout << "  R     - Reset to default" << std::endl;
        std::cout << "  P     - Print current values" << std::endl;
        std::cout << "  ESC   - Exit and print results" << std::endl;
    }
    
    void printResults() {
        std::cout << "\n=== FINAL RESULTS ===" << std::endl;
        for (size_t i = 0; i < samples.size(); i++) {
            std::cout << "Sample " << i << ": offset=0x" << std::hex << samples[i].offset 
                      << ", length=" << std::dec << samples[i].length 
                      << " (" << samples[i].name << ")" << std::endl;
        }
        
        std::cout << "\nC++ code:" << std::endl;
        std::cout << "instruments.push_back({0x" << std::hex << samples[0].offset << ", " << std::dec << samples[0].length << "}); // " << samples[0].name << std::endl;
        std::cout << "instruments.push_back({0x" << std::hex << samples[1].offset << ", " << std::dec << samples[1].length << "}); // " << samples[1].name << std::endl;
        std::cout << "instruments.push_back({0x" << std::hex << samples[2].offset << ", " << std::dec << samples[2].length << "}); // " << samples[2].name << std::endl;
        std::cout << "instruments.push_back({0x" << std::hex << samples[3].offset << ", " << std::dec << samples[3].length << "}); // " << samples[3].name << std::endl;
        std::cout << "instruments.push_back({0x" << std::hex << samples[4].offset << ", " << std::dec << samples[4].length << "}); // " << samples[4].name << std::endl;
    }
    
    // Convert file offset to screen X coordinate
    int offsetToX(uint32_t offset) {
        return 50 + (offset * (WINDOW_WIDTH - 100)) / waveData.size();
    }
    
    // Convert screen X coordinate to file offset
    uint32_t xToOffset(int x) {
        x = std::max(50, std::min(x, WINDOW_WIDTH - 50));
        return ((x - 50) * waveData.size()) / (WINDOW_WIDTH - 100);
    }
    
    // Draw waveform
    void drawWaveform() {
        // Clear background
        SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
        SDL_RenderClear(renderer);
        
        // Draw waveform
        SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
        for (int x = 50; x < WINDOW_WIDTH - 50; x++) {
            uint32_t offset = xToOffset(x);
            if (offset < waveData.size()) {
                int8_t sample = waveData[offset];
                int y = WAVEFORM_Y + WAVEFORM_HEIGHT/2 + (sample * WAVEFORM_HEIGHT / 512);
                SDL_RenderDrawPoint(renderer, x, y);
            }
        }
        
        // Draw sample regions
        for (size_t i = 0; i < samples.size(); i++) {
            const SampleParams& sample = samples[i];
            int startX = offsetToX(sample.offset);
            int endX = offsetToX(sample.offset + sample.length);
            
            // Draw sample region border only (no fill so waveform shows through)
            SDL_SetRenderDrawColor(renderer, sample.color.r, sample.color.g, sample.color.b, 255);
            SDL_Rect rect = {startX, WAVEFORM_Y, endX - startX, WAVEFORM_HEIGHT};
            SDL_RenderDrawRect(renderer, &rect);
            
            // Draw handles
            SDL_Rect startHandle = {startX - HANDLE_SIZE/2, WAVEFORM_Y - HANDLE_SIZE/2, HANDLE_SIZE, HANDLE_SIZE};
            SDL_Rect endHandle = {endX - HANDLE_SIZE/2, WAVEFORM_Y - HANDLE_SIZE/2, HANDLE_SIZE, HANDLE_SIZE};
            
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderFillRect(renderer, &startHandle);
            SDL_RenderFillRect(renderer, &endHandle);
            
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderDrawRect(renderer, &startHandle);
            SDL_RenderDrawRect(renderer, &endHandle);
        }
        
        // Draw sample boxes below
        for (size_t i = 0; i < samples.size(); i++) {
            const SampleParams& sample = samples[i];
            int startX = offsetToX(sample.offset);
            int endX = offsetToX(sample.offset + sample.length);
            
            // Sample box
            SDL_Rect box = {startX, SAMPLE_BOX_Y + static_cast<int>(i) * 20, endX - startX, 18};
            SDL_SetRenderDrawColor(renderer, sample.color.r, sample.color.g, sample.color.b, 100);
            SDL_RenderFillRect(renderer, &box);
            SDL_SetRenderDrawColor(renderer, sample.color.r, sample.color.g, sample.color.b, 255);
            SDL_RenderDrawRect(renderer, &box);
        }
        
        SDL_RenderPresent(renderer);
    }
    
    // Check if mouse is over a handle
    int getHandleAt(int x, int y) {
        for (size_t i = 0; i < samples.size(); i++) {
            const SampleParams& sample = samples[i];
            int startX = offsetToX(sample.offset);
            int endX = offsetToX(sample.offset + sample.length);
            
            // Check start handle
            if (x >= startX - HANDLE_SIZE/2 && x <= startX + HANDLE_SIZE/2 &&
                y >= WAVEFORM_Y - HANDLE_SIZE/2 && y <= WAVEFORM_Y + HANDLE_SIZE/2) {
                return i; // Start handle
            }
            
            // Check end handle
            if (x >= endX - HANDLE_SIZE/2 && x <= endX + HANDLE_SIZE/2 &&
                y >= WAVEFORM_Y - HANDLE_SIZE/2 && y <= WAVEFORM_Y + HANDLE_SIZE/2) {
                return i + 5; // End handle
            }
        }
        return -1;
    }
    
    void run() {
        std::cout << "\n=== VISUAL SAMPLE TUNER ===" << std::endl;
        std::cout << "Drag the white handles to adjust sample boundaries" << std::endl;
        std::cout << "SPACE - Play/Stop current sample" << std::endl;
        std::cout << "1-5 - Select sample to play" << std::endl;
        std::cout << "ESC - Exit and print results" << std::endl;
        std::cout << "=================================" << std::endl;
        
        bool running = true;
        while (running) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                switch (event.type) {
                    case SDL_QUIT:
                        running = false;
                        break;
                        
                    case SDL_MOUSEBUTTONDOWN:
                        if (event.button.button == SDL_BUTTON_LEFT) {
                            int handle = getHandleAt(event.button.x, event.button.y);
                            if (handle != -1) {
                                dragging = true;
                                dragHandle = handle;
                                dragStartX = event.button.x;
                                if (handle < 5) {
                                    dragStartOffset = samples[handle].offset;
                                } else {
                                    dragStartOffset = samples[handle - 5].offset + samples[handle - 5].length;
                                }
                            }
                        }
                        break;
                        
                    case SDL_MOUSEBUTTONUP:
                        if (event.button.button == SDL_BUTTON_LEFT) {
                            dragging = false;
                            dragHandle = -1;
                        }
                        break;
                        
                    case SDL_MOUSEMOTION:
                        if (dragging && dragHandle != -1) {
                            int deltaX = event.motion.x - dragStartX;
                            uint32_t newOffset = xToOffset(offsetToX(dragStartOffset) + deltaX);
                            
                            if (dragHandle < 5) {
                                // Dragging start handle
                                int sampleIndex = dragHandle;
                                uint32_t maxOffset = samples[sampleIndex].offset + samples[sampleIndex].length - 1;
                                newOffset = std::min(newOffset, maxOffset);
                                
                                if (sampleIndex > 0) {
                                    uint32_t prevEnd = samples[sampleIndex - 1].offset + samples[sampleIndex - 1].length;
                                    newOffset = std::max(newOffset, prevEnd);
                                }
                                
                                samples[sampleIndex].offset = newOffset;
                            } else {
                                // Dragging end handle
                                int sampleIndex = dragHandle - 5;
                                uint32_t minOffset = samples[sampleIndex].offset + 1;
                                newOffset = std::max(newOffset, minOffset);
                                
                                if (sampleIndex < static_cast<int>(samples.size()) - 1) {
                                    uint32_t nextStart = samples[sampleIndex + 1].offset;
                                    newOffset = std::min(newOffset, nextStart);
                                }
                                
                                samples[sampleIndex].length = newOffset - samples[sampleIndex].offset;
                            }
                            
                            // Update current sample if it's the one being edited
                            if (currentChannel == static_cast<uint32_t>(dragHandle < 5 ? dragHandle : dragHandle - 5)) {
                                currentOffset = samples[currentChannel].offset;
                                currentLength = samples[currentChannel].length;
                                loadCurrentSample();
                            }
                        }
                        break;
                        
                    case SDL_KEYDOWN:
                        switch (event.key.keysym.sym) {
                            case SDLK_ESCAPE:
                                running = false;
                                break;
                                
                            case SDLK_SPACE:
                                if (playing) {
                                    stop();
                                } else {
                                    play();
                                }
                                break;
                                
                            case SDLK_1:
                            case SDLK_2:
                            case SDLK_3:
                            case SDLK_4:
                            case SDLK_5: {
                                int sampleIndex = event.key.keysym.sym - SDLK_1;
                                if (sampleIndex < static_cast<int>(samples.size())) {
                                    currentChannel = sampleIndex;
                                    currentOffset = samples[currentChannel].offset;
                                    currentLength = samples[currentChannel].length;
                                    loadCurrentSample();
                                    stop();
                                }
                                break;
                            }
                        }
                        break;
                }
            }
            
            // Redraw the waveform
            drawWaveform();
            
            SDL_Delay(16); // ~60 FPS
        }
        
        stop();
        printResults();
    }
};

int main() {
    SampleTuner tuner;
    tuner.run();
    return 0;
}
