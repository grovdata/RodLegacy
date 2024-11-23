#pragma once

#include <tuple>
#include <vector>
#include <fstream>
#include <cstdint>
#include <iomanip>

using namespace std;
using Color = tuple<uint8_t, uint8_t, uint8_t>;
using Palette = vector<Color>;

enum AmigaLibFile {
    LIBSTART = 0,
    LIBP = 1
};

enum LibAssetType {
    LibAssetRegular,
    LibAssetMask,
    LibAssetSpritesheet,
    LibAssetInvalid
};

enum AssetLocation {
    AssetLocationAmigaLibStart = 0,
    AssetLocationAmigaLibP = 1
};

enum CarDecal {
    CarDecalHotLips = 0,
    CarDecalRanger = 1,
    CarDecalNotBulletProof = 2,
    CarDecalHotRod = 3,
    CarDecalDestinyCanWait = 4,
    CarDecalLoveBandit = 5,
    CarDecalBigChief = 6,
    CarDecalImNotACop = 7,
    CarDecalCount,
    CarDecalNone,
};

enum CarVendor_t {
    CarVendorChrysler = 0,
    CarVendorFord = 1,
    CarVendorGM = 2,
    CarVendorCustom = 3
};

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
    RACING_UNKNOWN_THING5 = 178,
    CHEVROLET_2DR_COUPE_1940_BODY = 179,
    CHEVROLET_2DR_COUPE_1940_CHOPPED_ROOF = 180,
    CHEVROLET_2DR_COUPE_1940_CHOPPED_REARBUMPER = 181,
    CHEVROLET_2DR_COUPE_1940_CHOPPED_FRONTBUMPER = 182,
    CHEVROLET_ROADSTER_1940_BODY = 183,
    CHEVROLET_ROADSTER_1940_CHOPPED_ROOF = 184,
    CHEVROLET_ROADSTER_1940_CHOPPED_REARBUMPER = 185,
    CHEVROLET_ROADSTER_1940_CHOPPED_FRONTBUMPER = 186,
    CHEVROLET_MASTER_DELUXE_2DR_1938_BODY = 187,
    CHEVROLET_MASTER_DELUXE_2DR_1938_CHOPPED_ROOF = 188,
    CHEVROLET_MASTER_DELUXE_2DR_1938_CHOPPED_REARBUMPER = 188,
    CHEVROLET_MASTER_DELUXE_2DR_1938_CHOPPED_FRONTBUMPER = 189,
    CHEVROLET_CORVETTE_1956_BODY = 191,
    CHEVROLET_CORVETTE_1956_CHOPPED_REARBUMPER = 192,
    CHEVROLET_CORVETTE_1956_CHOPPED_FRONTBUMPER = 193,
    CHEVROLET_CORVETTE_1961_BODY = 194,
    CHEVROLET_CORVETTE_1961_CHOPPED_REARBUMPER = 195,
    CHEVROLET_CORVETTE_1961_CHOPPED_FRONTBUMPER = 196,
    CHEVROLET_CORVETTE_1963_BODY = 197,
    CHEVROLET_CORVETTE_1963_CHOPPED_REARBUMPER = 198,
    CHEVROLET_CORVETTE_1963_CHOPPED_FRONTBUMPER = 199,
//
    FORD_FAIRLANE_500_1956_BODY = 224,
    FORD_FAIRLANE_500_1956_CHOPPED_ROOF = 225,
    FORD_FAIRLANE_500_1956_CHOPPED_REARBUMPER = 226,
    FORD_FAIRLANE_500_1956_CHOPPED_FRONTBUMPER = 227,
