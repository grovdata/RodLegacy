#include "scenes/sr2_ending.h"

#include "core/state.h"
#include "parsers/libp.h"

namespace {

constexpr Uint64 kTickMs = 20;           // one PAL vblank
constexpr int kMaxTicksPerUpdate = 10;
constexpr int kWidth = 320;
constexpr int kHeight = 200;
constexpr int kBackdrop = 33;
constexpr int kPanel = 32;
constexpr int kPeople = 34;
constexpr int kCars = 35;
constexpr int kPalette = 8;
constexpr int kLastLibStartAsset = 35;

// Each frame lasts 10 vblanks whatever its hold byte says; 255 marks the frame after which the car drives off.
constexpr int kFrameTicks = 10;
constexpr int kDriveOffHold = 255;
constexpr int kTextTicks = 750;          // or any key
constexpr int kEndHoldTicks = 80;
constexpr int kTextFillPen = 44;
constexpr SDL_Point kPanelPos = {8, 20};
// Clean ground in the car sheet, copied over the parked car in the backdrop when it drives off.
constexpr SDL_Rect kGroundPatchSrc = {96, 104, 136, 46};
constexpr SDL_Point kGroundPatchDst = {136, 146};

}

void Sr2Ending::Init(bool debugMode) {
    Scene::Init(debugMode);

    AssetManager& assets = state.GetAssets();
    const auto& palettes = assets.GetGamePalettes();
    if (assets.GetGame() != SrGame::SR2 || (int)palettes.size() <= kPalette ||
        !Sr2EndParse(assets.GetSr2End(), script)) {
        return;
    }
    palette = palettes[kPalette];

    BitmapParser bp;
    std::vector<uint8_t>& lib = assets.GetLibStartData();
    const auto entries = bp.ReadEntries(lib);
    if (entries.first <= kLastLibStartAsset) return;

    auto load = [&](int i, Image& img) {
        const BitmapEntry_t& e = entries.second[i];
        img.w = e.width;
        img.h = e.height;
        img.px = bp.PlanarToIndices(bp.ReadImage(lib, e, i, entries.first), e);
    };
    load(kBackdrop, backdrop);
    load(kPanel, panel);
    load(kPeople, people);
    load(kCars, cars);

    texture320 = sdl2::TexturePtr(SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                                    SDL_TEXTUREACCESS_STREAMING, kWidth, kHeight));
    ready = true;
}

void Sr2Ending::OnSceneEnter() {
    phase = Phase::Text;
    frame = 0;
    ticksLeft = kTextTicks;
    carSheet = false;
    stage = backdrop.px;
    tickAccumMs = 0;
    if (AudioManager* am = state.GetAssets().GetAudioManager()) {
        am->PlayVictoryMusic();
    }
}

void Sr2Ending::StartAnimation() {
    phase = Phase::Animate;
    frame = 0;
    ticksLeft = kFrameTicks;
}

void Sr2Ending::Leave() {
    phase = Phase::Leaving;
    FadeOut();
}

void Sr2Ending::Tick() {
    if (--ticksLeft > 0) return;

    if (phase == Phase::Text) {
        StartAnimation();
    } else if (phase == Phase::Animate) {
        if (script.frames[frame].hold == kDriveOffHold) {
            const Image& c = cars;
            for (int y = 0; y < kGroundPatchSrc.h; y++) {
                for (int x = 0; x < kGroundPatchSrc.w; x++) {
                    stage[(kGroundPatchDst.y + y) * kWidth + kGroundPatchDst.x + x] =
                        c.px[(kGroundPatchSrc.y + y) * c.w + kGroundPatchSrc.x + x];
                }
            }
            carSheet = true;
        }
        if (frame + 1 >= script.frames.size()) {
            phase = Phase::Hold;
            ticksLeft = kEndHoldTicks;
        } else {
            ++frame;
            ticksLeft = kFrameTicks;
        }
    } else if (phase == Phase::Hold) {
        Leave();
    }
}

void Sr2Ending::Update(Uint64 deltaTime) {
    Scene::Update(deltaTime);
    if (!ready || phase == Phase::Leaving) return;

    tickAccumMs += deltaTime;
    int ticks = 0;
    while (tickAccumMs >= kTickMs && ticks++ < kMaxTicksPerUpdate && phase != Phase::Leaving) {
        tickAccumMs -= kTickMs;
        Tick();
    }
    if (ticks > kMaxTicksPerUpdate) tickAccumMs = 0;
}

void Sr2Ending::Compose() {
    auto blit = [&](const Image& src, int sx, int sy, int w, int h, int dx, int dy, bool keyed) {
        for (int y = 0; y < h; y++) {
            const int py = dy + y;
            if (py < 0 || py >= kHeight || sy + y >= src.h) continue;
            for (int x = 0; x < w; x++) {
                const int px = dx + x;
                if (px < 0 || px >= kWidth || sx + x >= src.w) continue;
                const uint8_t v = src.px[(sy + y) * src.w + sx + x];
                if (v != 0 || !keyed) screen[py * kWidth + px] = v;
            }
        }
    };

    if (phase == Phase::Text) {
        screen.fill(kTextFillPen);
        blit(panel, 0, 0, panel.w, panel.h, kPanelPos.x, kPanelPos.y, false);
        return;
    }

    std::copy(stage.begin(), stage.end(), screen.begin());
    const Image& sheet = carSheet ? cars : people;
    for (const Sr2EndSprite& s : script.frames[frame].sprites) {
        const Sr2EndRect& r = script.rects[s.rect];
        blit(sheet, r.x, r.y, r.w, r.h, s.x, s.y, true);
    }
}

void Sr2Ending::DrawScene() {
    if (!ready) return;
    Compose();

    const Palette colors = BitmapParser().PreparePalette(palette, true);
    void* pixels = nullptr;
    int pitch = 0;
    if (SDL_LockTexture(texture320.get(), nullptr, &pixels, &pitch) != 0) return;
    for (int y = 0; y < kHeight; y++) {
        auto* row = reinterpret_cast<SrLibColor*>(static_cast<uint8_t*>(pixels) + y * pitch);
        for (int x = 0; x < kWidth; x++) {
            row[x] = colors[screen[y * kWidth + x] & 63];
        }
    }
    SDL_UnlockTexture(texture320.get());
    SDL_RenderCopy(renderer, texture320.get(), nullptr, nullptr);
}

bool Sr2Ending::PushEvent(SDL_Event* event) {
    const bool isKey = event->type == SDL_KEYDOWN || event->type == SDL_KEYUP;
    const SDL_Keycode sym = event->key.keysym.sym;
    if (isKey && (sym == SDLK_ESCAPE || (sym >= SDLK_F1 && sym <= SDLK_F12))) {
        return false;
    }
    const bool pressed = (event->type == SDL_KEYDOWN && event->key.repeat == 0) ||
                         event->type == SDL_MOUSEBUTTONDOWN ||
                         event->type == SDL_CONTROLLERBUTTONDOWN;
    if (pressed) {
        if (phase == Phase::Text && ready) {
            StartAnimation();
        } else if (phase != Phase::Leaving) {
            Leave();
        }
    }
    return isKey || pressed;
}

void Sr2Ending::OnFadeComplete() {
    Scene::OnFadeComplete();
    if (phase == Phase::Leaving) {
        state.MoveToScene(SCENE_KING_OF_COUNTY);
    }
}
