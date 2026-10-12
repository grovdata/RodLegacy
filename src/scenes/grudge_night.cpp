#include "scenes/grudge_night.h"

#include "core/state.h"
#include "race/driving.h"
#include "race/racing.h"
#include "race/roster.h"

#include <algorithm>
#include <cstdlib>

namespace {

// Read off lgrudge.c: the panel is a 216x164 buffer blitted at.
constexpr SDL_Rect kPanelRect = {48, 24, 216, 164};
constexpr int kBannerX = 24;
constexpr int kBannerY = 30;
constexpr int kBannerH = 50;
// Two numbered columns of five, as the original lists them, with a heading
// above and a single line under.
constexpr int kHeadlineX = 56;
constexpr int kHeadlineY = kBannerY + kBannerH + 4;    // 84, above the table
constexpr int kFieldY = 96;
constexpr int kFieldStep = 10;                         // five rows, 96..136
constexpr int kFieldRows = 5;
constexpr int kFieldColX[2] = {56, 164};
constexpr int kDetailY = 150;                          // under the table
// The round screen: who you are up against, then their car under it.
constexpr int kOpponentY = kHeadlineY + 12;
constexpr int kOpponentCarY = kOpponentY + 10;
constexpr SDL_Point kCarPos = {56, 105};        // the 200x66 piece
constexpr SDL_Point kPortraitPos = {208, 82};   // the 40x35 piece
// The original blits the car into that 200x66 box, which is far smaller than a full car sprite.
constexpr int kCarBoxW = 200;
constexpr int kCarBoxH = 66;
// FUN_002552c8's Grudge banner animation: four slides, two pixels a frame. The text (FUN_00254f22) appears once all four have landed.
//
//   1  banner         (24, 30)     from the right
//   2  panel 216x164  (48, 24)     from the top
//   3  car 200x66     (56, 105)    from the left
//   0  portrait 40x35 (208, 82)    from the bottom
constexpr int kSlideStep = 2;
// Travel per piece, from each mode's own frame count in FUN_00254996.
constexpr int kSlidePanelTravel = ((24 + 164) / 2) * kSlideStep;        // mode 2
constexpr int kSlideBannerTravel = ((GAME_WIDTH - 24) / 2) * kSlideStep;  // mode 1
constexpr int kSlideCarTravel = ((56 + 200 - 60) / 2) * kSlideStep;        // mode 3
constexpr int kSlidePortraitTravel = ((200 - 82 - 16) / 2) * kSlideStep;   // mode 0

// The panel is dark, so the writing on it has to be light. This was black on
// black, which drew every label into a texture nobody could see.
constexpr SDL_Color kInk = {0xd0, 0xd0, 0xd0, 0xff};
constexpr SDL_Color kPanelFill = {0x18, 0x18, 0x1c, 0xff};
constexpr SDL_Color kPanelBorder = {0x90, 0x90, 0x98, 0xff};
constexpr uint32_t kTextColor = 0x00e0e0e0;
constexpr size_t kFieldSize = 10;
constexpr int kTenthsPerSecond = 10;
constexpr uint32_t kRoundText = 0x40900;       // printf: the round
constexpr uint32_t kPrepareText = 0x40926;
constexpr uint32_t kStandingsText = 0x4094e;

std::string Tenths(int tenths) {
    return std::to_string(tenths / kTenthsPerSecond) + "." +
           std::to_string(tenths % kTenthsPerSecond);
}

// lgrudge.c's "%s (%+d.%d)": the name, then the handicap as seconds and tenths with a forced sign.
std::string SignedTenths(int tenths) {
    const int whole = tenths / kTenthsPerSecond;
    const int frac = std::abs(tenths % kTenthsPerSecond);
    const bool negative = tenths < 0;
    std::string out = negative && whole == 0 ? "-0" : std::to_string(whole);
    if (!negative && whole >= 0) {
        out = "+" + out;
    }
    return out + "." + std::to_string(frac);
}

// Label is a Bitmap and Bitmap::Draw() is `if.
std::string NumberedRow(size_t n, const std::string& name) {
    const std::string num = std::to_string(n);
    return (num.size() < 2 ? " " + num : num) + " " + name;
}

void SetLabel(Label& label, const std::string& text) {
    label.SetText(text);
    label.SetVisible(!text.empty());
}

}  // namespace

GrudgeNight::GrudgeNight() = default;