//
    FORD_THUNDERBIRD_1957_BODY = 256,
    FORD_THUNDERBIRD_1957_CHOPPED_ROOF = 257,
    FORD_THUNDERBIRD_1957_CHOPPED_REARBUMPER = 258,
    FORD_THUNDERBIRD_1957_CHOPPED_FRONTBUMPER = 259,
    //
    DECALS = 280,
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
    CAR_TIRES_BRAND = 321,
    CAR_TIRES_RACING = 322,
    CAR_TIRES_STOCK = 323,

    //
    GUS_GAS_SCREEN = 340,
    GUS_GAS_MASK = 341,
    DRIVER_01 = 342,
    DRIVER_02 = 343,
    DRIVER_03 = 344,
    DRIVER_04 = 345,
    DRIVER_05 = 346,
    DRIVER_06 = 347,
    DRIVER_07 = 348,
    DRIVER_08 = 349,
    DRIVER_09 = 350,
    DRIVER_10 = 351,
    DRIVER_11 = 352,
    DRIVER_12 = 353,
    DRIVER_13 = 354,
    DRIVER_14 = 355,
    DRIVER_15 = 356,
    DRIVER_16 = 357,
    DRIVER_17 = 358,
    DRIVER_18 = 359,
    DRIVER_19 = 360,
    DRIVER_20 = 361,
    DRIVER_21 = 362,
    DRIVER_22 = 363,
    DRIVER_THE_KING = 364,
    //
    GARAGE_SCREEN = 365,
    //
    PHONEBOOK_GUY = 374
};

enum StockCarModels {
    CHEVROLET_2DR_COUPE_1940 = 179,
    CHEVROLET_ROADSTER_1940 = 183,
    CHEVROLET_MASTER_DELUXE_2DR_1938 = 187,
    CHEVROLET_CORVETTE_1956 = 191,
    CHEVROLET_CORVETTE_1961 = 194,
    CHEVROLET_CORVETTE_1963 = 197,
    DODGE_CUSTOM_ROYAL_LANCER_1955 = 200,
    FORD_DEUCE_COUPE_1932 = 204,
    DODGE_POLARA_1962 = 206,
    PLYMOUTH_SAVOY_2DR_1962 = 210,
    FORD_SEDAN_1932 = 214,
    FORD_DELUXE_2DR_1940 = 216,
    FORD_FAIRLANE_VICTORIA_1955 = 220,
    FORD_FAIRLANE_500_1956 = 224,
    MERCURY_MONTEREY_1954 = 228,
    MERCURY_CUSTOM_2DR_1956 = 232,
    CHEVROLET_STYLELINE_2DR_1949 = 236,
    CHEVROLET_IMPALA_1958 = 240,
    OLDSMOBILE_88_2DR_CUSTOM_1949 = 244,
    OLDSMOBILE_88_CUSTOM_1952 = 248,
    PONTIAC_SILVER_STREAK_1950 = 252,
    FORD_THUNDERBIRD_1957 = 256,
    VALIANT_V200_1961 = 260,
    VALIANT_V100_2DR_1963 = 264,
    CHEVROLET_BELAIR_SPORTS_COUPE = 268,
    THE_KING_CHEVROLET_CORVETTE_1963 = 272
};

#pragma pack(push, 1)
struct BitmapEntry_t {
    uint16_t width;
    uint16_t height;
    uint8_t depth;
    uint8_t unknown1;
    uint8_t rleDictionary[16];
    uint32_t offset;
    uint16_t sizePacked;
};
#pragma pack(pop)

struct BitmapOffset_t {
    uint8_t offsetXRearTire;
    uint8_t offsetXFrontTire;
    uint8_t offsetXDriver;
    uint8_t offsetYDriver;
    uint8_t offsetXChoppedRoof;
    uint8_t offsetXAirScoop;
    uint8_t offsetYAirScoop;
    uint8_t offsetYRearBumper;
    uint8_t offsetXFrontBumper;
    uint8_t offsetYFrontBumper;
    uint8_t offsetXDecal;
    uint8_t offsetYDecal;
};

struct ClassifiedEntry_t {
    int year;
    string carName;
    string adText;
    int price;
};

struct CarMetadata_t {
    BitmapOffset_t offsets;
    ClassifiedEntry_t classified;
};

// hard-coded in SR.EXE / SRSE.EXE @ 0x3CA23
const int COUNT[] = {1, 1, 2, 3, 4, 2, 3, 5, 6, 6, 4, 7, 7, 5, 8, 8};
const unsigned char VALUE[] = {0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0xFF, 0xFF, 0x00};

