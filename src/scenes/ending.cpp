#include "scenes/ending.h"

#include "core/game_text.h"
#include "garage/garage.h"
#include "ui/hud.h"
#include "core/mouse.h"

extern Mouse mouse;
#include "core/state.h"
#include "gfx/bitmap.h"
#include "parsers/libp.h"

namespace {

// srcX, srcY, w, h, dstX, dstY. Entries 1..19 are the girl (bottom-aligned in an 80x44 window);
// 20..28 are the kiss, at screen coordinates.
struct Frame {
    int sx, sy, w, h, dx, dy;
};
constexpr Frame kGirl[20] = {
    {  0,   0, 48, 24,  0, 0},   // entry 0 is never drawn
    { 48,   0, 32, 24,  8, 0},
    { 80,   0, 32, 24, 16, 0},
    {112,   0, 48, 24,  8, 0},
    {  0,  25, 40, 43, 16, 0},
    { 40,  25, 40, 43, 16, 0},
    { 80,  24, 40, 44, 16, 0},
    {120,  24, 40, 44, 16, 0},
    {  0,  68, 40, 44, 16, 0},
    { 40,  68, 40, 44, 16, 0},
    { 80,  68, 40, 44, 16, 0},
    {120,  68, 40, 44, 16, 0},
    {  0, 112, 40, 44, 16, 0},
    { 40, 112, 40, 44, 16, 0},
    { 80, 112, 40, 44, 16, 0},
    {120, 112, 40, 44, 16, 0},
    {  0, 156, 48, 44,  8, 0},
    { 48, 156, 56, 44,  0, 0},
    {104, 156, 56, 44,  0, 0},
    {160, 156, 40, 44, 16, 0},
};
constexpr Frame kKiss[9] = {
    {208, 54,   8,   2, 128,  84},
    {200, 54,   8,   3, 112,  88},
    {184, 54,  16,   4,  96,  94},
    {160, 54,  24,  10, 112, 113},
    {160, 86,  40,  10, 128, 113},
    {160, 65,  48,  20, 136, 104},
    {160,  1, 112,  52, 128,  70},
    {  0,  1, 160,  97, 112,  52},
    {  0, 99, 224, 101,  80,  51},
};

constexpr int kWinPicX = 8;      // LAB_0262(300, 8, 20, 44): picture at (8,20) over a fill of pen 44
constexpr int kWinPicY = 20;
constexpr int kWinFillPen = 44;   // EHB half of pen 12
constexpr int kWinX = 103;
constexpr int kWinY = 67;
constexpr int kWinH = 44;

constexpr Uint64 kFrameMs = 200;
constexpr int kBeatFrame = 13;   // this one is held twice as long
constexpr Uint64 kBeatExtraMs = 400;
constexpr Uint64 kLipsHoldMs = 1600;    // Delay(80) on the full lips
constexpr Uint64 kHeroDelayMs = 1000;   // Delay(50) after they are gone, before the line

}

Ending::Ending() : CarScene((Asset){LibAssetRegular, SrLibFileAmigaLibP, GARAGE_SCREEN}) {}

void Ending::OnSceneEnter() {
    CarScene::OnSceneEnter();
    phase = Phase::Win;
    frame = 1;
    phaseStart = GameTicks();
    heroBubble.reset();
    SetPointer(SrLibAmigaSpriteAssetMousePointerThumbsUp);
    mouse.SetVisible(true);
    hud->SetText("");

    SetAudioManager(state.GetAssets().GetAudioManager());

    AssetManager& assets = state.GetAssets();
    girl = assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, GIRLFRIEND_ANIMATION});
    kiss = assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, KISS_ANIMATION});
    winPic = assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, WIN_SCREEN});
    {
        BitmapParser parser;
        const Palette pal = parser.PreparePalette(
            assets.GetAssetPalette({LibAssetRegular, SrLibFileAmigaLibP, WIN_SCREEN}), true);
        winFill = pal[kWinFillPen];
    }

    paintCan = std::make_unique<Bitmap>(10, 12, 2);
    paintCan->SetPosition(233, 166);
    paintCan->SetVisible(true);
    DrawPaintCan(*paintCan, CarColorKing, false);
}

SDL_Point Ending::GetCarIdlePosition() {
    Opponent* king = state.GetKingOpponent();
    const int height = (king && king->car) ? king->car->GetModel().height : 0;
    return {SrLibQueryGetCarIdleX(SCENE_GARAGE),
            SrLibQueryGetCarRoofYOffset(SCENE_GARAGE, height)};
}

void Ending::OnCarAnimationComplete() {
    if (phase == Phase::DriveIn) {
        phase = Phase::Girl;
        frame = 1;
        phaseStart = GameTicks();
        if (kingCarBitmap) {
            kingCarBitmap->SetDriverVisible(false);
        }
    }
}