void GrudgeNight::Init(bool debugMode) {
    Scene::Init(debugMode);

    RectangleConfig panelCfg{};
    panelCfg.rect = kPanelRect;
    panelCfg.fillColor = kPanelFill;
    panelCfg.borderColor = kPanelBorder;
    panelCfg.inset = 1;
    panel = std::make_unique<Rectangle>(panelCfg);
    panel->SetVisible(true);

    SFont_Font* font = state.GetAssets().GetMainFont();
    headline.Init(font, kHeadlineX, kHeadlineY, kTextColor);
    detail.Init(font, kHeadlineX, kDetailY, kTextColor);

    // Numbered down the first column, then the second: 1..5 then 6..10.
    fieldLabels.resize(kFieldSize);
    for (size_t i = 0; i < kFieldSize; ++i) {
        const int col = static_cast<int>(i) / kFieldRows;
        const int row = static_cast<int>(i) % kFieldRows;
        fieldLabels[i].Init(font, kFieldColX[col], kFieldY + row * kFieldStep, kTextColor);
    }

    opponentLine.Init(font, kHeadlineX, kOpponentY, kTextColor);
    opponentCarLine.Init(font, kHeadlineX, kOpponentCarY, kTextColor);
    standingLabels.resize(kFieldSize);
    for (size_t i = 0; i < kFieldSize; ++i) {
        const int col = static_cast<int>(i) / kFieldRows;
        const int row = static_cast<int>(i) % kFieldRows;
        standingLabels[i].Init(font, kFieldColX[col], kFieldY + row * kFieldStep, kTextColor);
    }


    Refresh();
}

namespace {
// The original leaves blanks in these lines and pokes the digits in.
std::string Poke(std::string t, size_t at, size_t len, const std::string& with) {
    if (at == std::string::npos || at + len > t.size()) return t;
    return t.replace(at, len, with);
}
// "   . ." or "   .  ": two blanks for the seconds, the dot, one for the tenths.
std::string PokeTime(const std::string& t, const char* after, int tenths) {
    const size_t dot = t.find(after);
    return dot == std::string::npos || dot < 2 ? t : Poke(t, dot - 2, 4, Tenths(tenths));
}
}

std::string GrudgeNight::Line(int id, size_t line) const {
    if (messages.empty() && state.GetAssets().IsGameLoaded(SrGame::SR2)) {
        messages = SrLibReadSr2Messages(state.GetAssets().GetSrExeData(SrGame::SR2));
    }
    if (id < 0 || id >= static_cast<int>(messages.size()) || line >= messages[id].size()) return {};
    return messages[id][line];
}

std::string GrudgeNight::Sr2String(uint32_t fileOffset) const {
    const std::vector<uint8_t>& exe = state.GetAssets().GetSrExeData(SrGame::SR2);
    if (fileOffset >= exe.size()) return {};
    return reinterpret_cast<const char*>(exe.data() + fileOffset);
}

std::string GrudgeNight::ResultMessage() const {
    // Straight after the qualifier the news is the breakout it just set.
    if (night.outcome == srrace::GrudgeOutcome::Racing && night.round == 0) {
        return PokeTime(Line(SR2_MSG_GRUDGE_BREAKOUT_SET + 1), ". .", night.breakoutTenths);
    }
    switch (night.outcome) {
        case srrace::GrudgeOutcome::BrokeOut:     return Line(SR2_MSG_GRUDGE_BROKE_OUT);
        case srrace::GrudgeOutcome::CrossedLine:  return Line(SR2_MSG_GRUDGE_CROSSED_LINE);
        case srrace::GrudgeOutcome::WeakBreakout: return Line(SR2_MSG_GRUDGE_WEAK_BREAKOUT);
        case srrace::GrudgeOutcome::Eliminated:
            return PokeTime(Line(SR2_MSG_GRUDGE_LOST), ".  ", lossMarginTenths);
        case srrace::GrudgeOutcome::Finished:     return Line(SR2_MSG_GRUDGE_STANDINGS);
        case srrace::GrudgeOutcome::Racing: {
            std::string won = Line(SR2_MSG_GRUDGE_ROUND_WON);
            won = PokeTime(won, ". .", lastTimeTenths);
            const size_t round = won.find("   ");
            won = Poke(won, round, 3, " " + std::to_string(night.round) + " ");
            return won + "\n" + Line(SR2_MSG_GRUDGE_ROUND_WON, 1);
        }
    }
    return {};
}

