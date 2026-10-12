#include "core/recorder.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <csignal>
#endif

#include "audio/record_tap.h"
#include "core/global.h"
#include "core/log.h"
#include "core/state.h"
#include "gfx/SFont.h"
#include "gfx/bitmap.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

// A line of text in the top left for a couple of seconds, drawn after the frame is captured.
std::string noticeText;
Uint64 noticeUntil = 0;

void ShowNotice(const std::string& text) {
    noticeText = text;
    noticeUntil = SDL_GetTicks64() + 2500;
}

void DrawNotice() {
    static Bitmap* bmp = nullptr;
    static std::string shown;
    if (SDL_GetTicks64() >= noticeUntil) return;
    if (!bmp || shown != noticeText) {
        SFont_Font* font = state.GetAssets().GetMainFont();
        if (!font) return;
        SDL_Surface* s = SDL_CreateRGBSurface(0, SFont_TextWidth(font, noticeText.c_str()) + 6,
                                              SFont_TextHeight(font) + 4, 32,
                                              0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
        if (!s) return;
        SDL_FillRect(s, nullptr, SDL_MapRGBA(s->format, 0, 0, 0, 180));
        SFont_Write(s, font, 3, 2, noticeText.c_str());
        SDL_SetSurfaceBlendMode(s, SDL_BLENDMODE_NONE);  // LoadSurface copies by blit
        delete bmp;
        bmp = new Bitmap();
        bmp->LoadSurface(s);
        bmp->SetPosition(4, 4);
        bmp->SetVisible(true);
        SDL_FreeSurface(s);
        shown = noticeText;
    }
    bmp->Draw();
}

// ffmpeg runs hidden: popen on POSIX, CreateProcess on Windows, where _popen from a GUI program hangs.
#ifdef _WIN32
HANDLE recordProcess = nullptr;
HANDLE recordStdin = nullptr;

bool StartHidden(std::string cmd, HANDLE stdinRead, PROCESS_INFORMATION& pi) {
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    if (stdinRead) {
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = stdinRead;
    }
    return CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, stdinRead != nullptr, CREATE_NO_WINDOW,
                          nullptr, nullptr, &si, &pi) != 0;
}

bool FfmpegAvailable() {
    char path[MAX_PATH];
    return SearchPathA(nullptr, "ffmpeg.exe", nullptr, MAX_PATH, path, nullptr) != 0;
}

bool OpenFfmpeg(const std::string& cmd) {
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE rd = nullptr;
    if (!CreatePipe(&rd, &recordStdin, &sa, 0)) return false;
    SetHandleInformation(recordStdin, HANDLE_FLAG_INHERIT, 0);
    PROCESS_INFORMATION pi{};
    const bool ok = StartHidden(cmd, rd, pi);
    CloseHandle(rd);
    if (!ok) {
        CloseHandle(recordStdin);
        recordStdin = nullptr;
        return false;
    }
    CloseHandle(pi.hThread);
    recordProcess = pi.hProcess;
    return true;
}

bool WriteFfmpeg(const void* data, size_t n) {
    const char* p = static_cast<const char*>(data);
    while (n > 0) {
        DWORD wrote = 0;
        if (!WriteFile(recordStdin, p, static_cast<DWORD>(n), &wrote, nullptr) || wrote == 0) return false;
        p += wrote;
        n -= wrote;
    }
    return true;
}

void CloseFfmpeg() {
    CloseHandle(recordStdin);
    recordStdin = nullptr;
    WaitForSingleObject(recordProcess, INFINITE);
    CloseHandle(recordProcess);
    recordProcess = nullptr;
}

bool RunFfmpeg(const std::string& cmd) {
    PROCESS_INFORMATION pi{};
    if (!StartHidden(cmd, nullptr, pi)) return false;
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return code == 0;
}

bool FfmpegOpen() { return recordStdin != nullptr; }
#else
FILE* recordPipe = nullptr;

bool FfmpegAvailable() { return std::system("ffmpeg -version >/dev/null 2>&1") == 0; }

bool OpenFfmpeg(const std::string& cmd) {
    signal(SIGPIPE, SIG_IGN);
    recordPipe = popen(cmd.c_str(), "w");
    return recordPipe != nullptr;
}

bool WriteFfmpeg(const void* data, size_t n) { return fwrite(data, 1, n, recordPipe) == n; }

void CloseFfmpeg() {
    pclose(recordPipe);
    recordPipe = nullptr;
}

bool RunFfmpeg(const std::string& cmd) { return std::system(cmd.c_str()) == 0; }

bool FfmpegOpen() { return recordPipe != nullptr; }
#endif

int recordW = 0;
int recordH = 0;
int recordStep = 1;   // window pixels per piped pixel
std::vector<Uint8> recordFrame;
std::vector<Uint8> recordOut;
std::string recordName;

