#include "race/racing.h"
#include "race/racing_internal.h"
#include "debug/debug_ui.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>

#include "core/log.h"
#include "core/state.h"

using namespace racing_internal;

void Racing::SetDebugFeaturesEnabled(bool enabled) {
    debugFeaturesEnabled = enabled;
    if (!enabled && flybyMode) {
        flybyMode = false;
        opponentActive = opponentActiveBeforeFlyby;
    }
}

std::string Racing::FormatSegmentFlags(uint16_t flags) {
    std::string out;
    if (flags & 0x0001) out += "city ";
    if (flags & 0x0002) out += "sign ";
    if (flags & 0x0004) out += "side ";
    if (flags & 0x0008) out += "dash ";
    if (flags & 0x0010) out += "dashW ";
    if ((flags & 0x00e0) == 0x0020) out += "narrowL ";
    else if ((flags & 0x00e0) == 0x0040) out += "narrowR ";
    else if ((flags & 0x00e0) == 0x0080) out += "narrowB ";
    if (flags & 0x0100) out += "spr5 ";
    if (flags & 0x0200) out += "curveL ";
    if (flags & 0x0400) out += "curveR ";
    if (flags & 0x0800) out += "narrowWarn ";
    if (flags & 0xf000) out += "arch ";
    if (out.empty()) {
        out = "straight";
    }
    return out;
}

void Racing::ApplyFlybyCamera() {
    const int seg = ClampInt(static_cast<int>(flybySeg), 0, srrace::TRACK_SEGMENTS - 1);
    const int16_t elevAtCar = track.elev[seg];
    sim.SetCameraState(flybySeg, flybyScrollAcc, flybyHeading, flybyLateral, elevAtCar);
    if (flybyNpcVisible) {
        sim.SetNpcDebugPose(flybySeg + 6, 2560);
    } else {
        sim.SetNpcDebugPose(flybySeg - 20, 2560);
    }
}

void Racing::FlybyScroll(int scrollDelta) {
    if (scrollDelta == 0) {
        return;
    }
    int32_t seg = flybySeg;
    int acc = static_cast<int>(flybyScrollAcc) + scrollDelta;
    while (acc > 7) {
        seg += 1;
        acc -= 8;
    }
    while (acc < 0) {
        seg -= 1;
        acc += 8;
    }
    constexpr int kMaxSeg = srrace::TRACK_SEGMENTS - 1;
    if (seg < 0) {
        seg = 0;
        acc = 0;
    } else if (seg > kMaxSeg) {
        seg = kMaxSeg;
        acc = 7;
    }
    flybySeg = seg;
    flybyScrollAcc = static_cast<int16_t>(acc);
    ApplyFlybyCamera();
}

void Racing::FlybyJumpSegments(int segDelta) {
    if (segDelta == 0) {
        return;
    }
    int32_t seg = flybySeg + segDelta;
    seg = std::max<int32_t>(0, std::min<int32_t>(seg, srrace::TRACK_SEGMENTS - 1));
    flybySeg = seg;
    ApplyFlybyCamera();
}

void Racing::UpdateFlybyMovement(float frameSeconds) {
    // Key state is polled from SDL, so the debug UI's keyboard capture is checked explicitly.
    const Uint8* keyboard = DebugUiWantsKeyboard() ? nullptr : SDL_GetKeyboardState(nullptr);
    if (!keyboard) {
        return;
    }

    static float scrollRepeat = 0.0f;
    const bool forward = keyboard[SDL_SCANCODE_RIGHTBRACKET] != 0;
    const bool backward = keyboard[SDL_SCANCODE_LEFTBRACKET] != 0;
    if (!forward && !backward) {
        scrollRepeat = 0.0f;
        return;
    }

    scrollRepeat += frameSeconds;
    const float interval = (keyboard[SDL_SCANCODE_LSHIFT] || keyboard[SDL_SCANCODE_RSHIFT]) ? 0.03f : 0.06f;
    if (scrollRepeat < interval) {
        return;
    }
    scrollRepeat = 0.0f;

    const bool shift = keyboard[SDL_SCANCODE_LSHIFT] || keyboard[SDL_SCANCODE_RSHIFT];
    const int step = shift ? 8 : 1;
    FlybyScroll(forward ? step : -step);
}