void GrudgeNight::ShowResultBubble() {
    const std::string text = ResultMessage();
    if (text.empty()) {
        resultBubble.reset();
        return;
    }
    resultBubble = std::make_unique<Bubble>(text, BUBBLE_RIGHT_TWO_LINES, false);
    resultBubble->SetVisible(true);
}

void GrudgeNight::Begin() {
    night = srrace::GrudgeNightState{};
    resultBubble.reset();
    bracketRng = srrace::RaceRng(static_cast<uint32_t>(SDL_GetTicks()) | 1u);
    roundCar = nullptr;
    roundPortrait = nullptr;
    slideLeft = {};
    carBitmaps.clear();
    SetLabel(opponentLine, "");
    SetLabel(opponentCarLine, "");
    // The banner and the drag tree are Street Rod 2 art.
    if (!state.GetAssets().IsGameLoaded(SrGame::SR2)) {
        std::string err;
        if (!state.GetAssets().LoadGame(SrGame::SR2, &err)) {
            LOG_WARN("Grudge Night needs Street Rod 2's lib.p: %s", err.c_str());
        }
    }
    state.SetBankroll(state.GetBankroll() - srrace::GRUDGE_ENTRY_FEE);

    field.clear();
    bracket = {};
    Roster& roster = state.GetRoster();
    const size_t count = std::min<size_t>(roster.GetOpponentCount(), kFieldSize - 1);
    for (size_t i = 0; i < count; ++i) {
        Opponent* opp = roster.GetOpponentByIndex(static_cast<int>(i));
        if (opp == nullptr) {
            continue;
        }
        field.push_back(opp->driver.name);
        // Each driver's predicted time off their car and their tier.
        const SrDriverData* row = roster.GetSrDriver(opp->driver.srDriverIndex);
        const int tier = row != nullptr ? static_cast<int>(row->tier) : 0;
        bracket[i].driver = static_cast<int>(i);
        bracket[i].breakoutTenths =
            srrace::GrudgeBreakoutForCar(opp->driver.preferredCar, tier);
    }
    field.push_back(state.GetPlayerName());
    if (count == 0) {
        LOG_WARN("Grudge Night: the roster has no opponents, so there is no field");
    }

    for (size_t i = 0; i < fieldLabels.size(); ++i) {
        // "%2d %s", the same shape the standings use.
        SetLabel(fieldLabels[i], i < field.size() ? NumberedRow(i + 1, field[i])
                                                  : std::string());
    }
    Refresh();
}

// Who the player meets this round, and their dial.
int GrudgeNight::OpponentBreakoutTenths() const {
    const int slot = OpponentSlot();
    return slot >= 0 ? bracket[static_cast<size_t>(slot)].breakoutTenths : 0;
}

int GrudgeNight::PlayerSlot() const {
    return srrace::GrudgePlayerSlot(bracket.data(), static_cast<int>(bracket.size()));
}

int GrudgeNight::OpponentSlot() const {
    return srrace::GrudgeOpponentSlot(bracket.data(), static_cast<int>(bracket.size()));
}

Opponent* GrudgeNight::OpponentAt(int slot) const {
    if (slot < 0 || slot >= static_cast<int>(bracket.size())) {
        return nullptr;
    }
    const int driver = bracket[static_cast<size_t>(slot)].driver;
    if (driver < 0) {
        return nullptr;   // the player, or an unfilled slot
    }
    return state.GetRoster().GetOpponentByIndex(driver);
}

// How much the car has to shrink to sit in the original's 200x66 box. Never
// enlarged: a car already small enough is left alone.
float GrudgeNight::CarScale() const {
    if (roundCar == nullptr || roundCar->GetCar() == nullptr) {
        return 1.0f;
    }
    const CarModel& model = roundCar->GetCar()->GetModel();
    if (model.width <= 0 || model.height <= 0) {
        return 1.0f;
    }
    const float byW = static_cast<float>(kCarBoxW) / static_cast<float>(model.width);
    const float byH = static_cast<float>(kCarBoxH) / static_cast<float>(model.height);
    return std::min(1.0f, std::min(byW, byH));
}

// The frame the race finished on, as the backdrop. False when there is none, so
// the caller can fall back.
bool GrudgeNight::DrawRaceBackdrop() {
    Driving* driving = state.GetDrivingScene();
    Racing* racing = driving != nullptr ? driving->GetRacing() : nullptr;
    SDL_Surface* frame = racing != nullptr ? racing->GetSurface() : nullptr;
    if (frame == nullptr) {
        return false;
    }
    // Rebuilt whenever the surface changes under us, which is every race.
    if (backdrop == nullptr || backdropFrom != frame) {
        backdrop = sdl2::TexturePtr(SDL_CreateTextureFromSurface(renderer, frame));
        backdropFrom = frame;
    }
    if (backdrop == nullptr) {
        return false;
    }
    const SDL_Rect dst = {0, 0, frame->w, frame->h};
    SDL_RenderCopy(renderer, backdrop.get(), nullptr, &dst);
    return true;
}

