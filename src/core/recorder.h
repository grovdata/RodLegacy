#pragma once

#include <SDL.h>
#include <string>

// F6 pipes the rendered frames to ffmpeg on PATH. Each audio device copies its output into record_tap, and stopping
// mixes those over the video into recording_N.mp4. Without ffmpeg it only shows a notice.
namespace recorder {

void Toggle(SDL_Renderer* renderer);
// Call before present: captures the frame, then draws the notice so it stays out of the video.
void Frame(SDL_Renderer* renderer);
void Stop();
// A line of text in the top left for a couple of seconds, drawn by Frame.
void ShowNotice(const std::string& text);

}  // namespace recorder
