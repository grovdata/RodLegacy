#include "garage/garage.h"
#include "core/state.h"
#include "ui/hud.h"
#include "ui/bubble.h"
#include "ui/checkbox.h"
#include "core/assets.h"
#include "core/state.h"
#include "car/engine_bay.h"
#include "core/embedded_loader.h"
#include "audio/audio_manager.h"
#include "car/engine_part.h"
#include "parsers/libp.h"
#include <SDL_image.h>
#include <algorithm>

void Garage::Sr2TireRoll(bool on) {
    if (!audioManager || on == sr2TireRolling) return;
    sr2TireRolling = on;
    if (on) audioManager->StartTireRollLoop();
    else audioManager->StopTireRollLoop();
}

void Garage::Sr2JackCrankSound(bool up, int crank) {
    if (!audioManager) return;
    const int period = up ? SR2_JACK_UP_PERIOD - SR2_JACK_PERIOD_STEP * crank
                          : SR2_JACK_DOWN_PERIOD + SR2_JACK_PERIOD_STEP * crank;
    audioManager->PlaySound(AudioSampleType::HISS, SR2_JACK_CRANK_VOLUME / 64.0f, 3579545 / period);
}

void Garage::StartTireChangeAnimation(const Part& newTire) {
    if (!carBitmap) return;
    pendingTire = newTire;
    tireChangePhase = TireChangePhase::JACK_SLIDE_UP;
    tireChangeTimer = 0.0f;
    initialFrontWheelPos = carBitmap->GetFrontWheelPos();
    initialRearWheelPos  = carBitmap->GetRearWheelPos();
    animFrontWheelX = (float)initialFrontWheelPos.x;
    animRearWheelX  = (float)initialRearWheelPos.x;
    animFrontWheelY = (float)initialFrontWheelPos.y;
    animRearWheelY  = (float)initialRearWheelPos.y;
    tireChangeLiftY = 0;
    jackStartY = state.GetAssets().GetGame() == SrGame::SR2 ? 157.0f
                                                             : (float)(initialRearWheelPos.y + initialRearWheelPos.h);
    jackYPos   = jackStartY;
    carBitmap->SetWheelsVisible(false);
    carBitmap->SetWheelhousesVisible(true);
    carBitmap->ResetWheelFrame();
    if (jackAnim) {
        jackAnim->SetFrame(0);
        jackAnim->Stop();
    }
}