void GrudgeNight::StartSlides() {
    slideLeft[SLIDE_PANEL] = kSlidePanelTravel;
    slideLeft[SLIDE_BANNER] = kSlideBannerTravel;
    slideLeft[SLIDE_CAR] = kSlideCarTravel;
    slideLeft[SLIDE_PORTRAIT] = kSlidePortraitTravel;
}

bool GrudgeNight::Sliding() const {
    for (int left : slideLeft) {
        if (left > 0) {
            return true;
        }
    }
    return false;
}

bool GrudgeNight::BeginPiece(SlidePiece piece, SDL_Rect& saved) const {
    const int left = slideLeft[piece];
    if (left <= 0) {
        return false;
    }
    int dx = 0;
    int dy = 0;
    switch (piece) {
        case SLIDE_BANNER: dx = left;  break;   // mode 1, in from the right
        case SLIDE_PANEL:  dy = -left; break;   // mode 2, down from the top
        case SLIDE_CAR:      dx = -left; break;  // mode 3, in from the left
        case SLIDE_PORTRAIT: dy = left;  break;  // mode 0, up from the bottom
        default: return false;
    }
    SDL_RenderGetViewport(renderer, &saved);
    const SDL_Rect shifted = {saved.x + dx, saved.y + dy, saved.w, saved.h};
    SDL_RenderSetViewport(renderer, &shifted);
    return true;
}

void GrudgeNight::EndPiece(bool pushed, const SDL_Rect& saved) const {
    if (pushed) {
        SDL_RenderSetViewport(renderer, &saved);
    }
}

void GrudgeNight::BuildRoundPresentation() {
    roundCar = nullptr;
    roundPortrait = nullptr;
    StartSlides();
    Opponent* opp = OpponentAt(OpponentSlot());
    if (opp == nullptr) {
        SetLabel(opponentLine, "");
        SetLabel(opponentCarLine, "");
        return;
    }
    // "%s (%+d.%d)" at 0x2636ee: the name and the handicap.
    const int handicap = bracket[static_cast<size_t>(OpponentSlot())].breakoutTenths -
                         night.breakoutTenths;
    SetLabel(opponentLine, opp->driver.name + " (" + SignedTenths(handicap) + ")");
    SetLabel(opponentCarLine, opp->car ? opp->car->GetModel().name : std::string());
    if (!opp->car) {
        return;
    }
    auto found = carBitmaps.find(opp->car);
    if (found == carBitmaps.end()) {
        found = carBitmaps
                    .emplace(opp->car,
                             std::make_unique<CarBitmap>(
                                 opp->car, &opp->driver,
                                 static_cast<StandardCarColor>(opp->car->GetColor())))
                    .first;
    }
    roundCar = found->second.get();
    // The portrait is its own piece here, so the car is shown empty.
    roundCar->SetDriverVisible(false);
    roundPortrait = LoadDriverPortrait(opp->driver);
}

// The final table, indexed by placing.
void GrudgeNight::BuildStandings() {
    for (Label& l : standingLabels) {
        SetLabel(l, "");
    }
    for (const srrace::GrudgeFieldEntry& e : bracket) {
        if (e.driver == srrace::GRUDGE_EMPTY || e.placing < 0 ||
            static_cast<size_t>(e.placing) >= standingLabels.size()) {
            continue;
        }
        std::string name;
        if (e.driver == srrace::GRUDGE_PLAYER) {
            name = state.GetPlayerName() + " *";
        } else {
            Opponent* opp = state.GetRoster().GetOpponentByIndex(e.driver);
            name = opp != nullptr ? opp->driver.name : std::string();
        }
        if (name.empty()) {
            continue;
        }
        SetLabel(standingLabels[static_cast<size_t>(e.placing)],
                 NumberedRow(static_cast<size_t>(e.placing) + 1, name));
    }
}