#define RLE       {{1, 0x0}, {1, 0xff}, {2, 0x0}, {3, 0x0}, {2, 0xff}, {4, 0x0}, {5, 0x0}, {3, 0xff},	{6, 0x0}, {4, 0xff}, {7, 0x0}, {5, 0xff}, {8, 0x0}, {6, 0xff}, {9, 0x0}, {7, 0xff}}
#define PAL1      {0x0000, 0x0810, 0x0911, 0x0B21, 0x0D22, 0x0F32, 0x0F44, 0x0F65, 0x0F87, 0x0F98, 0x0FFF, 0x0EEE, 0x0CCC, 0x0BBB, 0x0AAA, 0x0999, 0x0888, 0x0950, 0x0C71, 0x0F96, 0x0333, 0x0222, 0x005B, 0x007F, 0x00F0, 0x00D0, 0x0FE9, 0x0FD2, 0x0B21, 0x0F32, 0x009F, 0x00BF}
#define PAL_GAS   {0x0000, 0x0810, 0x0911, 0x0B21, 0x0D22, 0x0F32, 0x0F44, 0x0F65, 0x0F87, 0x0F98, 0x0FFF, 0x0EEE, 0x0CCC, 0x0BBB, 0x0AAA, 0x0999, 0x0888, 0x0950, 0x0C71, 0x0F96, 0x0333, 0x0222, 0x005B, 0x007F, 0x00F0, 0x00B0, 0x0FE9, 0x0FD2, 0x0B21, 0x0F32, 0x0F96, 0x0C63}
#define CAR_INDEX ((int[]){179, 183, 187, 191, 194, 197, 200, 204, 206, 210, 214, 216, 220, 224, 228, 232, 236, 240, 244, 248, 252, 256, 260, 264, 268}) // King: 272
#define CAR_INDEX_COUNT sizeof(CAR_INDEX) / sizeof(int)
#define CAR_PLAYER_MODEL_COUNT 25
#define DRIVER_COUNT 22

// Helper functions
uint8_t  ReadUint8(std::ifstream& file);
uint16_t ReadUint16BigEndian(std::ifstream& file);
uint32_t ReadUint32BigEndian(std::ifstream& file);
string ReadNullTerminatedString(ifstream& file);

class BitmapParser {
   private:

    Palette pal1;



    void                                            PrintBitmapEntry(int number, BitmapEntry_t entry);
    void                                            PrintBitplanes(const std::vector<uint16_t>& data);

   public:

	pair<int, vector<BitmapEntry_t>>                ReadEntries(const string& filename);
    vector<Color>                                   PlanarToChunky(const std::vector<uint8_t>& data, BitmapEntry_t entry, const Palette& palette, const Palette& carPalette = {});
    vector<uint8_t>                                 GetPackedData(const string& filename, const BitmapEntry_t& bitmapEntry, int totalEntriesCount);
    vector<uint8_t>                                 UnpackData(const vector<pair<int, uint8_t>> &rle, const BitmapEntry_t &sprite, const vector<uint8_t> &data);
    vector<uint8_t>                                 ReadImage(const string& filename, BitmapEntry_t entry, int index, int totalEntriesCount);
    vector<pair<BitmapEntry_t, vector<uint8_t>>>    ReadAllImages(const string& filename);

    Palette                                         PreparePalette(const vector<uint16_t>& palette, bool ehb);
    Palette                                         PrepareCarPalette(const vector<uint16_t>& palette);
    Color                                           GetRGBFromRGB4(uint16_t rgb4);
    vector<uint16_t>                                GetPalette(AssetLocation location, int index);

    LibAssetType                                    GetAssetType(AssetLocation location, int index);

    pair<int, int>                                  GetSpritesheetInfo(AssetLocation location, int index);
    void                                            SavePaletteToBMP(const Palette& palette, const std::string& filename);
};

class DataParser {
    private:
        vector<pair<BitmapEntry_t, BitmapOffset_t>> offsets;
        void CleanupCarString(string& str);
    public:
        vector<CarMetadata_t>  ReadEntries(const string& filename);
        int                    GetCarIndex(AmigaLibAsset asset);
};