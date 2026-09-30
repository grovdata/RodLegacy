#include "scenes/intro.h"

#include "core/state.h"
#include "parsers/libp.h"

namespace {

constexpr Uint64 kTickMs = 20;        // one PAL vblank
constexpr int kMaxTicksPerUpdate = 10;

// The original holds the plate while loading, so there is no fixed length to copy.
constexpr int kPlateHoldTicks = 150;

}

void Intro::Init(bool debugMode) {
    Scene::Init(debugMode);

    BitmapParser bp;
    std::vector<uint8_t>& lib = state.GetAssets().GetLibStartData();
    const auto entries = bp.ReadEntries(lib);
    for (int i = 0; i < static_cast<int>(images.size()) && i < entries.first; i++) {
        const BitmapEntry_t& e = entries.second[i];
        images[i].w = e.width;
        images[i].h = e.height;
        images[i].px = bp.PlanarToIndices(bp.ReadImage(lib, e, i, entries.first), e);
    }

    frame = sdl2::TexturePtr(SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                               SDL_TEXTUREACCESS_STREAMING,
                                               IntroPlayer::kWidth, IntroPlayer::kHeight));
}

void Intro::OnSceneEnter() {
    player.Start(images);
    tickAccumMs = 0;
    plateTicks = 0;
    leaving = false;
    musicStarted = false;
    // Music starts at step 50 of the title animation, not before.
    if (AudioManager* am = state.GetAssets().GetAudioManager()) {
        am->StopMusicPlayback();
    }
}

void Intro::Update(Uint64 deltaTime) {
    Scene::Update(deltaTime);
    if (leaving) return;

    tickAccumMs += deltaTime;
    int ticks = 0;
    std::vector<IntroSound> sounds;
    while (tickAccumMs >= kTickMs && ticks++ < kMaxTicksPerUpdate) {
        tickAccumMs -= kTickMs;
        if (player.Done()) {
            if (++plateTicks >= kPlateHoldTicks) Leave();
        } else {
            player.Tick(sounds);
        }
    }
    if (ticks > kMaxTicksPerUpdate) tickAccumMs = 0;   // a long stall; do not race to catch up
    for (IntroSound s : sounds) PlaySound(s);
}

void Intro::PlaySound(IntroSound sound) {
    AudioManager* am = state.GetAssets().GetAudioManager();
    if (am == nullptr) return;

    auto chime = [am](int chA, int periodA, int chB) { am->PlayChime(chA, periodA, chB); };
    auto hit = [am](int param) {
        am->PlaySound(AudioSampleType::CUTSCENE_HIT, SRL0_CUTSCENE_HIT_VOLUME / 64.0f,
                      static_cast<int>(3579545u / (param + SRL0_CUTSCENE_HIT_PERIOD_BIAS)));
    };
    switch (sound) {
        case IntroSound::ChimeA: chime(0, 0x96, 3); break;
        case IntroSound::ChimeB: chime(0, 0x82, 1); break;
        case IntroSound::ChimeC: chime(1, 0xa5, 2); break;
        case IntroSound::Ignition: {
            std::uniform_int_distribution<int> spread(0, SRL0_BAY_IGNITION_PERIOD_SPREAD - 1);
            const int period = SRL0_BAY_IGNITION_PERIOD_BASE + spread(RandomEngine());
            am->PlaySound(AudioSampleType::BAY_IGNITION, SRL0_BAY_IGNITION_VOLUME / 64.0f,
                          static_cast<int>(3579545u / period));
            break;
        }
        case IntroSound::Hit100: hit(100); break;
        case IntroSound::Hit200: hit(200); break;
        case IntroSound::Hit150: hit(150); break;
        case IntroSound::Screech: am->PlaySound(AudioSampleType::TIRE_SCREECH, 0x30 / 64.0f); break;
        case IntroSound::StartMusic:
            musicStarted = true;
            am->RestartMusic();
            break;
    }
}

void Intro::DrawScene() {
    const Palette colors = BitmapParser().PreparePalette(
        std::vector<uint16_t>(player.Palette().begin(), player.Palette().end()), true);

    void* pixels = nullptr;
    int pitch = 0;
    if (SDL_LockTexture(frame.get(), nullptr, &pixels, &pitch) != 0) return;
    const uint8_t* src = player.Front();
    for (int y = 0; y < IntroPlayer::kHeight; y++) {
        auto* row = reinterpret_cast<SrLibColor*>(static_cast<uint8_t*>(pixels) + y * pitch);
        for (int x = 0; x < IntroPlayer::kWidth; x++) {
            row[x] = colors[src[y * IntroPlayer::kWidth + x] & 63];
        }
    }
    SDL_UnlockTexture(frame.get());
    SDL_RenderCopy(renderer, frame.get(), nullptr, nullptr);
}

void Intro::Leave() {
    if (leaving) return;
    leaving = true;
    if (!musicStarted) PlaySound(IntroSound::StartMusic);
    FadeOut();
}

void Intro::OnFadeComplete() {
    Scene::OnFadeComplete();
    if (leaving) {
        state.MoveToScene(SCENE_NAME_ENTRY);
    }
}

bool Intro::PushEvent(SDL_Event* event) {
    const bool isKey = event->type == SDL_KEYDOWN || event->type == SDL_KEYUP;
    const SDL_Keycode sym = event->key.keysym.sym;
    if (isKey && (sym == SDLK_ESCAPE || (sym >= SDLK_F1 && sym <= SDLK_F12))) {
        return false;
    }
    const bool key = event->type == SDL_KEYDOWN && event->key.repeat == 0;
    const bool pressed = key || event->type == SDL_MOUSEBUTTONDOWN ||
                         event->type == SDL_CONTROLLERBUTTONDOWN;
    // Deliberate: the original only lets you skip the title animation.
    if (pressed) Leave();
    return isKey || pressed;
}
