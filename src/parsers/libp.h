#pragma once

#include <tuple>
#include <vector>

//#include "../include/libamivideo/amivideotypes.h"
//#include "../include/libamivideo/screen.h"

using namespace std;

enum AmigaLibAsset {
    SPEECH_BUBBLE_ARROW_RIGHT = 0,
    SPEECH_BUBBLE_ARROW_LEFT = 1,
    SPEECH_BUBBLE_ARROW_LEFT_TWO_ROWS = 2,
    RACING_CRASHED_INTERIOR = 3,
    SMOKE_PUFF = 4,
    NEWSPAPER_MENU = 5,
    NEWSPAPER_DODGERS_MALIBU = 6,
    NEWSPAPER_YOUTH_KILLED = 7,
    NEWSPAPER_OSCAR_NOMINATION = 8,
    NEWSPAPER_GM_FORD_LAUGH_AT_JAPAN = 9,
    NEWSPAPER_NO_SOLDIERS_IN_VIETNAM = 10,
    RACING_INTERIOR_1 = 11,
    RACING_CLOUDS = 12,
    RACING_MOUNTAINS = 13,
    RACING_SIGN_COUNTRY_LINE = 14,
    RACING_SIGN_GARAGE = 15,
    RACING_SIGN_GUS_GAS = 16,
    RACING_DRIVE_IN = 17,
	RACING_WINDSHIELD_BEAM = 18,
	RACING_HOOD = 19,
	RACING_CEILING = 20,
	RACING_PALM_TREE = 21,
	RACING_UNKNOWN_THING1 = 22,
	RACING_UNKNOWN_THING2 = 23,
    RACING_SIGN_ROAD_WORK_AHEAD = 24,
    RACING_BUSH = 25,
    RACING_SIGN_SPEED_LIMIT_35 = 26,
    RACING_OBSTRUCTION = 27,
    RACING_ROCK1 = 28,
	RACING_ROCK2 = 29,
    RACING_TELEPHONE_POLES = 30,
    RACING_TREE1 = 31,
    RACING_TREE2 = 32,
    RACING_SIGN_ROUTE_69 = 33,
    RACING_SIGN_CITY_LIMITS = 34,
    RACING_SIGN_EXIT_48 = 35,
    RACING_SIGN_MERGE = 36,
    RACING_SIGN_TURN_LEFT = 37,
    RACING_SIGN_TURN_RIGHT = 38,
    RACING_SIGN_MULTITURN_LEFT = 39,
    RACING_SIGN_MULTITURN_RIGHT = 40,
    RACING_UNKNOWN_THING3 = 41,
    RACING_UNKNOWN_BOX = 42,
    RACING_SIGN_TRY_VEGAS_GAMBLER = 43,
    RACING_SIGN_PLAY_BLOCKOUT = 44,
    RACING_RACING_TACHOMETER = 45,
    RACING_GEARBOX_AUTO = 46,
    RACING_GEARBOX_MANUAL_3SPEED = 47,
    RACING_GEARBOX_MANUAL_4SPEED = 48,
	RACING_GEARBOX_KNOB_3SPEED = 49,
	RACING_GEARBOX_KNOB_4SPEED = 50,
	RACING_UNKNOWN_THING4 = 51,
	RACING_WHEEL_LEFT_DETAIL = 52,
	RACING_WHEEL_RIGHT_DETAIL = 53,
	//
    RACING_RACING_DICE1 = 73,
    RACING_RACING_DICE2 = 74,
    RACING_RACING_DICE3 = 75,
    RACING_RACING_DICE4 = 76,
    RACING_RACING_DICE5 = 77,
    //
    RACING_RACING_SPEEDING_TICKET = 124,
    RACING_RACING_SPEEDING_TICKET_COP_HAND = 125,
    //
    NEWSPAPER_EXIT = 177,
    //
    GIRLFRIEND_ANIMATION = 297,
    KISS_ANIMATION = 298,
    WIN_SCREEN = 299,
    ROSTER_SCREEN_CALIFORNIA_HIGHWAY_PATROL = 300,
    //
    DINER_SCREEN = 305,
    CAR_GAS_CAP_FILLING = 307,
    CAR_GAS_CAP = 308,
    //
    CAR_SCOOP1 = 310,
    CAR_SCOOP2 = 311,
    CAR_SCOOP3 = 312,
    CAR_SCOOP4 = 313,
    //
    DMV_SCREEN = 318,
    CALENDAR_AUGUST_SCREEN = 319,
    CALENDAR_JULY_SCREEN = 320,
    //
    GUS_GAS_SCREEN = 340,
    GUS_GAS_MASK = 341,
    //
    GARAGE_SCREEN = 365,
};

// An entry representing a packed bitmap
struct LibBitmapEntry_t {
    uint16_t width;
    uint16_t height;
    uint8_t depth;
    uint8_t unknown1;
    uint8_t rleDictionary[16];
    uint32_t offset;
    uint16_t sizePacked;
};

// hard-coded in SR.EXE / SRSE.EXE @ 0x3CA23
const int COUNT[] = {1, 1, 2, 3, 4, 2, 3, 5, 6, 6, 4, 7, 7, 5, 8, 8};
const unsigned char VALUE[] = {0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0xFF, 0xFF, 0x00};

class BitmapParser {
   private:
    int8_t readUint8(uint8_t *result, FILE *f);
    int8_t readUint16BigEndian(uint16_t *result, FILE *f);
    int8_t readUint32BigEndian(uint32_t *result, FILE *f);
    void printEntry(int number, LibBitmapEntry_t *entry);
    void printBitplanes(uint8_t *buf, size_t size);

   public:
    std::vector<LibBitmapEntry_t> GetEntries(char *filename);

    bool GetPackedData(char *filename, uint8_t *buf, LibBitmapEntry_t *bitmapEntry, int totalEntriesCount);
    vector<uint8_t> UnpackData(const vector<pair<int, uint8_t>> &rle, const LibBitmapEntry_t &sprite, const vector<uint8_t> &data);
    // vector<SDL_Surface *> ReadAllImages(char *filename);
    // SDL_Surface *ReadImage(char *filename, LibBitmapEntry_t entry, int index);
    void OldUnpack(LibBitmapEntry_t *bitmapEntry, uint8_t *data, SDL_Surface *surface);
};