bool Racing::ToggleFlybyMode() {
    if (!debugFeaturesEnabled) {
        return false;
    }
    if (!simInitialized) {
        InitializeRaceSimulation();
    }

    if (!flybyMode) {
        const srrace::CarState& p = sim.Player();
        flybySeg = p.segPos;
        flybyScrollAcc = p.scrollAcc;
        flybyHeading = p.heading;
        flybyLateral = p.lateral;
        opponentActiveBeforeFlyby = opponentActive;
        opponentActive = false;
        flybyNpcVisible = false;
        raceOutcome = RaceOutcome::Ongoing;
        bShowCrashOverlay = false;
    } else {
        opponentActive = opponentActiveBeforeFlyby;
    }

    flybyMode = !flybyMode;
    if (flybyMode) {
        ApplyFlybyCamera();
    }
    return true;
}

bool Racing::OnFlybyKeyDown(SDL_Keycode key) {
    if (!flybyMode) {
        return false;
    }

    switch (key) {
        case SDLK_EQUALS:
        case SDLK_KP_PLUS:
            flybyHeading = static_cast<int16_t>(flybyHeading + 1);
            ApplyFlybyCamera();
            return true;
        case SDLK_MINUS:
        case SDLK_KP_MINUS:
            flybyHeading = static_cast<int16_t>(flybyHeading - 1);
            ApplyFlybyCamera();
            return true;
        case SDLK_COMMA:
            flybyLateral = static_cast<int16_t>(flybyLateral - 40);
            ApplyFlybyCamera();
            return true;
        case SDLK_PERIOD:
            flybyLateral = static_cast<int16_t>(flybyLateral + 40);
            ApplyFlybyCamera();
            return true;
        case SDLK_LEFTBRACKET: {
            const Uint8* kb = SDL_GetKeyboardState(nullptr);
            const bool ctrl = kb && (kb[SDL_SCANCODE_LCTRL] || kb[SDL_SCANCODE_RCTRL]);
            if (ctrl) {
                FlybyJumpSegments(-10);
                return true;
            }
            return false;
        }
        case SDLK_RIGHTBRACKET: {
            const Uint8* kb = SDL_GetKeyboardState(nullptr);
            const bool ctrl = kb && (kb[SDL_SCANCODE_LCTRL] || kb[SDL_SCANCODE_RCTRL]);
            if (ctrl) {
                FlybyJumpSegments(10);
                return true;
            }
            return false;
        }
        case SDLK_HOME:
            flybySeg = srrace::RACE_START_SEG;
            flybyScrollAcc = 0;
            ApplyFlybyCamera();
            return true;
        case SDLK_END:
            flybySeg = sim.FinishSegment() - 1;
            flybyScrollAcc = 0;
            ApplyFlybyCamera();
            return true;
        case SDLK_PAGEUP:
            FlybyJumpSegments(50);
            return true;
        case SDLK_PAGEDOWN:
            FlybyJumpSegments(-50);
            return true;
        case SDLK_n:
            flybyNpcVisible = !flybyNpcVisible;
            ApplyFlybyCamera();
            return true;
        default:
            break;
    }
    return false;
}

std::vector<std::string> Racing::GetFlybyDebugLines(int lastVisibleRow) const {
    std::vector<std::string> lines;

    const srrace::CarState& player = sim.Player();
    const int seg = flybyMode
                        ? ClampInt(static_cast<int>(flybySeg), 0, srrace::TRACK_SEGMENTS - 1)
                        : ClampInt(static_cast<int>(player.segPos), 0, srrace::TRACK_SEGMENTS - 1);
    const int acc = flybyMode ? static_cast<int>(flybyScrollAcc)
                              : static_cast<int>(player.scrollAcc);
    const int heading = flybyMode ? static_cast<int>(flybyHeading)
                                  : static_cast<int>(player.heading);
    const int lateral = flybyMode ? static_cast<int>(flybyLateral)
                                  : static_cast<int>(player.lateral);

    char buf[160];
    std::snprintf(buf, sizeof(buf), "%s seg %d +%d/8  var %d  %s",
                  flybyMode ? "FLYBY" : "race", seg, acc, trackVariant,
                  currentTrackType == SR_TRACK_DRAG_RACE ? "drag" : "road");
    lines.push_back(buf);

    std::snprintf(buf, sizeof(buf), "curve %d elev %d  heading %d  lat %d",
                  static_cast<int>(track.curve[seg]),
                  static_cast<int>(track.elev[seg]),
                  heading, lateral);
    lines.push_back(buf);

    if (!flybyMode) {
        std::snprintf(buf, sizeof(buf), "speed %d  rpm %d  gear %d  step %d",
                      static_cast<int>(player.speed), static_cast<int>(player.rpm),
                      static_cast<int>(player.gear), static_cast<int>(player.scrollStep));
        lines.push_back(buf);
    }

    std::snprintf(buf, sizeof(buf), "flags 0x%04x %s  Lobj 0x%04x Robj 0x%04x",
                  static_cast<unsigned>(track.flags[seg]),
                  FormatSegmentFlags(track.flags[seg]).c_str(),
                  static_cast<unsigned>(track.left[seg]),
                  static_cast<unsigned>(track.right[seg]));
    lines.push_back(buf);

    if (lastVisibleRow >= 0 && lastVisibleRow < static_cast<int>(rows.size())) {
        const RowProjection& row = rows[static_cast<size_t>(lastVisibleRow)];
        std::snprintf(buf, sizeof(buf), "row0 seg %d y %d L %d R %d d %d",
                      row.seg, row.y, row.leftX, row.rightX, row.distance);
        lines.push_back(buf);
    }

    if (flybyMode) {
        lines.push_back("[ ] step  shift+[ ] seg  ctrl+[ ] +/-10seg");
        lines.push_back("=/- heading  ,/. lateral  Home/End start/finish");
        lines.push_back("PgUp/Dn +/-50seg  N toggle NPC  F9 exit flyby");
        lines.push_back("P save road PNG (no cockpit)");
    } else {
        lines.push_back("F9 enter flyby");
    }
    return lines;
}

