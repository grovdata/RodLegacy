#include "scenes/ending.h"

#include "core/state.h"
#include "gfx/bitmap.h"
#include "parsers/libp.h"

namespace {

// The original's frame table, 29 entries of 6 bytes at A4-0x3e0e (static
// 0x24afca): srcX, srcY, w, h, dstX, dstY. Entries 1..19 are the girl getting
// out (dstY unused, they bottom-align in an 80x44 window); 20..28 are the kiss
// growing at the camera and use dstX/dstY as screen coordinates.
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

// Where the girl animation is composited and lands.
constexpr int kWinX = 103;
constexpr int kWinY = 67;
constexpr int kWinH = 44;

// Delay(10) / Delay(20) in dos.library ticks, 1 tick = 1/50 s.
constexpr Uint64 kFrameMs = 200;
constexpr int kBeatFrame = 13;   // this one is held twice as long
constexpr Uint64 kBeatExtraMs = 400;

}  // namespace

Ending::Ending() : CarScene((Asset){LibAssetRegular, SrLibFileAmigaLibP, GARAGE_SCREEN}) {}

void Ending::OnSceneEnter() {
    CarScene::OnSceneEnter();
    phase = Phase::Win;
    frame = 1;
    phaseStart = GameTicks();

    // The original starts this back in ProcessRaceOutcome, on the result
    // screen; the scene opening is the closest point we have to that.
    SetAudioManager(state.GetAssets().GetAudioManager());
    if (audioManager != nullptr) {
        audioManager->PlayVictoryMusic();
    }

    AssetManager& assets = state.GetAssets();
    girl = assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, GIRLFRIEND_ANIMATION});
    kiss = assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, KISS_ANIMATION});
    winPic = assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, WIN_SCREEN});
}

// The King parks where any car parks in the garage.
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
        // She is the one getting out, so the portrait behind the windscreen
        // goes the moment her animation starts.
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
        // 299 is a full-screen picture with its own text, so it stands alone.
        if (winPic != nullptr) {
            winPic->SetPosition(0, 0);
            winPic->Draw();
        }
        return;
    }

    UpdateCarAnimation(deltaTime);
    if (state.GetCurrentScene() != this) {
        return;   // UpdateCarAnimation can switch scenes under us
    }

    // Frame pacing for whichever animation is running.
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

    CarScene::DrawScene();   // the garage

    // She is getting out of the car, so she is behind it. Once the animation
    // is done she stays on her last frame for the rest of the scene.
    if (girl != nullptr && phase != Phase::DriveIn) {
        const Frame& f = kGirl[phase == Phase::Girl ? frame : girlFrame];
        girl->DrawWithRects({f.sx, f.sy, f.w, f.h},
                            {kWinX + f.dx, kWinY + kWinH - f.h, f.w, f.h});
    }
    if (carBitmap != nullptr) {
        carBitmap->Draw();
    }
    if ((phase == Phase::Kiss || phase == Phase::Done || phase == Phase::Leaving) &&
        kiss != nullptr) {
        const Frame& f = kKiss[frame];
        kiss->DrawWithRects({f.sx, f.sy, f.w, f.h}, {f.dx, f.dy, f.w, f.h});
    }
}

void Ending::OnFadeComplete() {
    CarScene::OnFadeComplete();
    if (phase == Phase::Leaving) {
        state.MoveToScene(SCENE_KING_OF_COUNTY);
    }
}

bool Ending::PushEvent(SDL_Event* event) {
    // The cutscene has no cursor, so a pad has only its buttons to advance with.
    const bool pressed = event->type == SDL_KEYDOWN ||
                         event->type == SDL_MOUSEBUTTONDOWN ||
                         event->type == SDL_CONTROLLERBUTTONDOWN;
    if (pressed && phase == Phase::Win) {
        phase = Phase::DriveIn;
        phaseStart = GameTicks();
        Opponent* king = state.GetKingOpponent();
        if (king != nullptr && king->car) {
            girlfriendDriver = king->driver;
            girlfriendDriver.spriteIndex = DRIVER_KINGS_GIRLFRIEND;
            girlfriendDriver.customDriverPath.clear();
            kingCarBitmap = std::make_unique<CarBitmap>(
                king->car, &girlfriendDriver,
                static_cast<StandardCarColor>(king->car->GetColor()));
            carBitmap = kingCarBitmap.get();
            // StartAnimateCar does the placing, but it takes the roof offset
            // for the *current* scene, and SCENE_ENDING is not in that table -
            // so put the car back on the garage floor afterwards.
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
        phase = Phase::Leaving;
        FadeOut();
        return true;
    }
    if (pressed) {
        return true;    // the cutscene is not skippable until the last screen
    }
    return CarScene::PushEvent(event);
}
