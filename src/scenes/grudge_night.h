#pragma once

#include "car/carbitmap.h"
#include "race/grudge.h"
#include "race/roster.h"
#include "scenes/scene.h"
#include "ui/bubble.h"
#include "ui/label.h"
#include "ui/rectangle.h"

#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>

// Street Rod 2's Grudge Night: a bracket meeting on Wednesdays, entry $30.

class GrudgeNight : public Scene {
public:
    GrudgeNight();

    void Init(bool debugMode) override;

    // Starts a night: charges the entry fee and builds the field. Call before
    // moving to the scene, not from Init, which runs once at startup.
    void Begin();
    void DrawScene() override;
    void OnSceneEnter() override;
    bool PushEvent(SDL_Event* event) override;

    // Called when a grudge race finishes, with the player's time in tenths.
    void OnRaceFinished(bool won, bool failed, int timeTenths, int marginTenths,
                        bool crossedLine);

    [[nodiscard]] const srrace::GrudgeNightState& Night() const { return night; }

    // The dial of whoever the player meets this round. 0 before the bracket is
    // seeded, which means no handicap either way.
    [[nodiscard]] int OpponentBreakoutTenths() const;

    // What the night has to say about the round just run, in the game's own
    // words. Shown as a bubble both here and by Driving right after the race.
    [[nodiscard]] std::string ResultMessage() const;

private:
    void StartRace();
    void Advance();
    void Refresh();
    // Which bracket slot the player sits in, and who they are paired with.
    [[nodiscard]] int PlayerSlot() const;
    [[nodiscard]] int OpponentSlot() const;
    // The roster entry behind a bracket slot, or null for the player and for a
    // slot no opponent was built for.
    [[nodiscard]] Opponent* OpponentAt(int slot) const;
    void BuildRoundPresentation();
    void BuildStandings();

    srrace::GrudgeNightState night;
    std::vector<std::string> field;
    // The bracket, in the order GrudgeSeedBracket leaves it. Slot 9 is the
    // player before the seeding; afterwards find them by driver == -1.
    std::array<srrace::GrudgeFieldEntry, srrace::GRUDGE_FIELD_SIZE> bracket {};
    Label headline;
    Label detail;
    std::vector<Label> fieldLabels;
    std::unique_ptr<Rectangle> panel;
    // The round's result, in the game's own words. Built on demand so the
    // message table is only wanted once a night is actually running.
    std::unique_ptr<Bubble> resultBubble;
    // This round's opponent, presented the way the original's round screen does
    // it: name, dial, car model, and the car itself.
    Label opponentLine;
    Label opponentCarLine;
    // Keyed by car so a driver met twice in a night is not rebuilt.
    std::map<CarPtr, std::unique_ptr<CarBitmap>> carBitmaps;
    CarBitmap* roundCar = nullptr;
    // The opponent's face, the 40x35 piece that comes up last.
    Bitmap* roundPortrait = nullptr;
    // The race frame behind the panel, and which surface it was made from.
    sdl2::TexturePtr backdrop;
    SDL_Surface* backdropFrom = nullptr;
    std::vector<Label> standingLabels;
    // The draw that settles the pairs the player is not in. Seeded per night.
    srrace::RaceRng bracketRng {1u};
    // The key or click that left the previous screen still has its release to come.
    bool sawPressHere = false;
    // Filled in from the race just run, for the lines that quote numbers.
    int lossMarginTenths = 0;
    int lastTimeTenths = 0;
    // The round screen arrives in four pieces from four sides, as FUN_002552c8's four FUN_00254996 calls do.
    enum SlidePiece { SLIDE_PANEL, SLIDE_BANNER, SLIDE_CAR, SLIDE_PORTRAIT, SLIDE_COUNT };
    std::array<int, SLIDE_COUNT> slideLeft {};
    bool DrawRaceBackdrop();
    [[nodiscard]] float CarScale() const;
    void StartSlides();
    [[nodiscard]] bool Sliding() const;
    // Pushes a viewport translated by this piece's remaining travel; the
    // matching EndPiece puts the old one back.
    bool BeginPiece(SlidePiece piece, SDL_Rect& saved) const;
    void EndPiece(bool pushed, const SDL_Rect& saved) const;
    // Street Rod 2's 61 message records, read from its executable. Empty when
    // that is not available, and then the built-in English stands in.
    mutable std::vector<std::vector<std::string>> messages;
    [[nodiscard]] std::string Line(int id, size_t line = 0) const;
    [[nodiscard]] std::string Sr2String(uint32_t fileOffset) const;
    void ShowResultBubble();
};
