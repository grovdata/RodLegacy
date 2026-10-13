#include "debug/sfx_debug.h"

#include "core/state.h"

namespace {
constexpr uint32_t kWhite = 0xFFFFFFFF;

struct Entry {
    const char* name;
    AudioSampleType type;
};

const Entry kSounds[] = {
    {"Crash (wreck)",            AudioSampleType::CRASH},
    {"Car bash (garage clank)",  AudioSampleType::CAR_BASH},
    {"Car jack",                 AudioSampleType::CAR_JACK},
    {"Part mount",               AudioSampleType::PART_MOUNT},
    {"Hiss / ratchet",           AudioSampleType::HISS},
    {"Cutscene hit",             AudioSampleType::CUTSCENE_HIT},
    {"Tire screech",             AudioSampleType::TIRE_SCREECH},
    {"Cop siren",                AudioSampleType::COP_SIREN},
    {"Engine start",             AudioSampleType::ENGINE_START},
    {"Bay ignition",             AudioSampleType::BAY_IGNITION},
    {"Horn",                     AudioSampleType::HONK},
    {"Paint job",                AudioSampleType::PAINT_SPRAY},
    {"Tire change: air wrench",  AudioSampleType::TIRE_AIR_WRENCH},
    {"Tire change: putt",        AudioSampleType::TIRE_PUTT},
    {"Tire change: lug nuts",    AudioSampleType::TIRE_LUG_NUT},
    {"Reject beep",              AudioSampleType::REJECT_BEEP},
    {"Drivetrain blown",         AudioSampleType::DRIVETRAIN_BLOWN},
    {"Sticker",                  AudioSampleType::STICKER},
};
constexpr int kCount = sizeof(kSounds) / sizeof(kSounds[0]);
}

void SfxDebug::Init(bool debugMode) {
    Scene::Init(debugMode);

    struct Spec { SDL_Rect rect; const char* text; std::function<void()> click; };
    const Spec specs[] = {
        {{8, 150, 26, 16}, "<", [this] { Cycle(-1); }},
        {{38, 150, 26, 16}, ">", [this] { Cycle(1); }},
        {{70, 150, 56, 16}, "Amiga", [this] { Play(AudioManager::SfxSource::AMIGA); }},
        {{130, 150, 56, 16}, "PC spkr", [this] { Play(AudioManager::SfxSource::PCSPEAKER); }},
        {{190, 150, 56, 16}, "AdLib", [this] { Play(AudioManager::SfxSource::ADLIB); }},
        {{250, 150, 56, 16}, "C64", [this] { Play(AudioManager::SfxSource::C64); }},
    };
    buttons.clear();
    for (const Spec& s : specs) {
        ButtonConfig cfg{};
        cfg.bounds = s.rect;
        cfg.text = s.text;
        cfg.onClick = s.click;
        buttons.push_back(std::make_unique<Button>(cfg));
    }

    SFont_Font* font = state.GetAssets().GetMainFont();
    nameLabel.Init(font, 8, 60, kWhite);
    hintLabel.Init(font, 8, 120, kWhite);
    hintLabel.SetText("Left/Right: sound   A / P / D / C: play");
    RefreshLabels();
}

void SfxDebug::Cycle(int delta) {
    current = (current + delta + kCount) % kCount;
    RefreshLabels();
}

void SfxDebug::Play(AudioManager::SfxSource src) {
    if (AudioManager* am = state.GetAssets().GetAudioManager()) am->PlaySoundVia(src, kSounds[current].type);
}

void SfxDebug::RefreshLabels() {
    nameLabel.SetText(std::to_string(current + 1) + "/" + std::to_string(kCount) + "  " + kSounds[current].name);
}

void SfxDebug::DrawScene() {
    Scene::DrawScene();
    nameLabel.Draw();
    hintLabel.Draw();
    for (auto& b : buttons) b->Draw();
}

bool SfxDebug::PushEvent(SDL_Event* event) {
    if (event->type == SDL_KEYDOWN) {
        switch (event->key.keysym.sym) {
            case SDLK_LEFT:  Cycle(-1); return true;
            case SDLK_RIGHT: Cycle(1); return true;
            case SDLK_a:     Play(AudioManager::SfxSource::AMIGA); return true;
            case SDLK_p:     Play(AudioManager::SfxSource::PCSPEAKER); return true;
            case SDLK_d:     Play(AudioManager::SfxSource::ADLIB); return true;
            case SDLK_c:     Play(AudioManager::SfxSource::C64); return true;
            default: break;
        }
    }
    if (event->type == SDL_MOUSEBUTTONDOWN || event->type == SDL_MOUSEBUTTONUP) {
        for (auto& b : buttons) b->HandleEvent(event);
        return true;
    }
    return Scene::PushEvent(event);
}