void StopRecording() {
    if (!FfmpegOpen()) return;
    CloseFfmpeg();
    const std::vector<record_tap::Source> audio = record_tap::Stop();

    const std::string video = recordName + ".video.mp4";
    std::string mux = "ffmpeg -nostdin -loglevel error -y -i " + video;
    for (const auto& a : audio) {
        mux += " -f s16le -ar " + std::to_string(a.freq) + " -ac " + std::to_string(a.channels) + " -i " + a.path;
    }
    mux += " -filter_complex \"amix=inputs=" + std::to_string(audio.size()) +
           ":normalize=0:duration=longest[a]\" -map 0:v -map \"[a]\" -c:v copy -c:a aac -b:a 192k -shortest -movflags +faststart " +
           recordName + ".mp4";
    std::error_code ec;
    if (!audio.empty() && RunFfmpeg(mux)) {
        fs::remove(video, ec);
    } else {
        fs::rename(video, recordName + ".mp4", ec);   // keep the picture at least
    }
    for (const auto& a : audio) fs::remove(a.path, ec);
    LOG_INFO("Recording saved: %s.mp4", recordName.c_str());
    ShowNotice("Saved " + recordName + ".mp4");
}

void StartRecording(SDL_Renderer* renderer) {
    if (!FfmpegAvailable()) {
        LOG_INFO("Recording needs ffmpeg installed");
        ShowNotice("Recording needs ffmpeg installed");
        return;
    }
    SDL_GetRendererOutputSize(renderer, &recordW, &recordH);
    int n = 1;
    while (fs::exists("recording_" + std::to_string(n) + ".mp4")) n++;
    recordName = "recording_" + std::to_string(n);
    // Thin the window to the nearest whole multiple of the output in the game, so the pipe carries a fraction of
    // it; ffmpeg's nearest-neighbour scale only has work to do when the window is not a clean multiple.
    const int outW = GAME_WIDTH * state.GetAssets().GetRecordScale();
    const int outH = GAME_HEIGHT * state.GetAssets().GetRecordScale();
    recordStep = std::max(1, std::min(recordW / outW, recordH / outH));
    const int pipeW = recordW / recordStep;
    const int pipeH = recordH / recordStep;
    const std::string cmd = "ffmpeg -loglevel error -y -f rawvideo -pix_fmt bgra -s " +
                            std::to_string(pipeW) + "x" + std::to_string(pipeH) +
                            " -use_wallclock_as_timestamps 1 -i - -fps_mode cfr -r 60 -vf scale=" +
                            std::to_string(outW) + ":" + std::to_string(outH) + ":flags=neighbor"
                            " -c:v libx264 -preset medium -crf 18 -pix_fmt yuv420p -movflags +faststart " +
                            recordName + ".video.mp4";
    if (!OpenFfmpeg(cmd)) {
        LOG_INFO("Could not start ffmpeg");
        ShowNotice("Could not start ffmpeg");
        return;
    }
    recordFrame.resize(static_cast<size_t>(recordW) * recordH * 4);
    recordOut.resize(static_cast<size_t>(pipeW) * pipeH * 4);
    record_tap::Start(recordName);
    LOG_INFO("Recording to %s.mp4", recordName.c_str());
    ShowNotice("Recording (F6 to stop)");
}

void Capture(SDL_Renderer* renderer) {
    if (!FfmpegOpen()) return;
    int w = 0, h = 0;
    SDL_GetRendererOutputSize(renderer, &w, &h);
    if (w != recordW || h != recordH) {
        StopRecording();   // the window changed size; ffmpeg's input size is fixed
        return;
    }
    if (SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_ARGB8888, recordFrame.data(), w * 4) != 0) {
        StopRecording();
        return;
    }
    const int pipeW = recordW / recordStep;
    const int pipeH = recordH / recordStep;
    for (int y = 0; y < pipeH; ++y) {
        const auto* src = reinterpret_cast<const Uint32*>(recordFrame.data()) + static_cast<size_t>(y) * recordStep * w;
        auto* dst = reinterpret_cast<Uint32*>(recordOut.data()) + static_cast<size_t>(y) * pipeW;
        for (int x = 0; x < pipeW; ++x) dst[x] = src[x * recordStep];
    }
    if (!WriteFfmpeg(recordOut.data(), recordOut.size())) StopRecording();
}

}  // namespace

namespace recorder {

void Toggle(SDL_Renderer* renderer) {
    if (FfmpegOpen()) {
        StopRecording();
    } else {
        StartRecording(renderer);
    }
}

void Frame(SDL_Renderer* renderer) {
    Capture(renderer);
    DrawNotice();
}

void Stop() { StopRecording(); }

void ShowNotice(const std::string& text) { ::ShowNotice(text); }

}  // namespace recorder