std::vector<DebugRow> Racing::GetRacingDebugRows(int lastVisibleRow) const {
    std::vector<DebugRow> out;

    const srrace::CarState& player = sim.Player();
    const int seg = flybyMode
                        ? ClampInt(static_cast<int>(flybySeg), 0, srrace::TRACK_SEGMENTS - 1)
                        : ClampInt(static_cast<int>(player.segPos), 0, srrace::TRACK_SEGMENTS - 1);
    const int acc = flybyMode ? static_cast<int>(flybyScrollAcc)
                              : static_cast<int>(player.scrollAcc);
    const int heading = flybyMode ? static_cast<int>(flybyHeading)
                                  : static_cast<int>(player.heading);
    const int lateral = flybyMode ? static_cast<int>(flybyLateral)
                                  : static_cast<int>(player.lateral);

    char buf[160];
    out.push_back({"track", ""});
    out.push_back({"mode", flybyMode ? "flyby camera" : "racing"});
    out.push_back({"track", currentTrackType == SR_TRACK_DRAG_RACE ? "drag" : "road"});
    out.push_back({"variant", std::to_string(trackVariant)});

    std::snprintf(buf, sizeof(buf), "%d  +%d/8", seg, acc);
    out.push_back({"segment", buf});
    out.push_back({"curve", std::to_string(static_cast<int>(track.curve[seg]))});
    out.push_back({"elevation", std::to_string(static_cast<int>(track.elev[seg]))});
    out.push_back({"heading", std::to_string(heading)});
    out.push_back({"lateral", std::to_string(lateral)});

    std::snprintf(buf, sizeof(buf), "0x%04x %s", static_cast<unsigned>(track.flags[seg]),
                  FormatSegmentFlags(track.flags[seg]).c_str());
    out.push_back({"flags", buf});
    std::snprintf(buf, sizeof(buf), "0x%04x", static_cast<unsigned>(track.left[seg]));
    out.push_back({"left object", buf});
    std::snprintf(buf, sizeof(buf), "0x%04x", static_cast<unsigned>(track.right[seg]));
    out.push_back({"right object", buf});

    if (sim.Kind() == srrace::RaceKind::Grudge) {
        const int treeSeg = srrace::RaceWindowFor(srrace::RaceKind::Grudge).start + 4;
        int treeRow = -1;
        for (int r = 0; r < kForwardRows; ++r) {
            if (rows[r].seg == treeSeg) {
                treeRow = r;
                break;
            }
        }
        std::snprintf(buf, sizeof(buf), "seg %d row %d %s", treeSeg, treeRow,
                      Sr2Asset(SR2_DRAG_TREE) ? "sheet ok" : "NO SHEET");
        out.push_back({"drag tree", buf});
    }

    out.push_back({"player car", ""});
    out.push_back({"speed", std::to_string(static_cast<int>(player.speed)) + " mph"});
    out.push_back({"rpm", std::to_string(static_cast<int>(player.rpm))});
    out.push_back({"gear", std::to_string(static_cast<int>(player.gear))});
    out.push_back({"scroll step", std::to_string(static_cast<int>(player.scrollStep))});
    out.push_back({"lap distance", std::to_string(player.distanceAcc)});

    if (lastVisibleRow >= 0 && lastVisibleRow < static_cast<int>(rows.size())) {
        const RowProjection& row = rows[static_cast<size_t>(lastVisibleRow)];
        std::snprintf(buf, sizeof(buf), "seg %d  y %d  L %d  R %d  d %d",
                      row.seg, row.y, row.leftX, row.rightX, row.distance);
        out.push_back({"far row", buf});
    } else {
        out.push_back({"far row", "-"});
    }

    // Segments scaled by 8 plus the scroll remainder; positive means ahead.
    const srrace::CarState& npc = sim.Npc();
    const int segGap = static_cast<int>(npc.segPos) - static_cast<int>(player.segPos);
    const int fineGap = segGap * 8 + (npc.scrollAcc - player.scrollAcc);
    out.push_back({"opponent", ""});
    out.push_back({"active", opponentActive ? "yes" : "no"});
    static const char* const kCarTypeNames[] = {
        "chevy", "chrysler", "cop", "oldsmobile", "chevy2 (corvette 56/61)", "corvette 63", "ford"
    };
    const char* carTypeName = (opponentCarType >= 0 &&
                                opponentCarType < static_cast<int>(std::size(kCarTypeNames)))
                                   ? kCarTypeNames[opponentCarType]
                                   : "chevy";
    out.push_back({"car type", carTypeName});
    std::snprintf(buf, sizeof(buf), "%d seg  (%d units)", segGap, fineGap);
    out.push_back({"gap", buf});
    out.push_back({"segment", std::to_string(static_cast<int>(npc.segPos))});
    out.push_back({"speed", std::to_string(static_cast<int>(npc.speed)) + " mph"});
    out.push_back({"lateral", std::to_string(static_cast<int>(npc.lateral))});
    out.push_back({"lateral gap", std::to_string(npc.lateral - player.lateral)});
    out.push_back({"view state", std::to_string(static_cast<int>(sim.NpcAi().viewState)) +
                                 (sim.NpcAi().viewHidden ? " (hidden)" : "")});

    // State 0: the spawn roll did not arm one.
    const int copState = sim.CopState();
    out.push_back({"cop", ""});
    // Always three rows, so nothing below shifts when the state changes.
    const bool copPossible = sim.IsRoadRace() && copState != 0;
    out.push_back({"can appear", sim.Kind() == srrace::RaceKind::Drag ? "no (drag race)"
                                 : copState == 0     ? "no (roll failed)"
                                                     : "yes"});
    out.push_back({"trigger seg",
                   copPossible ? std::to_string(sim.CopTriggerSegment()) : std::string("-")});
    if (copPossible) {
        const int toTrigger = sim.CopTriggerSegment() - static_cast<int>(player.segPos);
        out.push_back({"to trigger", toTrigger > 0 ? std::to_string(toTrigger) + " seg"
                                                   : std::string("passed")});
    } else {
        out.push_back({"to trigger", "-"});
    }
    const char* copStateName = copState == 0   ? "not armed"
                               : copState == 1 ? "armed, waiting"
                               : copState == 2 ? "pursuing"
                                               : "pursuing, siren on";
    out.push_back({"state", copStateName});
    out.push_back({"busted at", sim.PlayerBusted()
                                    ? std::to_string(sim.CopCaughtSpeed()) + " mph"
                                    : std::string("-")});
    out.push_back({"fine", sim.PlayerBusted() ? "$" + std::to_string(sim.SpeedingFine())
                                              : std::string("-")});
    return out;
}