void Ending::DrawScene() {
    currentTime = GameTicks() / 1000.0;
    const double deltaTime = currentTime - lastFrameTime;
    lastFrameTime = currentTime;

    if (phase == Phase::Win) {
        SDL_SetRenderDrawColor(renderer, winFill.r, winFill.g, winFill.b, 255);
        SDL_Rect screen = {0, 0, GAME_WIDTH, GAME_HEIGHT};
        SDL_RenderFillRect(renderer, &screen);
        if (winPic != nullptr) {
            winPic->SetPosition(kWinPicX, kWinPicY);
            winPic->Draw();
        }
        hud->Draw();
        return;
    }

    UpdateCarAnimation(deltaTime);
    if (state.GetCurrentScene() != this) {
        return;   // UpdateCarAnimation can switch scenes under us
    }

    const Uint64 now = GameTicks();
    if (phase == Phase::Girl || phase == Phase::Kiss) {
        Uint64 due = kFrameMs;
        if (phase == Phase::Girl && frame == kBeatFrame) {
            due += kBeatExtraMs;
        }
        if (now - phaseStart >= due) {
            phaseStart = now;
            ++frame;
            if (phase == Phase::Girl && frame > 19) {
                girlFrame = 19;     // she stays standing there
                phase = Phase::Kiss;
                frame = 0;
            } else if (phase == Phase::Kiss && frame > 8) {
                frame = 8;          // hold the last, biggest frame
                phase = Phase::Done;
            }
        }
    }
    if (phase == Phase::Done && !heroBubble && now - phaseStart >= kLipsHoldMs + kHeroDelayMs) {
        heroBubble = std::make_unique<Bubble>(GameText::Msg(GameText::HERO), BUBBLE_LEFT, false);
        heroBubble->SetVisible(true);
    }

    CarScene::DrawScene();
    paintCan->Draw();

    if (girl != nullptr && phase != Phase::DriveIn) {
        const Frame& f = kGirl[phase == Phase::Girl ? frame : girlFrame];
        girl->DrawWithRects({f.sx, f.sy, f.w, f.h},
                            {kWinX + f.dx, kWinY + kWinH - f.h, f.w, f.h});
    }
    if (carBitmap != nullptr) {
        carBitmap->Draw();
    }
    const bool lipsShown = phase == Phase::Kiss ||
                           (phase == Phase::Done && now - phaseStart < kLipsHoldMs);
    if (lipsShown && kiss != nullptr) {
        const Frame& f = kKiss[frame];
        kiss->DrawWithRects({f.sx, f.sy, f.w, f.h}, {f.dx, f.dy, f.w, f.h});
    }
    if (heroBubble) {
        heroBubble->Draw();
    }
}

void Ending::OnFadeComplete() {
    CarScene::OnFadeComplete();
    if (phase == Phase::Leaving) {
        state.MoveToScene(SCENE_KING_OF_COUNTY);
    }
}

bool Ending::PushEvent(SDL_Event* event) {
    const bool pressed = event->type == SDL_KEYDOWN ||
                         event->type == SDL_MOUSEBUTTONDOWN ||
                         event->type == SDL_CONTROLLERBUTTONDOWN;
    if (pressed && phase == Phase::Win) {
        phase = Phase::DriveIn;
        phaseStart = GameTicks();
        SetPointer(SrLibAmigaSpriteAssetMousePointerHand);
        Opponent* king = state.GetKingOpponent();
        if (king != nullptr && king->car) {
            girlfriendDriver = king->driver;
            girlfriendDriver.spriteIndex = DRIVER_KINGS_GIRLFRIEND;
            girlfriendDriver.customDriverPath.clear();
            kingCarBitmap = std::make_unique<CarBitmap>(
                king->car, &girlfriendDriver,
                static_cast<StandardCarColor>(king->car->GetColor()));
            carBitmap = kingCarBitmap.get();
            // StartAnimateCar takes the roof offset for the current scene, which SCENE_ENDING lacks.
            needsNewAnimation = true;
            StartAnimateCar(king->car, &girlfriendDriver, CarAnimationType::SINGLE_CAR_DRIVE);
            cary = GetCarIdlePosition().y;
            carBitmap->Move({carx, cary});
        } else {
            phase = Phase::Girl;         // no car to drive in; get on with it
        }
        return true;
    }
    if (pressed && phase == Phase::Done) {
        if (!heroBubble) {
            return true;
        }
        phase = Phase::Leaving;
        FadeOut();
        return true;
    }
    if (pressed) {
        return true;    // the cutscene is not skippable until the last screen
    }
    return CarScene::PushEvent(event);
}