void GrudgeNight::Refresh() {
    switch (srrace::GrudgeNextStep(night)) {
        case srrace::GrudgeStep::Qualify:
            SetLabel(headline, "");
            SetLabel(detail, Sr2String(kPrepareText));
            break;
        case srrace::GrudgeStep::Race:
            {
                char round[32];
                snprintf(round, sizeof(round), Sr2String(kRoundText).c_str(), night.round + 1);
                SetLabel(headline, round);
            }
            SetLabel(detail, "Your breakout is " + Tenths(night.breakoutTenths) +
                           ".\nRun faster and you are out.");
            BuildRoundPresentation();
            break;
        case srrace::GrudgeStep::Standings:
            // lgrudge.c prints its own heading over the table.
            SetLabel(headline, Sr2String(kStandingsText));
            // FUN_00255374 prints the line only for a placing that pays.
            if (night.placing < srrace::GRUDGE_PAYING_PLACES) {
                std::string earns = Line(SR2_MSG_GRUDGE_STANDINGS, 1);
                earns = Poke(earns, earns.find('?'), 1, std::to_string(night.placing + 1));
                earns = Poke(earns, earns.find("$   "), 4, "$" + std::to_string(srrace::GrudgeWinnings(night)));
                earns = Poke(earns, earns.find("``you"), 2, "\n");
                SetLabel(detail, earns);
            } else {
                SetLabel(detail, "");
            }
            BuildStandings();
            break;
        case srrace::GrudgeStep::Over:
            {
                std::string out = Line(SR2_MSG_GRUDGE_BROKE_OUT, 1);
                SetLabel(headline, Poke(out, out.find("``for"), 2, "\n"));
            }
            switch (night.outcome) {
                case srrace::GrudgeOutcome::BrokeOut:
                    SetLabel(detail, Line(SR2_MSG_GRUDGE_BROKE_OUT));
                    break;
                case srrace::GrudgeOutcome::CrossedLine:
                    SetLabel(detail, Line(SR2_MSG_GRUDGE_CROSSED_LINE));
                    break;
                case srrace::GrudgeOutcome::WeakBreakout:
                    SetLabel(detail, Line(SR2_MSG_GRUDGE_WEAK_BREAKOUT));
                    break;
                default:
                    SetLabel(detail, "");
                    break;
            }
            break;
    }

}

void GrudgeNight::StartRace() {
    resultBubble.reset();
    // Race the driver the round screen just showed.
    if (night.round >= 0) {
        Opponent* opp = OpponentAt(OpponentSlot());
        if (opp != nullptr) {
            state.SetNextOpponent(opp);
        }
    }
    state.SetPendingRace(RaceType::Grudge, 0, false);
    state.MoveToScene(SCENE_DRIVING);
}

void GrudgeNight::OnRaceFinished(bool won, bool failed, int timeTenths, int marginTenths,
                                 bool crossedLine) {
    lossMarginTenths = marginTenths;
    lastTimeTenths = timeTenths;
    if (night.round < 0) {
        // Nobody to lose to - the qualifier just has to be completed.
        srrace::GrudgeQualified(night, !failed, timeTenths);
        // The player's own time goes in as their breakout, then the field is
        // sorted and paired - lgrudge.c FUN_00253bce does both in that order.
        if (night.outcome == srrace::GrudgeOutcome::Racing) {
            bracket[srrace::GRUDGE_FIELD_SIZE - 1].driver = srrace::GRUDGE_PLAYER;
            bracket[srrace::GRUDGE_FIELD_SIZE - 1].breakoutTenths = night.breakoutTenths;
            srrace::GrudgeSeedBracket(bracket.data(), srrace::GRUDGE_FIELD_SIZE);
        }
    } else {
        const int closedRound = night.round;
        // The opponent breaking out also hands the round over, so advance on who took it.
        const bool tookRound = srrace::GrudgeRoundDone(
            night, {won, won, OpponentBreakoutTenths()}, timeTenths, crossedLine);
        // Whoever lost this pair goes out and the other pairs are drawn, so the
        // next round is against somebody new.
        srrace::GrudgeAdvanceBracket(bracket.data(), static_cast<int>(bracket.size()),
                                     closedRound, tookRound, bracketRng);
        night.placing = srrace::GrudgePlayerPlacing(bracket.data(),
                                                    static_cast<int>(bracket.size()));
    }

    if (srrace::GrudgeNextStep(night) == srrace::GrudgeStep::Standings) {
        state.SetBankroll(state.GetBankroll() + srrace::GrudgeWinnings(night));
        // Winning the night is what the King wants to see (lgrudge.c 0x255722).
        if (night.placing == 0) {
            state.GetReputation().grudge_won += 1;
        }
    }
    ShowResultBubble();
    Refresh();
}