std::vector<DebugRow> Racing::GetOpponentDebugRows() const {
    std::vector<DebugRow> out;
    const srrace::CarState& player = sim.Player();
    const srrace::CarState& npc = sim.Npc();
    const srrace::NpcState& ai = sim.NpcAi();

    const char* branch = "accelerating";
    switch (ai.branch) {
        case srrace::NpcBranch::HoldingLevel:  branch = "holding level (player +16)"; break;
        case srrace::NpcBranch::CruisingAhead: branch = "clear ahead, cruising"; break;
        case srrace::NpcBranch::CopClosingCap: branch = "cop cap (player +32)"; break;
        case srrace::NpcBranch::CorneringHold: branch = "in a curve, holding"; break;
        case srrace::NpcBranch::CorneringCap:  branch = "braked for the curve"; break;
        case srrace::NpcBranch::Accelerating:  break;
    }

    out.push_back({"driving", ""});
    out.push_back({"decision", branch});
    out.push_back({"eased to", ai.branchTarget > 0
                                   ? std::to_string(ai.branchTarget) + " mph"
                                   : std::string("-")});
    out.push_back({"gave up", ai.gaveUp ? "yes" : "no"});
    out.push_back({"stalled", ai.stallTicks > 0
                                  ? std::to_string(ai.stallTicks) + " ticks"
                                  : std::string("-")});

    out.push_back({"steering", ""});
    out.push_back({"lane target", std::to_string(ai.lateralTarget)});
    out.push_back({"lateral", std::to_string(static_cast<int>(npc.lateral))});

    out.push_back({"car", ""});
    out.push_back({"speed", std::to_string(static_cast<int>(npc.speed)) + " mph"});
    out.push_back({"rpm", std::to_string(static_cast<int>(npc.rpm))});
    out.push_back({"gear", std::to_string(static_cast<int>(npc.gear))});
    out.push_back({"cruise / risky", std::to_string(ai.targetSpeedNormal) + " / " +
                                     std::to_string(ai.targetSpeedRisky)});
    out.push_back({"performance", std::to_string(sim.NpcTables().perfScore)});
    out.push_back({"top speed", std::to_string(sim.NpcTables().topSpeed) + " mph"});

    const int segGap = static_cast<int>(npc.segPos) - static_cast<int>(player.segPos);
    out.push_back({"gap", std::to_string(segGap) + " seg"});
    return out;
}