void Garage::UpdateTireChangeAnimation(double deltaTime) {
    if (tireChangePhase == TireChangePhase::IDLE) return;

    constexpr float JACK_SLIDE_DUR   = 0.5f;
    constexpr float CRANK_ANIM_DUR   = 0.36f; // 3 frames x 0.12s each
    constexpr float CRANK_PAUSE_DUR  = 0.15f;
    constexpr float CRANK_TOTAL_DUR  = CRANK_ANIM_DUR + CRANK_PAUSE_DUR;
    constexpr float WHEEL_DROP_DUR   = 0.3f;
    constexpr float WHEEL_ROLL_SPEED = 100.0f;
    constexpr float WHEEL_RISE_DUR   = 0.3f;
    const int   wheelSize    = 31;
    const float rearRollDur  = (initialRearWheelPos.x  + wheelSize) / WHEEL_ROLL_SPEED;
    const float frontRollDur = (initialFrontWheelPos.x + wheelSize) / WHEEL_ROLL_SPEED;

    tireChangeTimer += (float)deltaTime;
    const bool sr2 = state.GetAssets().GetGame() == SrGame::SR2;

    auto lerp = [](float a, float b, float t) {
        return a + (b - a) * t;
    };
    auto clamp01 = [](float t) { return t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t); };

    switch (tireChangePhase) {

        case TireChangePhase::JACK_SLIDE_UP: {
            float t = clamp01(tireChangeTimer / JACK_SLIDE_DUR);
            jackYPos = lerp(jackStartY, 123.0f, t);
            if (jackAnim) jackAnim->SetFrame(0);
            if (tireChangeTimer >= JACK_SLIDE_DUR) {
                jackYPos = 123.0f;
                tireChangeTimer = 0.0f;
                tireChangePhase = TireChangePhase::JACK_CRANK_UP;
                if (jackAnim) jackAnim->SetFrame(0);
                lastJackCrankSounded = -1;
            }
            break;
        }

        case TireChangePhase::JACK_CRANK_UP: {
            int crankNum   = (int)(tireChangeTimer / CRANK_TOTAL_DUR);
            float localT   = tireChangeTimer - crankNum * CRANK_TOTAL_DUR;
            crankNum = std::min(crankNum, JACK_CRANKS - 1);

            if (crankNum != lastJackCrankSounded && localT < CRANK_ANIM_DUR) {
                if (sr2) Sr2JackCrankSound(true, crankNum);
                else if (audioManager) audioManager->PlaySound(AudioSampleType::CAR_JACK);
                lastJackCrankSounded = crankNum;
            }

            if (localT < CRANK_ANIM_DUR) {
                int frame = (int)(localT / CRANK_ANIM_DUR * jackTotalFrames);
                frame = std::min(frame, jackTotalFrames - 1);
                if (jackAnim) jackAnim->SetFrame(frame);
                tireChangeLiftY = crankNum;
            } else {
                if (jackAnim) jackAnim->SetFrame(jackTotalFrames - 1);
                tireChangeLiftY = crankNum + 1;
            }

            if (tireChangeTimer >= JACK_CRANKS * CRANK_TOTAL_DUR) {
                tireChangeLiftY  = JACK_CRANKS;
                animRearWheelX   = (float)initialRearWheelPos.x;
                animRearWheelY   = (float)(initialRearWheelPos.y - tireChangeLiftY);
                animFrontWheelX  = (float)initialFrontWheelPos.x;
                animFrontWheelY  = (float)(initialFrontWheelPos.y - tireChangeLiftY);
                tireChangeTimer  = 0.0f;
                tireChangePhase  = TireChangePhase::REAR_WHEEL_OUT;
                // The DOS games' tyre change sounds: air wrench off, the swap, lug nuts on. Silent on the Amiga.
                if (audioManager) audioManager->PlaySound(AudioSampleType::TIRE_AIR_WRENCH);
            }
            break;
        }

        case TireChangePhase::REAR_WHEEL_OUT: {
            if (tireChangeTimer < WHEEL_DROP_DUR) {
                float t = clamp01(tireChangeTimer / WHEEL_DROP_DUR);
                animRearWheelY = lerp((float)(initialRearWheelPos.y - tireChangeLiftY),
                                      (float)initialRearWheelPos.y, t);
            } else {
                float t = clamp01((tireChangeTimer - WHEEL_DROP_DUR) / rearRollDur);
                animRearWheelY = (float)initialRearWheelPos.y;
                animRearWheelX = lerp((float)initialRearWheelPos.x, (float)-wheelSize, t);
                if (sr2) Sr2TireRoll(true);
            }
            if (tireChangeTimer >= WHEEL_DROP_DUR + rearRollDur) {
                if (sr2) Sr2TireRoll(false);
                animFrontWheelX = (float)initialFrontWheelPos.x;
                animFrontWheelY = (float)(initialFrontWheelPos.y - tireChangeLiftY);
                tireChangeTimer = 0.0f;
                tireChangePhase = TireChangePhase::FRONT_WHEEL_OUT;
                if (audioManager) audioManager->PlaySound(AudioSampleType::TIRE_AIR_WRENCH);
            }
            break;
        }

        case TireChangePhase::FRONT_WHEEL_OUT: {
            if (tireChangeTimer < WHEEL_DROP_DUR) {
                float t = clamp01(tireChangeTimer / WHEEL_DROP_DUR);
                animFrontWheelY = lerp((float)(initialFrontWheelPos.y - tireChangeLiftY),
                                       (float)initialFrontWheelPos.y, t);
            } else {
                float t = clamp01((tireChangeTimer - WHEEL_DROP_DUR) / frontRollDur);
                animFrontWheelY = (float)initialFrontWheelPos.y;
                animFrontWheelX = lerp((float)initialFrontWheelPos.x, (float)-wheelSize, t);
                if (sr2) Sr2TireRoll(true);
            }
            if (tireChangeTimer >= WHEEL_DROP_DUR + frontRollDur) {
                if (sr2) Sr2TireRoll(false);
                if (carBitmap) {
                    carBitmap->LoadNewTireSprite(pendingTire.partType.tireType);
                }
                animRearWheelX  = (float)-wheelSize;
                animRearWheelY  = (float)initialRearWheelPos.y;
                tireChangeTimer = 0.0f;
                tireChangePhase = TireChangePhase::NEW_REAR_WHEEL_IN;
                if (audioManager) audioManager->PlaySound(AudioSampleType::TIRE_PUTT);
            }
            break;
        }

        case TireChangePhase::NEW_REAR_WHEEL_IN: {
            if (tireChangeTimer < rearRollDur) {
                float t = clamp01(tireChangeTimer / rearRollDur);
                animRearWheelX = lerp((float)-wheelSize, (float)initialRearWheelPos.x, t);
                animRearWheelY = (float)initialRearWheelPos.y;
                if (sr2) Sr2TireRoll(true);
            } else {
                if (sr2) Sr2TireRoll(false);
                if (carBitmap) carBitmap->ResetWheelFrame();
                float t = clamp01((tireChangeTimer - rearRollDur) / WHEEL_RISE_DUR);
                animRearWheelX = (float)initialRearWheelPos.x;
                animRearWheelY = lerp((float)initialRearWheelPos.y,
                                      (float)(initialRearWheelPos.y - tireChangeLiftY), t);
            }
            if (tireChangeTimer >= rearRollDur + WHEEL_RISE_DUR) {
                if (sr2) PlayBayToolSound(true);
                animRearWheelX  = (float)initialRearWheelPos.x;
                animRearWheelY  = (float)(initialRearWheelPos.y - tireChangeLiftY);
                animFrontWheelX = (float)-wheelSize;
                animFrontWheelY = (float)initialFrontWheelPos.y;
                tireChangeTimer = 0.0f;
                tireChangePhase = TireChangePhase::NEW_FRONT_WHEEL_IN;
                if (audioManager) audioManager->PlaySound(AudioSampleType::TIRE_PUTT);
            }
            break;
        }

        case TireChangePhase::NEW_FRONT_WHEEL_IN: {
            if (tireChangeTimer < frontRollDur) {
                float t = clamp01(tireChangeTimer / frontRollDur);
                animFrontWheelX = lerp((float)-wheelSize, (float)initialFrontWheelPos.x, t);
                animFrontWheelY = (float)initialFrontWheelPos.y;
                if (sr2) Sr2TireRoll(true);
            } else {
                if (sr2) Sr2TireRoll(false);
                if (carBitmap) carBitmap->ResetWheelFrame();
                float t = clamp01((tireChangeTimer - frontRollDur) / WHEEL_RISE_DUR);
                animFrontWheelX = (float)initialFrontWheelPos.x;
                animFrontWheelY = lerp((float)initialFrontWheelPos.y,
                                       (float)(initialFrontWheelPos.y - tireChangeLiftY), t);
            }
            if (tireChangeTimer >= frontRollDur + WHEEL_RISE_DUR) {
                if (sr2) PlayBayToolSound(false);
                if (carBitmap) carBitmap->SetWheelhousesVisible(false);
                animRearWheelX  = (float)initialRearWheelPos.x;
                animRearWheelY  = (float)(initialRearWheelPos.y - tireChangeLiftY);
                animFrontWheelX = (float)initialFrontWheelPos.x;
                animFrontWheelY = (float)(initialFrontWheelPos.y - tireChangeLiftY);
                tireChangeTimer = 0.0f;
                tireChangePhase = TireChangePhase::JACK_CRANK_DOWN;
                if (audioManager) audioManager->PlaySound(AudioSampleType::TIRE_LUG_NUT);
                if (jackAnim) jackAnim->SetFrame(0);
                lastJackCrankSounded = -1;
            }
            break;
        }

        case TireChangePhase::JACK_CRANK_DOWN: {
            int crankNum = (int)(tireChangeTimer / CRANK_TOTAL_DUR);
            float localT = tireChangeTimer - crankNum * CRANK_TOTAL_DUR;
            crankNum = std::min(crankNum, JACK_CRANKS - 1);

            if (crankNum != lastJackCrankSounded && localT < CRANK_ANIM_DUR) {
                if (sr2) Sr2JackCrankSound(false, crankNum);
                else if (audioManager) audioManager->PlaySound(AudioSampleType::CAR_JACK);
                lastJackCrankSounded = crankNum;
            }

            if (localT < CRANK_ANIM_DUR) {
                int frame = (int)(localT / CRANK_ANIM_DUR * jackTotalFrames);
                frame = std::min(frame, jackTotalFrames - 1);
                if (jackAnim) jackAnim->SetFrame(frame);
                tireChangeLiftY = JACK_CRANKS - crankNum;
            } else {
                if (jackAnim) jackAnim->SetFrame(0);
                tireChangeLiftY = JACK_CRANKS - crankNum - 1;
            }

            animRearWheelY  = (float)(initialRearWheelPos.y  - tireChangeLiftY);
            animFrontWheelY = (float)(initialFrontWheelPos.y - tireChangeLiftY);

            if (tireChangeTimer >= JACK_CRANKS * CRANK_TOTAL_DUR) {
                tireChangeLiftY = 0;
                jackYPos        = 123.0f;
                tireChangeTimer = 0.0f;
                tireChangePhase = TireChangePhase::JACK_SLIDE_DOWN;
            }
            break;
        }

        case TireChangePhase::JACK_SLIDE_DOWN: {
            float t = clamp01(tireChangeTimer / JACK_SLIDE_DUR);
            jackYPos = lerp(123.0f, jackStartY, t);

            animRearWheelY  = (float)initialRearWheelPos.y;
            animFrontWheelY = (float)initialFrontWheelPos.y;

            if (tireChangeTimer >= JACK_SLIDE_DUR) {
                if (sr2 && audioManager) {
                    for (int i = 0; i < 2; ++i) {
                        audioManager->PlaySound(AudioSampleType::HISS, 9 / 64.0f, 3579545 / (150 + std::rand() % 50));
                    }
                }
                if (state.GetCurrentCar()) state.GetCurrentCar()->SetTires(pendingTire);
                state.AdvanceTicks(600);   // lgarage.c 0x25355e
                if (carBitmap) carBitmap->SetWheelsVisible(true);
                tireChangeLiftY = 0;
                tireChangePhase = TireChangePhase::IDLE;
            }
            break;
        }

        default:
            break;
    }
}