void GrudgeNight::DrawScene() {
    // The original never clears: the round screen goes up over whatever the race left on screen.
    if (!DrawRaceBackdrop()) {
        Scene::DrawScene();
    }
    // The four pieces on top of it each arrive from their own side.
    SDL_Rect saved{};
    bool pushed = BeginPiece(SLIDE_PANEL, saved);
    panel->Draw();
    EndPiece(pushed, saved);
    pushed = BeginPiece(SLIDE_BANNER, saved);
    // Street Rod 2's own banner, fetched here rather than in Init: the scene is
    // built at startup, which is before that library is open.
    Bitmap* banner = state.GetAssets().GetBitmapAsset(
        {LibAssetRegular, SrLibFileAmigaLibP, SR2_GRUDGE_BANNER, SrGame::SR2});
    if (banner != nullptr) {
        banner->SetPosition(kBannerX, kBannerY);
        banner->Draw();
    }
    EndPiece(pushed, saved);

    EndPiece(pushed, saved);

    // The car and the portrait, each from its own side.
    const bool racing = srrace::GrudgeNextStep(night) == srrace::GrudgeStep::Race;
    if (racing && roundCar != nullptr) {
        // Scaled to the box rather than translated, so the slide offset has to go into the position.
        const float scale = CarScale();
        float sx = 1.0f;
        float sy = 1.0f;
        SDL_RenderGetScale(renderer, &sx, &sy);
        SDL_RenderSetScale(renderer, sx * scale, sy * scale);
        const int dx = -slideLeft[SLIDE_CAR];
        roundCar->Move({static_cast<int>((kCarPos.x + dx) / scale),
                        static_cast<int>(kCarPos.y / scale)});
        roundCar->Draw();
        SDL_RenderSetScale(renderer, sx, sy);
    }
    if (racing && roundPortrait != nullptr) {
        pushed = BeginPiece(SLIDE_PORTRAIT, saved);
        roundPortrait->SetPosition(kPortraitPos.x, kPortraitPos.y);
        roundPortrait->Draw();
        EndPiece(pushed, saved);
    }

    // FUN_00254f22 runs after all four slides, so nothing is written until they
    // have all landed.
    if (Sliding()) {
        for (int& left : slideLeft) {
            left = std::max(0, left - kSlideStep);
        }
        return;
    }
    headline.Draw();
    detail.Draw();
    switch (srrace::GrudgeNextStep(night)) {
        case srrace::GrudgeStep::Qualify:
            // Before the bracket exists, just who turned up.
            for (size_t i = 0; i < field.size() && i < fieldLabels.size(); ++i) {
                fieldLabels[i].Draw();
            }
            break;
        case srrace::GrudgeStep::Race:
            opponentLine.Draw();
            opponentCarLine.Draw();
            break;
        case srrace::GrudgeStep::Standings:
            for (Label& l : standingLabels) {
                l.Draw();
            }
            break;
        case srrace::GrudgeStep::Over:
            break;
    }
    // Over the panel, and never shifted.
    if (resultBubble) {
        resultBubble->Draw();
    }
}

// Whatever the step is, carry on: race, or leave once the night is done.
void GrudgeNight::Advance() {
    switch (srrace::GrudgeNextStep(night)) {
        case srrace::GrudgeStep::Qualify:
        case srrace::GrudgeStep::Race:
            StartRace();
            break;
        case srrace::GrudgeStep::Standings:
        case srrace::GrudgeStep::Over:
            state.MoveToScene(SCENE_GARAGE);
            break;
    }
}

void GrudgeNight::OnSceneEnter() {
    Scene::OnSceneEnter();
    // A race loss leaves the thumbs-down cursor behind.
    SetPointer(SrLibAmigaSpriteAssetMousePointerHand);
    sawPressHere = false;
}

bool GrudgeNight::PushEvent(SDL_Event* event) {
    // No buttons on this screen - a key or a click is the only input it takes.
    if (event->type == SDL_MOUSEBUTTONDOWN || event->type == SDL_KEYDOWN) {
        // A press during the slide lands it rather than advancing, so the screen
        // can always be read before it is dismissed.
        if (Sliding()) {
            slideLeft = {};
            return true;
        }
        sawPressHere = true;
        return true;
    }
    if (event->type == SDL_MOUSEBUTTONUP || event->type == SDL_KEYUP) {
        if (sawPressHere) {
            sawPressHere = false;
            Advance();
        }
        return true;
    }
    return Scene::PushEvent(event);
}