std::vector<DebugIndicator> Racing::GetOpponentIndicators() const {
    const srrace::CarState& npc = sim.Npc();
    const srrace::NpcState& ai = sim.NpcAi();

    // The steering's own deadband, so the lamp doesn't flicker.
    const int latDelta = ai.lateralTarget - npc.lateral;
    const bool left = latDelta < -8;
    const bool right = latDelta > 8;

    return {
        {"<<", left},
        {"--", !left && !right},
        {">>", right},
        {"GAS", ai.throttle},
        {"SHIFT", ai.shifted},
        {"EASED", ai.branch != srrace::NpcBranch::Accelerating &&
                  ai.branch != srrace::NpcBranch::CorneringHold},
    };
}

bool Racing::SaveRoadViewPng(std::string* savedPath) {
    if (!pixels) {
        return false;
    }

    namespace fs = std::filesystem;
    const fs::path dir = "roadfly";
    std::error_code ec;
    fs::create_directories(dir, ec);

    const int seg = flybyMode
                        ? ClampInt(static_cast<int>(flybySeg), 0, srrace::TRACK_SEGMENTS - 1)
                        : ClampInt(static_cast<int>(sim.Player().segPos), 0, srrace::TRACK_SEGMENTS - 1);
    const int acc = flybyMode ? static_cast<int>(flybyScrollAcc) : static_cast<int>(sim.Player().scrollAcc);
    const char* trackTag = currentTrackType == SR_TRACK_DRAG_RACE ? "drag" : "road";

    std::string path;
    for (int n = 0; n < 10000; ++n) {
        char buf[192];
        if (n == 0) {
            std::snprintf(buf, sizeof(buf), "roadfly/%s_seg%03d_acc%d_var%d.png",
                          trackTag, seg, acc, trackVariant);
        } else {
            std::snprintf(buf, sizeof(buf), "roadfly/%s_seg%03d_acc%d_var%d_%d.png",
                          trackTag, seg, acc, trackVariant, n);
        }
        path = buf;
        if (!fs::exists(path)) {
            break;
        }
    }

    SDL_Surface* src = pixels;
    sdl2::SurfacePtr converted;
    if (src->format->format != SDL_PIXELFORMAT_RGBA32) {
        converted = sdl2::SurfacePtr(SDL_ConvertSurfaceFormat(src, SDL_PIXELFORMAT_RGBA32, 0));
        if (!converted) {
            LOG_INFO("SaveRoadViewPng: surface convert failed: %s", SDL_GetError());
            return false;
        }
        src = converted.get();
    }

    if (IMG_SavePNG(src, path.c_str()) != 0) {
        LOG_INFO("SaveRoadViewPng: save failed: %s", SDL_GetError());
        return false;
    }

    LOG_INFO("Road view saved: %s", path.c_str());
    if (savedPath != nullptr) {
        *savedPath = path;
    }
    return true;
}

void Racing::DrawFlybyHud(int lastVisible) {
    if (!hud) {
        return;
    }
    hud->Clear();
    hud->AddText(6, 0, "FLYBY");
    const std::vector<std::string> lines = GetFlybyDebugLines(lastVisible);
    int y = 10;
    for (size_t i = 1; i < lines.size() && i < 4; ++i) {
        hud->AddText(6, y, lines[i].c_str());
        y += 10;
    }
    hud->Draw();
}
