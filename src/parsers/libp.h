#ifndef LIBP_H
#define LIBP_H

/*
    SrLib: 2024 by grovdata
    This library reads and writes assets for the Street Rod games.

    SrLib is a fan project and is not affiliated with streetrodonline.com
    who owns the Street Rod copyright.

*/

#include <tuple>
#include <vector>
#include <map>
#include <fstream>
#include <cstdint>
#include <iomanip>
#include <string>

// lodepng is built as plain C (LODEPNG_NO_COMPILE_CPP); wrap for C++ linkage.
#include <cstring>
#include <cstdlib>
extern "C" {
#include "lodepng.h"
}

enum SrLibFile {
    SrLibFileAmigaLibStart = 0,
    SrLibFileAmigaLibP = 1,
    SrLibFileDosLib1 = 2,
    SrLibFileDosLib1c = 3,
    SrLibFileDosLib2 = 4,
};


enum SrLibArchive {
    SrLibArchiveSR1AmigaDisk1 = 0,
    SrLibArchiveSR1AmigaDisk2 = 1,
    SrLibArchiveSR2AmigaDisk1 = 2,
    SrLibArchiveSR2AmigaDisk2 = 3,
};

struct SrLibColor {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};

using Palette = std::vector<SrLibColor>;
const SrLibColor AmigaMaskColor = {128, 16, 0, 255};

static const SrLibColor DOS_PALETTE[16] = {
    {0x00, 0x00, 0x00, 0xFF},  // 0: Black
    {0x00, 0x00, 0x00, 0xFF},  // 1: Black
    {0x55, 0x55, 0x55, 0xFF},  // 2: Dark Gray
    {0xAA, 0xAA, 0xAA, 0xFF},  // 3: Light Gray
    {0xFF, 0xFF, 0xFF, 0xFF},  // 4: White
    {0xAA, 0xAA, 0xAA, 0xFF},  // 5: Light Gray
    {0x55, 0x55, 0xFF, 0xFF},  // 6: Light Blue
    {0x00, 0x00, 0xAA, 0xFF},  // 7: Blue
    {0x00, 0xAA, 0xAA, 0xFF},  // 8: Cyan
    {0x00, 0xAA, 0x00, 0xFF},  // 9: Green
    {0xAA, 0x00, 0x00, 0xFF},  // 10: Red
    {0xFF, 0x55, 0x55, 0xFF},  // 11: Light Red
    {0xAA, 0x00, 0xAA, 0xFF},  // 12: Magenta
    {0x55, 0xFF, 0x55, 0xFF},  // 13: Light Green
    {0xAA, 0x55, 0x00, 0xFF},  // 14: Brown
    {0xFF, 0xFF, 0x55, 0xFF},  // 15: Yellow
};

extern const char *SrLibFileNames[];
extern const uint32_t SrLibFileChecksums[];
extern const char *SrLibArchiveNames[];
extern const uint32_t SrLibArchiveChecksums[];

enum LibAssetType {
    LibAssetRegular,
    LibAssetMask,
    LibAssetSpritesheet,
    LibAssetInvalid
};

enum SrLibGameScene {
    SCENE_GARAGE = 0,
    SCENE_DINER = 1,
    SCENE_GAS_STATION = 2,
    SCENE_CRUISING = 3,
    SCENE_DRIVING = 4,
    SCENE_NEWSPAPER = 5,
    SCENE_KING_OF_COUNTY = 6,
};

enum SrLibCarDecal {
    SrLibCarDecalAmigaHotLips = 0,
    SrLibCarDecalAmigaRanger = 1,
    SrLibCarDecalAmigaNotBulletProof = 2,
    SrLibCarDecalAmigaHotRod = 3,
    SrLibCarDecalAmigaDestinyCanWait = 4,
    SrLibCarDecalAmigaLoveBandit = 5,
    SrLibCarDecalAmigaBigChief = 6,
    SrLibCarDecalAmigaImNotACop = 7,
    SrLibCarDecalDosSeDecal1 = 8, // USA 1
    SrLibCarDecalDosSeDecal2 = 9, // Eagle
    SrLibCarDecalDosSeDecal3 = 10, // Not 4 Sale
    SrLibCarDecalDosSeDecal4 = 11, // Veteran
    SrLibCarDecalDosSeDecal5 = 12, // Hell
    SrLibCarDecalDosSeDecal6 = 13, // Hawaii
    SrLibCarDecalDosSeDecal7 = 14, // Banana Club
    SrLibCarDecalDosSeDecal8 = 15, // Explosive
    SrLibCarDecalCount,
    SrLibCarDecalNone,
    SrLibCarDecalCustom,
};

extern const char* SrLibCarDecalNames[16];

enum CarVendor_t {
    CarVendorChrysler = 0,
    CarVendorFord = 1,
    CarVendorGM = 2,
    CarVendorCustom = 3,
    CarVendorUnknown = 4
};

// Engines in Street Rod 1
enum CarEngine_t {
    CarEngineChrysler_V6 = 0,
    CarEngineChrysler_V8_285 = 1,
    CarEngineChrysler_V8_340 = 2,
    CarEngineGM_V6 = 3,
    CarEngineGM_V8_282 = 4,
    CarEngineGM_V8_327 = 5,
    CarEngineFord_V6 = 6,
    CarEngineFord_V8_292 = 7,
    CarEngineFord_V8_351 = 8
};

enum AmigaLibStartAsset {
    AMIGA_LIB_START_TITLE_SCREEN = 0,
    AMIGA_LIB_START_MOUSE_POINTERS = 1,
    AMIGA_LIB_START_CALIFORNIA_DREAMS_LOGO = 2,
    AMIGA_LIB_START_CALIFORNIA_DREAMS_NUMBER_PLATE = 3,
    AMIGA_LIB_START_KING_STREET_RODDERS = 4,
    AMIGA_LIB_START_GIRLFRIEND_ANIMATION = 5,
    AMIGA_LIB_START_KISS_ANIMATION = 6,
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
    RACING_SIGN_COUNTY_LINE = 14,
    RACING_SIGN_GARAGE = 15,
    RACING_SIGN_GUS_GAS = 16,
    RACING_DRIVE_IN = 17,
	RACING_WINDSHIELD_BEAM = 18,
	RACING_HOOD = 19,
	RACING_CEILING = 20,
	RACING_PALM_TREE = 21,
	RACING_ROAD_POST = 22,
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
    RACING_REARVIEWMIRROR_FRAME = 42,
    RACING_SIGN_TRY_VEGAS_GAMBLER = 43,
    RACING_SIGN_PLAY_BLOCKOUT = 44,
    RACING_RACING_CLOCK_11_10 = 45,    // Clock for automatic transmission (cruising in town)
    RACING_GEARBOX_AUTO = 46,
    RACING_GEARBOX_MANUAL_3SPEED = 47,
    RACING_GEARBOX_MANUAL_4SPEED = 48,
	RACING_GEARBOX_KNOB_3SPEED = 49,
	RACING_GEARBOX_KNOB_4SPEED = 50,
	RACING_GEARBOX_KNOB_AUTOMATIC = 51,
	RACING_WHEEL_LEFT_DETAIL = 52,
	RACING_WHEEL_RIGHT_DETAIL = 53,
    RACING_WHEEL_LEFT_DETAIL2 = 54,
    RACING_WHEEL_RIGHT_DETAIL2 = 55,
    RACING_WHEEL_CENTER_CENTERED = 56,
    RACING_WHEEL_LEFT_HAND_1 = 57,
    RACING_WHEEL_RIGHT_HAND_1 = 58,
    RACING_WHEEL_RIGHT_HAND_2 = 59,
    RACING_WHEEL_CENTER_TURNING_LEFT_OPPOSITE_1 = 60,
    RACING_WHEEL_CENTER_TURNING_LEFT = 61,
    RACING_WHEEL_RIGHT_HAND_3 = 62,
    RACING_WHEEL_RIGHT_HAND_4 = 63,
    RACING_WHEEL_LEFT_HAND_2 = 64,
    RACING_WHEEL_LEFT_HAND_3 = 65,
    RACING_WHEEL_LEFT_HAND_4 = 66,
    RACING_WHEEL_CENTER_TURNING_RIGHT = 67,
    RACING_WHEEL_CENTER_TURNING_RIGHT_OPPOSITE_1 = 68,
    RACING_WHEEL_CENTER_TURNING_RIGHT_OPPOSITE_2 = 69,
    RACING_WHEEL_CENTER_TURNING_RIGHT_OPPOSITE_3 = 70,
    RACING_RIGHT_HAND_SHIFTING = 71,
	RACING_WHEEL_LEFT_DETAIL4 = 72,
    RACING_RACING_DICE1 = 73,
    RACING_RACING_DICE2 = 74,
    RACING_RACING_DICE3 = 75,
    RACING_RACING_DICE4 = 76,
    RACING_RACING_DICE5 = 77,
    RACING_RACING_CLOCK_11_50 = 78,    // Clock for automatic transmission (Road Race)
    RACING_RACING_CLOCK_01_32 = 79,    // Clock for automatic transmission (Drag Race)
    RACING_RACING_CLOCK_02_55 = 80,    // Clock for automatic transmission (unknown mode)
    RACING_HORIZON_1 = 81,
    RACING_HORIZON_REARVIEWMIRROR = 82,
    RACING_OPPONENT_CAR_CHEVY_FRONT_SIDE = 83,
    RACING_OPPONENT_CAR_CHEVY_REAR_SIDE = 84, // Looks like Impala
    RACING_OPPONENT_CAR_CHEVY_REAR_1 = 85,
    RACING_OPPONENT_CAR_CHEVY_REAR_2 = 86,
    RACING_OPPONENT_CAR_CHEVY_REAR_3 = 87,
    RACING_OPPONENT_CAR_CHEVY_REAR_4 = 88,
    RACING_OPPONENT_CAR_CHEVY_REAR_5 = 89,
    RACING_OPPONENT_CAR_CHEVY_REAR_6 = 90,
    RACING_OPPONENT_CAR_CHEVY_REAR_7 = 91,
    RACING_OPPONENT_CAR_CHEVY_BEHIND_1 = 92,
    RACING_OPPONENT_CAR_CHEVY_BEHIND_2 = 93,
    RACING_OPPONENT_CAR_CHEVY_BEHIND_3 = 94,
    RACING_OPPONENT_CAR_CHEVY_BEHIND_4 = 95,
    RACING_OPPONENT_CAR_OLDSMOBILE_FRONT_SIDE = 96, // ?? Need to verify
    RACING_RED_LIGHTS = 97,
    RACING_OPPONENT_CAR_CHRYSLER_FRONT_SIDE = 98,
    RACING_OPPONENT_CAR_CHRYSLER_REAR_SIDE = 99, // Looks like Bel Air
    RACING_OPPONENT_CAR_CHRYSLER_REAR_1 = 100,
    RACING_OPPONENT_CAR_CHRYSLER_REAR_2 = 101,
    RACING_OPPONENT_CAR_CHRYSLER_REAR_3 = 102,
    RACING_OPPONENT_CAR_CHRYSLER_REAR_4 = 103,
    RACING_OPPONENT_CAR_CHRYSLER_REAR_5 = 104,
    RACING_OPPONENT_CAR_CHRYSLER_REAR_6 = 105,
    RACING_OPPONENT_CAR_CHRYSLER_REAR_7 = 106,
    RACING_OPPONENT_CAR_CHRYSLER_BEHIND_1 = 107,
    RACING_OPPONENT_CAR_CHRYSLER_BEHIND_2 = 108,
    RACING_OPPONENT_CAR_CHRYSLER_BEHIND_3 = 109,
    RACING_OPPONENT_CAR_CHRYSLER_BEHIND_4 = 110,
    RACING_OPPONENT_CAR_COP_FRONT_SIDE = 111,
    RACING_OPPONENT_CAR_COP_REAR_SIDE = 112,
    RACING_OPPONENT_CAR_COP_REAR_1 = 113,
    RACING_OPPONENT_CAR_COP_REAR_2 = 114,
    RACING_OPPONENT_CAR_COP_REAR_3 = 115,
    RACING_OPPONENT_CAR_COP_REAR_4 = 116,
    RACING_OPPONENT_CAR_COP_REAR_5 = 117,
    RACING_OPPONENT_CAR_COP_REAR_6 = 118,
    RACING_OPPONENT_CAR_COP_REAR_7 = 119,
    RACING_OPPONENT_CAR_COP_BEHIND_1 = 120,
    RACING_OPPONENT_CAR_COP_BEHIND_2 = 121,
    RACING_OPPONENT_CAR_COP_BEHIND_3 = 122,
    RACING_OPPONENT_CAR_COP_BEHIND_4 = 123,
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
    DODGE_CUSTOM_ROYAL_LANCER_1955_BODY = 200,
    DODGE_CUSTOM_ROYAL_LANCER_1955_CHOPPED_ROOF = 201,
    DODGE_CUSTOM_ROYAL_LANCER_1955_CHOPPED_REARBUMPER = 202,
    DODGE_CUSTOM_ROYAL_LANCER_1955_CHOPPED_FRONTBUMPER = 203,
    FORD_DEUCE_COUPE_1932_BODY = 204,
    FORD_DEUCE_COUPE_1932_CHOPPED_ROOF = 205,
    DODGE_POLARA_1962_BODY = 206,
    DODGE_POLARA_1962_CHOPPED_ROOF = 207,
    DODGE_POLARA_1962_CHOPPED_REARBUMPER = 208,
    DODGE_POLARA_1962_CHOPPED_FRONTBUMPER = 209,
    PLYMOUTH_SAVOY_2DR_1962_BODY = 210,
    PLYMOUTH_SAVOY_2DR_1962_CHOPPED_ROOF = 211,
    PLYMOUTH_SAVOY_2DR_1962_CHOPPED_REARBUMPER = 212,
    PLYMOUTH_SAVOY_2DR_1962_CHOPPED_FRONTBUMPER = 213,
    FORD_SEDAN_1932_BODY = 214,
    FORD_SEDAN_1932_CHOPPED_ROOF = 215,
    FORD_DELUXE_2DR_1940_BODY = 216,
    FORD_DELUXE_2DR_1940_CHOPPED_ROOF = 217,
    FORD_DELUXE_2DR_1940_CHOPPED_REARBUMPER = 218,
    FORD_DELUXE_2DR_1940_CHOPPED_FRONTBUMPER = 219,
    FORD_FAIRLANE_VICTORIA_1955_BODY = 220,
    FORD_FAIRLANE_VICTORIA_1955_CHOPPED_ROOF = 221,
    FORD_FAIRLANE_VICTORIA_1955_CHOPPED_REARBUMPER = 222,
    FORD_FAIRLANE_VICTORIA_1955_CHOPPED_FRONTBUMPER = 223,
    FORD_FAIRLANE_500_1956_BODY = 224,
    FORD_FAIRLANE_500_1956_CHOPPED_ROOF = 225,
    FORD_FAIRLANE_500_1956_CHOPPED_REARBUMPER = 226,
    FORD_FAIRLANE_500_1956_CHOPPED_FRONTBUMPER = 227,
    MERCURY_MONTEREY_1954_BODY = 228,
    MERCURY_MONTEREY_1954_CHOPPED_ROOF = 229,
    MERCURY_MONTEREY_1954_CHOPPED_REARBUMPER = 230,
    MERCURY_MONTEREY_1954_CHOPPED_FRONTBUMPER = 231,
    MERCURY_CUSTOM_2DR_1956_BODY = 232,
    MERCURY_CUSTOM_2DR_1956_CHOPPED_ROOF = 233,
    MERCURY_CUSTOM_2DR_1956_CHOPPED_REARBUMPER = 234,
    MERCURY_CUSTOM_2DR_1956_CHOPPED_FRONTBUMPER = 235,
    CHEVROLET_STYLELINE_2DR_1949_BODY = 236,
    CHEVROLET_STYLELINE_2DR_1949_CHOPPED_ROOF = 237,
    CHEVROLET_STYLELINE_2DR_1949_CHOPPED_REARBUMPER = 238,
    CHEVROLET_STYLELINE_2DR_1949_CHOPPED_FRONTBUMPER = 239,
    CHEVROLET_IMPALA_1958_BODY = 240,
    CHEVROLET_IMPALA_1958_CHOPPED_ROOF = 241,
    CHEVROLET_IMPALA_1958_CHOPPED_REARBUMPER = 242,
    CHEVROLET_IMPALA_1958_CHOPPED_FRONTBUMPER = 243,
    OLDSMOBILE_88_2DR_CUSTOM_1949_BODY = 244,
    OLDSMOBILE_88_2DR_CUSTOM_1949_CHOPPED_ROOF = 245,
    OLDSMOBILE_88_2DR_CUSTOM_1949_CHOPPED_REARBUMPER = 246,
    OLDSMOBILE_88_2DR_CUSTOM_1949_CHOPPED_FRONTBUMPER = 247,
    OLDSMOBILE_88_CUSTOM_1952_BODY = 248,
    OLDSMOBILE_88_CUSTOM_1952_CHOPPED_ROOF = 249,
    OLDSMOBILE_88_CUSTOM_1952_CHOPPED_REARBUMPER = 250,
    OLDSMOBILE_88_CUSTOM_1952_CHOPPED_FRONTBUMPER = 251,
    PONTIAC_SILVER_STREAK_1950_BODY = 252,
    PONTIAC_SILVER_STREAK_1950_CHOPPED_ROOF = 253,
    PONTIAC_SILVER_STREAK_1950_CHOPPED_REARBUMPER = 254,
    PONTIAC_SILVER_STREAK_1950_CHOPPED_FRONTBUMPER = 255,
    FORD_THUNDERBIRD_1957_BODY = 256,
    FORD_THUNDERBIRD_1957_CHOPPED_ROOF = 257,
    FORD_THUNDERBIRD_1957_CHOPPED_REARBUMPER = 258,
    FORD_THUNDERBIRD_1957_CHOPPED_FRONTBUMPER = 259,
    VALIANT_V200_1961_BODY = 260,
    VALIANT_V200_1961_CHOPPED_ROOF = 261,
    VALIANT_V200_1961_CHOPPED_REARBUMPER = 262,
    VALIANT_V200_1961_CHOPPED_FRONTBUMPER = 263,
    VALIANT_V100_2DR_1963_BODY = 264,
    VALIANT_V100_2DR_1963_CHOPPED_ROOF = 265,
    VALIANT_V100_2DR_1963_CHOPPED_REARBUMPER = 266,
    VALIANT_V100_2DR_1963_CHOPPED_FRONTBUMPER = 267,
    CHEVROLET_BELAIR_SPORTS_COUPE_BODY = 268,
    CHEVROLET_BELAIR_SPORTS_COUPE_CHOPPED_ROOF = 269,
    CHEVROLET_BELAIR_SPORTS_COUPE_CHOPPED_REARBUMPER = 270,
    CHEVROLET_BELAIR_SPORTS_COUPE_CHOPPED_FRONTBUMPER = 271,
    THE_KING_CHEVROLET_CORVETTE_1963_BODY = 272,
    ENGINE_BELT_ANIMATION = 273,
    ENGINE_PISTON_LEFT_ANIMATION = 274,
    ENGINE_PISTON_RIGHT_ANIMATION = 275,
    ENGINE_CRANK_ANIMATION = 276,
    ENGINE_FLYWHEEL_ANIMATION = 277,
    ENGINE_CAM_ANIMATION = 278,
    CALENDAR_CURRENT_DATE_RECTANGLE = 279,
    DECALS = 280,
    ENGINE_TRANSMISSION_WINDOW = 281,
    ENGINE_TRANSMISSION1 = 282,
    ENGINE_TRANSMISSION2 = 283,
    ENGINE_TRANSMISSION3 = 284,
    ENGINE_TRANSMISSION4 = 285,
    ENGINE_TRANSMISSION_BOLT = 286,
    CAR_WHEELHOUSE = 287,
    ENGINE_WINDOWFRAME = 288,
    ENGINE_V6 = 289,
    ENGINE_V8 = 290,
    ENGINE_V8_BIG = 291,
    ENGINE_WIRE_DETACHED = 292,
    ENGINE_WIRE_ATTACHED = 293,
    ENGINE_BOLT = 294,
    ENGINE_WINDOWBUTTONS = 295,
    KING_STREET_RODDERS = 296,
    GIRLFRIEND_ANIMATION = 297,
    KISS_ANIMATION = 298,
    WIN_SCREEN = 299,
    ROSTER_SCREEN_CALIFORNIA_HIGHWAY_PATROL = 300,
    //
    DINER_SCREEN = 305,
    GUS_GAS_PUMP1 = 306,
    GUS_GAS_PUMP_FILLING = 307,
    CAR_GAS_CAP = 308,
    //
    CAR_SCOOP1 = 310,
    CAR_SCOOP2 = 311,
    CAR_SCOOP3 = 312,
    CAR_SCOOP4 = 313,
    //
    CAR_SHADOW = 316,
    ENGINE_TUNING_SCREEN = 317,
    DMV_SCREEN = 318,
    CALENDAR_AUGUST_SEPTEMBER_SCREEN = 319,
    CALENDAR_JUNE_JULY_SCREEN = 320,
    CAR_TIRES_BRAND = 321,
    CAR_TIRES_RACING = 322,
    CAR_TIRES_STOCK = 323,
    ENGINE_WINDOWBUTTONS_DONE = 324,
    ENGINE_CARB_2BARREL = 325,
    ENGINE_CARB_4BARREL = 326,
    ENGINE_CARB_4BARREL_RACING = 327,
    ENGINE_MANIFOLD1 = 328,
    ENGINE_MANIFOLD2 = 329,
    ENGINE_MANIFOLD3 = 330,
    ENGINE_MANIFOLD4 = 331,
    ENGINE_MANIFOLD5 = 332,
    ENGINE_MANIFOLD6 = 333,
    DINER_SCREEN_MASK = 334,
    DINER_WAITRESS_ANIM_LEFT = 335,
    DINER_WAITRESS_ANIM_RIGHT = 336,
    GUS_GAS_ATTENDANT = 337,
    GUS_GAS_PUMP2 = 338,
    GUS_GAS_PUMP3 = 339,
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
    GARAGE_SCREEN = 365,
    CAR_JACK_ANIMATION = 366,
    JAIL_MASK = 367,
    JAIL = 368,
    ENGINE_TUNE_METER = 369,
    ENGINE_TUNE_GADGET = 369,   // alias for ENGINE_TUNE_METER
    ENGINE_TUNE_INDICATOR = 370,
    NOTHING_1 = 371,
    NOTHING_2 = 372,
    NOTHING_3 = 373,
    PHONEBOOK_GUY = 374
};

enum DosLib1Asset {
    DOS_LIB1_TITLE_SCREEN = 0,
    DOS_LIB1_MOUSE_POINTERS = 1,
    DOS_LIB1_CALIFORNIA_DREAMS_LOGO = 2,
    DOS_LIB1_CALIFORNIA_DREAMS_NUMBER_PLATE = 3,
    DOS_LIB1_KING_STREET_RODDERS = 4,
    DOS_LIB1_GIRLFRIEND_ANIMATION = 5,
    DOS_LIB1_KISS_ANIMATION = 6,
    DOS_LIB1_JUKEBOX_SCREEN = 7,
    DOS_LIB1_END_CREDITS = 8
};

enum DosLib2Asset {

    DOS_LIB2_SPEEDING_TICKET = 110,

    DOS_LIB2_SE_DECAL_1 = 186,
    DOS_LIB2_SE_DECAL_2 = 187,
    DOS_LIB2_SE_DECAL_3 = 188,
    DOS_LIB2_SE_DECAL_4 = 189,
    DOS_LIB2_SE_DECAL_5 = 190,
    DOS_LIB2_SE_DECAL_6 = 191,
    DOS_LIB2_SE_DECAL_7 = 192,
    DOS_LIB2_SE_DECAL_8 = 193
};


// Track data structures based on Street Rod binary analysis
enum SrTrackType {
    SR_TRACK_ROAD_RACE = 0,  // Complex curved course at 0xE000
    SR_TRACK_DRAG_RACE = 1   // Straight drag strip at 0xDD00
};

enum SrRoadSideSprite {
    SR_SPRITE_RACING_CLOUDS = 12,
    SR_SPRITE_RACING_MOUNTAINS = 13,
    SR_SPRITE_RACING_SIGN_COUNTY_LINE = 14,
    SR_SPRITE_RACING_SIGN_GARAGE = 15,
    SR_SPRITE_RACING_SIGN_GUS_GAS = 16,
    SR_SPRITE_RACING_DRIVE_IN = 17,
    SR_SPRITE_RACING_UNKNOWN_18 = 18,
    SR_SPRITE_RACING_UNKNOWN_19 = 19,
    SR_SPRITE_RACING_UNKNOWN_20 = 20,
    SR_SPRITE_RACING_PALM_TREE = 21,
    SR_SPRITE_RACING_ROAD_POSTS = 22,    // Alternative road posts at 0x10EC1
    SR_SPRITE_RACING_SIGN_ROAD_WORK_AHEAD = 24,
    SR_SPRITE_RACING_BUSH = 25,
    SR_SPRITE_RACING_TELEPHONE_POLE = 30, // Main roadside sprite at 0x10021
    SR_SPRITE_RACING_TREE_31 = 31,
    SR_SPRITE_RACING_TREE_32 = 32,
    SR_SPRITE_RACING_ROAD_POSTS_33 = 33   // Primary road posts (appears with telephone poles)
};

// Track coordinate segment (12 bytes) - raw storage format from binary
struct SrTrackCoordSegment {
    int16_t xStart;    // Start X coordinate (scale by 1/16 for 320x200 screen)
    int16_t xEnd;      // End X coordinate
    int16_t yStart;    // Start Y coordinate
    int16_t yEnd;      // End Y coordinate
    int16_t sprite1;   // First sprite ID (usually 0)
    int16_t sprite2;   // Second sprite ID or environment marker
};

// Roadside sprite placement (8 bytes) - environmental decoration format
struct SrRoadSideSpriteRecord {
    uint16_t spriteId1;      // First sprite ID
    uint16_t trackPosition1; // Position along track (scale by 1/16)
    uint16_t spriteId2;      // Second sprite ID
    uint16_t trackPosition2; // Position along track
};

// TrackSegment C structure (26 bytes) - runtime format used by game engine
struct SrTrackSegment {
    int16_t commandStart;   // Racing command/instruction start
    int16_t commandEnd;     // Racing command/instruction end
    int16_t commandOffset;  // Offset for commands
    int16_t opCode;         // Operation code for segment type
    int16_t trapXMin;       // Collision detection X minimum
    int16_t trapXMax;       // Collision detection X maximum
    int16_t trapYMin;       // Collision detection Y minimum
    int16_t trapYMax;       // Collision detection Y maximum
    int16_t flags;          // Segment flags (bit 1 = end-of-chain)
    uint8_t triggerFlag;    // Trigger conditions
    uint8_t padding[13];    // Padding to 26 bytes total

    // Check if this segment ends the chain
    bool IsEndOfChain() const { return (flags & 2) != 0; }

    // Get bounding box for collision detection
    void GetBoundingBox(int16_t& minX, int16_t& minY, int16_t& maxX, int16_t& maxY) const {
        minX = trapXMin; minY = trapYMin;
        maxX = trapXMax; maxY = trapYMax;
    }
};

// Custom rectangle type for frame definitions
struct SpriteRect {
    int16_t x, y;        // Position in sprite sheet
    uint16_t w, h;       // Dimensions of frame
};

// Animation metadata for sprites with multiple frames
struct SpriteAnimationInfo {
    uint16_t spriteId;           // The sprite asset ID
    uint8_t frameCount;          // Total number of frames
    uint16_t animationSpeed;     // Milliseconds per frame (0 = static)
    bool isLooping;              // Whether animation loops
    const SpriteRect* frames;    // Array of frame rectangles
};

// Misc frame definitions
static const SpriteRect RACING_TREE2_FRAMES[] = {
    {10, 139, 8, 7}, {0, 137, 10, 9}, {56, 133, 11, 11}, {68, 133, 14, 13}, {0, 122, 16, 15}, {15, 122, 19, 17}, {34, 122, 22, 20}, {56, 108, 26, 25}, {47, 76, 34, 32}, {0, 76, 48, 46}, {0, 0, 77, 76}
};

static const SpriteRect RACING_SIGN_COUNTY_LINE_FRAMES[] = {
    {19, 82, 7, 4}, {27, 81, 8, 5}, {36, 80, 9, 6}, {30, 70, 11, 7}, {16, 70, 13, 8}, {0, 69, 15, 10}, {46, 74, 18, 12}, {43, 60, 21, 14}, {38, 43, 26, 17}, {0, 43, 38, 26}, {0, 0, 64, 43}
};

// Engine animation frame definitions
// Belt animation: 2x2 grid of 86x53 frames
static const SpriteRect ENGINE_BELT_FRAMES[] = {
    {0, 0, 86, 53}, {86, 0, 86, 53}, {0, 53, 86, 53}, {86, 53, 86, 53}
};

// Left piston animation: 4x2 grid of 42x55 frames
static const SpriteRect ENGINE_PISTON_LEFT_FRAMES[] = {
    {0, 0, 42, 55}, {42, 0, 42, 55}, {84, 0, 42, 55}, {126, 0, 42, 55},
    {0, 55, 42, 55}, {42, 55, 42, 55}, {84, 55, 42, 55}, {126, 55, 42, 55}
};

// Right piston animation: 4x2 grid of 41x54 frames
static const SpriteRect ENGINE_PISTON_RIGHT_FRAMES[] = {
    {0, 0, 41, 54}, {41, 0, 41, 54}, {82, 0, 41, 54}, {123, 0, 41, 54},
    {0, 54, 41, 54}, {41, 54, 41, 54}, {82, 54, 41, 54}, {123, 54, 41, 54}
};

// Crank animation: 2x2 grid of 71x33 frames
static const SpriteRect ENGINE_CRANK_FRAMES[] = {
    {0, 0, 71, 33}, {71, 0, 71, 33}, {0, 33, 71, 33}, {71, 33, 71, 33}
};

// Flywheel animation: 2x2 grid of 43x34 frames
static const SpriteRect ENGINE_FLYWHEEL_FRAMES[] = {
    {0, 0, 43, 34}, {43, 0, 43, 34}, {0, 34, 43, 34}, {43, 34, 43, 34}
};

// Camshaft animation: 4x2 grid of 39x14 frames
static const SpriteRect ENGINE_CAM_FRAMES[] = {
    {0, 0, 39, 14}, {39, 0, 39, 14}, {78, 0, 39, 14}, {117, 0, 39, 14},
    {0, 14, 39, 14}, {39, 14, 39, 14}, {78, 14, 39, 14}, {117, 14, 39, 14}
};

static const SpriteRect CAR_JACK_FRAMES[] = {
    {0, 0, 32, 29}, {32, 0, 32, 29}, {64, 0, 32, 29}, // Jack frames
    {96, 0, 32, 29} // Last frame is a mask for the garage floor to hide the jack from background
};

// Animation data for known animated sprites
static const SpriteAnimationInfo SPRITE_ANIMATIONS[] = {
    {RACING_TREE2, 11, 150, true, RACING_TREE2_FRAMES},
    {RACING_SIGN_COUNTY_LINE, 11, 150, true, RACING_SIGN_COUNTY_LINE_FRAMES},
    {ENGINE_BELT_ANIMATION, 4, 500, true, ENGINE_BELT_FRAMES},
    {ENGINE_PISTON_LEFT_ANIMATION, 8, 500, true, ENGINE_PISTON_LEFT_FRAMES},
    {ENGINE_PISTON_RIGHT_ANIMATION, 8, 500, true, ENGINE_PISTON_RIGHT_FRAMES},
    {ENGINE_CRANK_ANIMATION, 4, 500, true, ENGINE_CRANK_FRAMES},
    {ENGINE_FLYWHEEL_ANIMATION, 4, 500, true, ENGINE_FLYWHEEL_FRAMES},
    {ENGINE_CAM_ANIMATION, 8, 500, true, ENGINE_CAM_FRAMES},
    {CAR_JACK_ANIMATION, 3, 500, true, CAR_JACK_FRAMES}, // Skip last frame
};

// Helper function to get animation info for a sprite
inline const SpriteAnimationInfo* GetSpriteAnimationInfo(uint16_t spriteId) {
    for (const auto& info : SPRITE_ANIMATIONS) {
        if (info.spriteId == spriteId) {
            return &info;
        }
    }
    return nullptr; // No animation info found
}

// Helper function to get frame rectangle for animation frame
inline SpriteRect GetAnimationFrameRect(const SpriteAnimationInfo* info, uint8_t frameIndex) {
    if (!info || frameIndex >= info->frameCount || !info->frames) {
        return {0, 0, 0, 0}; // Invalid frame
    }

    return info->frames[frameIndex];
}

// Opponent car sprite color palette index
// Opponent car sprites use indexed colors. Palette index 7 contains the magenta
// placeholder color (#f000c0 / RGB: 240, 0, 192) that represents the car body.
// To recolor an opponent car, replace palette[7] with the desired car color before
// converting the sprite from planar to chunky format.
constexpr uint8_t SR_OPPONENT_CAR_PALETTE_INDEX = 7;
constexpr uint8_t SR_OPPONENT_MAGENTA_R = 240;
constexpr uint8_t SR_OPPONENT_MAGENTA_G = 0;
constexpr uint8_t SR_OPPONENT_MAGENTA_B = 192;
constexpr uint8_t SR_OPPONENT_MAGENTA_TOLERANCE = 5;  // RGB tolerance for matching

// ============================================================================
// AUDIO SAMPLE DATA - Verified Sound Sample Offsets and Properties
// ============================================================================
//
// Street Rod uses three main audio files containing raw 8-bit signed PCM samples:
// - srs0.wave: Racing engine sounds
// - srl0.wave: General game sounds (engine start, effects)
// - srl1.wave: Music samples (California Dreams custom format)
//
// All samples are 8-bit SIGNED audio (-128 to 127), NOT unsigned!
// Amiga Paula chip base frequency: 3579545 Hz (NTSC) / 3546895 Hz (PAL)
// Sample playback rate = PAULA_BASE_FREQ / period
//
// ============================================================================

// srs0.wave - Racing Engine Sound
// Empirically this matches original game timbre best.
constexpr uint32_t SRS0_ENGINE_LOOP_OFFSET = 0x0000;  // Racing engine loop
constexpr uint32_t SRS0_ENGINE_LOOP_LENGTH = 0x0200;  // Bytes (looped)
constexpr uint16_t SRS0_ENGINE_IDLE_PERIOD = 252;     // Period at idle RPM (~14204 Hz)
constexpr uint16_t SRS0_ENGINE_MAX_PERIOD = 193;      // Period at max RPM (~18548 Hz)
constexpr uint32_t SRS0_ENGINE_BASE_RATE = 16000;     // Base sample rate (Hz), pitch-modulated by RPM

// srl0.wave - General Game Sounds
// Total file size: ~12KB+
constexpr uint32_t SRL0_ENGINE_START_OFFSET = 0x0000;  // Offset in bytes
constexpr uint16_t SRL0_ENGINE_START_PERIOD = 325;    // Amiga period (3579545 / 325 ≈ 11014 Hz)
constexpr uint32_t SRL0_ENGINE_START_LENGTH = 7827;  // Length in bytes
constexpr uint32_t SRL0_ENGINE_START_RATE = 11025;  // Sample rate in Hz
// Duration: 0.710 seconds

constexpr uint32_t SRL0_TIRE_SCREECH_OFFSET = 0x1E93;  // Offset in bytes
constexpr uint32_t SRL0_TIRE_SCREECH_LENGTH = 4910;  // Length in bytes
constexpr uint32_t SRL0_TIRE_SCREECH_RATE = 11025;  // Sample rate in Hz
// Duration: 0.445 seconds

constexpr uint32_t SRL0_HONK_OFFSET = 0x31C1;  // Offset in bytes
constexpr uint32_t SRL0_HONK_LENGTH = 1718;  // Length in bytes
constexpr uint32_t SRL0_HONK_RATE = 11025;  // Sample rate in Hz
// Duration: 0.156 seconds

constexpr uint32_t SRL0_CAR_BASH_OFFSET = 0x3877;  // Offset in bytes
constexpr uint32_t SRL0_CAR_BASH_LENGTH = 4773;  // Length in bytes
constexpr uint32_t SRL0_CAR_BASH_RATE = 11025;  // Sample rate in Hz
// Duration: 0.433 seconds

constexpr uint32_t SRL0_DEBRIS_OFFSET = 0x4B1C;  // Offset in bytes
constexpr uint32_t SRL0_DEBRIS_LENGTH = 4782;  // Length in bytes
constexpr uint32_t SRL0_DEBRIS_RATE = 11025;  // Sample rate in Hz
// Duration: 0.434 seconds

constexpr uint32_t SRL0_CAR_JACK_OFFSET = 0x5DCA;  // Offset in bytes
constexpr uint32_t SRL0_CAR_JACK_LENGTH = 3972;  // Length in bytes
constexpr uint32_t SRL0_CAR_JACK_RATE = 11025;  // Sample rate in Hz
// Duration: 0.360 seconds

constexpr uint32_t SRL0_COP_SIREN_OFFSET = 0x6D4E;  // Offset in bytes
constexpr uint32_t SRL0_COP_SIREN_LENGTH = 9766;  // Length in bytes
constexpr uint32_t SRL0_COP_SIREN_RATE = 11025;  // Sample rate in Hz
// Duration: 0.886 seconds


// Note: Other sound effects in srl0.wave (ignition, tire screech, honk, crash, siren)
// are present but exact offsets need verification from Amiga disassembly

// srl1.wave - Music Samples (California Dreams Format)
// Total file size: ~27KB
// These samples are used by the music sequencer for background music
constexpr uint32_t SRL1_TONE_OFFSET = 0x0000;         // Lead melody tone
constexpr uint32_t SRL1_TONE_LENGTH = 2355;           // Bytes
constexpr uint32_t SRL1_BASSDRUM_OFFSET = 0x0933;     // Bass drum percussion
constexpr uint32_t SRL1_BASSDRUM_LENGTH = 21;         // Bytes
constexpr uint32_t SRL1_HIHAT_OFFSET = 0x0948;        // Hi-hat percussion
constexpr uint32_t SRL1_HIHAT_LENGTH = 7;             // Bytes
constexpr uint32_t SRL1_BUZZ_OFFSET = 0x094F;         // Buzz/distortion effect
constexpr uint32_t SRL1_BUZZ_LENGTH = 24817;          // Bytes (rest of file)
constexpr uint32_t SRL1_MUSIC_SAMPLE_RATE = 22050;    // Standard sample rate for music

// Music format companion files (see streetrod-research/src/music for details):
// - msr.note: Note sequences and pattern data
// - srl.ins: Instrument definitions (maps notes to samples)
// - srl.env: Envelope data (volume/pitch modulation)

// Audio format notes:
// 1. All samples are 8-bit SIGNED PCM (-128 to 127)
// 2. Engine sounds use Amiga period-based pitch shifting
// 3. Music samples use fixed sample rates
// 4. Looped samples (engine) repeat seamlessly
// 5. One-shot samples (effects) play once without looping

// Helper function to calculate sample duration in seconds
constexpr inline float CalculateSampleDuration(uint32_t lengthInSamples, uint32_t sampleRate) {
    return static_cast<float>(lengthInSamples) / static_cast<float>(sampleRate);
}

// ============================================================================

// Track data offsets in Street Rod binary
constexpr uint32_t SR_TRACK_ROAD_RACE_OFFSET = 0xE000;      // Road race track (500 segments)
constexpr uint32_t SR_TRACK_DRAG_RACE_OFFSET = 0xDD00;      // Drag race track (~200 segments)
constexpr uint32_t SR_ROADSIDE_SPRITES_OFFSET = 0x10021;    // Roadside sprite placements
constexpr uint32_t SR_ROAD_POSTS_OFFSET = 0x10EC1;          // Road posts area
constexpr uint16_t SR_COORDINATE_SCALE_FACTOR = 16;         // Divide coordinates by this for screen coords

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

const int CAR_VENDOR_LOOKUP[][2] = {
    {CHEVROLET_2DR_COUPE_1940,             CarVendorGM},
    {CHEVROLET_ROADSTER_1940,              CarVendorGM},
    {CHEVROLET_MASTER_DELUXE_2DR_1938,     CarVendorGM},
    {CHEVROLET_CORVETTE_1956,              CarVendorGM},
    {CHEVROLET_CORVETTE_1961,              CarVendorGM},
    {CHEVROLET_CORVETTE_1963,              CarVendorGM},
    {DODGE_CUSTOM_ROYAL_LANCER_1955,       CarVendorChrysler},
    {FORD_DEUCE_COUPE_1932,                CarVendorFord},
    {DODGE_POLARA_1962,                    CarVendorChrysler},
    {PLYMOUTH_SAVOY_2DR_1962,              CarVendorChrysler},
    {FORD_SEDAN_1932,                      CarVendorFord},
    {FORD_DELUXE_2DR_1940,                 CarVendorFord},
    {FORD_FAIRLANE_VICTORIA_1955,          CarVendorFord},
    {FORD_FAIRLANE_500_1956,               CarVendorFord},
    {MERCURY_MONTEREY_1954,                CarVendorFord},
    {MERCURY_CUSTOM_2DR_1956,              CarVendorFord},
    {CHEVROLET_STYLELINE_2DR_1949,         CarVendorGM},
    {CHEVROLET_IMPALA_1958,                CarVendorGM},
    {OLDSMOBILE_88_2DR_CUSTOM_1949,        CarVendorGM},
    {OLDSMOBILE_88_CUSTOM_1952,            CarVendorGM},
    {PONTIAC_SILVER_STREAK_1950,           CarVendorGM},
    {FORD_THUNDERBIRD_1957,                CarVendorFord},
    {VALIANT_V200_1961,                    CarVendorChrysler},
    {VALIANT_V100_2DR_1963,                CarVendorChrysler},
    {CHEVROLET_BELAIR_SPORTS_COUPE,        CarVendorGM},
    {THE_KING_CHEVROLET_CORVETTE_1963,     CarVendorGM}
};


// Car X position lookup table by scene
const int CAR_IDLE_X[][2] = {
    {SCENE_GARAGE,      35},
    {SCENE_DINER,       82},
    {SCENE_GAS_STATION, 40},
    {SCENE_CRUISING,    0},  // Not used in idle position
    {SCENE_DRIVING,     0}   // Not used in idle position
};

#define CAR_IDLE_X_COUNT (sizeof(CAR_IDLE_X) / sizeof(CAR_IDLE_X[0]))

// Car Y offset lookup table by scene and car model
const int CAR_Y_OFFSETS[][6] = {
    // Format: {CarModel, Garage, Diner, GasStation, Cruising, Driving}
    {CHEVROLET_2DR_COUPE_1940,             82, 96, 112, 0, 0},
    {CHEVROLET_ROADSTER_1940,              82, 96, 112, 0, 0},
    {CHEVROLET_MASTER_DELUXE_2DR_1938,     82, 96, 112, 0, 0},
    {CHEVROLET_CORVETTE_1956,              100, 106, 102, 0, 0},
    {CHEVROLET_CORVETTE_1961,              102, 108, 104, 0, 0},
    {CHEVROLET_CORVETTE_1963,              92, 98, 94, 0, 0},
    {DODGE_CUSTOM_ROYAL_LANCER_1955,       90, 96, 92, 0, 0},
    {FORD_DEUCE_COUPE_1932,                86, 92, 88, 0, 0},
    {DODGE_POLARA_1962,                    94, 100, 96, 0, 0},
    {PLYMOUTH_SAVOY_2DR_1962,              92, 98, 94, 0, 0},
    {FORD_SEDAN_1932,                      84, 90, 86, 0, 0},
    {FORD_DELUXE_2DR_1940,                 90, 96, 92, 0, 0},
    {FORD_FAIRLANE_VICTORIA_1955,          96, 102, 98, 0, 0},
    {FORD_FAIRLANE_500_1956,               94, 100, 96, 0, 0},
    {MERCURY_MONTEREY_1954,                96, 102, 98, 0, 0},
    {MERCURY_CUSTOM_2DR_1956,              92, 98, 94, 0, 0},
    {CHEVROLET_STYLELINE_2DR_1949,         90, 96, 92, 0, 0},
    {CHEVROLET_IMPALA_1958,                94, 100, 96, 0, 0},
    {OLDSMOBILE_88_2DR_CUSTOM_1949,        92, 98, 94, 0, 10},
    {OLDSMOBILE_88_CUSTOM_1952,            90, 96, 92, 0, 0},
    {PONTIAC_SILVER_STREAK_1950,           90, 96, 92, 0, 0},
    {FORD_THUNDERBIRD_1957,                96, 102, 98, 0, 0},
    {VALIANT_V200_1961,                    94, 100, 96, 0, 0},
    {VALIANT_V100_2DR_1963,                92, 98, 94, 0, 0},
    {CHEVROLET_BELAIR_SPORTS_COUPE,        94, 100, 96, 0, 0},
    {THE_KING_CHEVROLET_CORVETTE_1963,     92, 98, 94, 0, 0}
};

#define CAR_Y_OFFSETS_COUNT (sizeof(CAR_Y_OFFSETS) / sizeof(CAR_Y_OFFSETS[0]))

const int CAR_WHEELS_Y_OFFSETS[][2] = {
    {CHEVROLET_2DR_COUPE_1940, 36},
    {CHEVROLET_ROADSTER_1940, 30},
    {CHEVROLET_MASTER_DELUXE_2DR_1938, 40},
    {CHEVROLET_CORVETTE_1956, 24},
    {CHEVROLET_CORVETTE_1961, 30},
    {CHEVROLET_CORVETTE_1963, 30},
    {DODGE_CUSTOM_ROYAL_LANCER_1955, 30},
    {FORD_DEUCE_COUPE_1932, 30},
    {DODGE_POLARA_1962, 30},
    {PLYMOUTH_SAVOY_2DR_1962, 30},
    {FORD_SEDAN_1932, 30},
    {FORD_DELUXE_2DR_1940, 39},
    {FORD_FAIRLANE_VICTORIA_1955, 30},
    {FORD_FAIRLANE_500_1956, 30},
    {MERCURY_MONTEREY_1954, 30},
    {MERCURY_CUSTOM_2DR_1956, 30},
    {CHEVROLET_STYLELINE_2DR_1949, 36},
    {CHEVROLET_IMPALA_1958, 30},
    {OLDSMOBILE_88_2DR_CUSTOM_1949, 30},
    {OLDSMOBILE_88_CUSTOM_1952, 30},
    {PONTIAC_SILVER_STREAK_1950, 30},
    {FORD_THUNDERBIRD_1957, 30},
    {VALIANT_V200_1961, 30},
    {VALIANT_V100_2DR_1963, 30},
    {CHEVROLET_BELAIR_SPORTS_COUPE, 30},
    {THE_KING_CHEVROLET_CORVETTE_1963, 30}
};

#define CAR_WHEELS_Y_OFFSETS_COUNT (sizeof(CAR_WHEELS_Y_OFFSETS) / sizeof(CAR_WHEELS_Y_OFFSETS[0]))

const int CAR_GAS_CAP_XY_OFFSETS[][3] = {
    {CHEVROLET_2DR_COUPE_1940, 74, 35},
    {CHEVROLET_ROADSTER_1940, 44, 7},
    {CHEVROLET_MASTER_DELUXE_2DR_1938, 37, 7},
    {CHEVROLET_CORVETTE_1956, 24, 7},
    {CHEVROLET_CORVETTE_1961, 30, 7},
    {CHEVROLET_CORVETTE_1963, 30, 7},
    {DODGE_CUSTOM_ROYAL_LANCER_1955, 30, 7},
    {FORD_DEUCE_COUPE_1932, 30, 7},
    {DODGE_POLARA_1962, 30, 7},
    {PLYMOUTH_SAVOY_2DR_1962, 30, 7},
    {FORD_SEDAN_1932, 30, 7},
    {FORD_DELUXE_2DR_1940, 39, 7},
    {FORD_FAIRLANE_VICTORIA_1955, 30, 7},
    {FORD_FAIRLANE_500_1956, 30, 7},
    {MERCURY_MONTEREY_1954, 30, 7},
    {MERCURY_CUSTOM_2DR_1956, 30, 7},
    {CHEVROLET_STYLELINE_2DR_1949, 30, 7},
    {CHEVROLET_IMPALA_1958, 30, 7},
    {OLDSMOBILE_88_2DR_CUSTOM_1949, 30, 7},
    {OLDSMOBILE_88_CUSTOM_1952, 30, 7},
    {PONTIAC_SILVER_STREAK_1950, 30, 7},
    {FORD_THUNDERBIRD_1957, 30, 7},
    {VALIANT_V200_1961, 30, 7},
    {VALIANT_V100_2DR_1963, 30, 7},
    {CHEVROLET_BELAIR_SPORTS_COUPE, 30, 7},
    {THE_KING_CHEVROLET_CORVETTE_1963, 30, 7}
};

#define CAR_GAS_CAP_XY_OFFSETS_COUNT (sizeof(CAR_GAS_CAP_XY_OFFSETS) / sizeof(CAR_GAS_CAP_XY_OFFSETS[0]))

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

#pragma pack(push, 1)
struct DOSBitmapEntry_t {
    uint16_t width;
    uint16_t height;
    uint16_t sizeDecoded;       // (width/8) * height * depth
    uint8_t type;               // 0=opaque, 1=transparent (red bg), 2=mask
    uint8_t rleDictionary[16];
    uint8_t padding;            // always 0
    uint32_t offset;
    uint16_t sizeEncoded;
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
    // Supplemental offsets
    // not found in SR_data
    uint8_t offsetYTires;
    uint8_t offsetXGasCap;
    uint8_t offsetYGasCap;
};

struct BitmapReplacement_t {
    int index;
    BitmapEntry_t entry;
    std::vector<uint8_t> planarData;
};

struct ClassifiedEntry_t {
    int year;
    char* carName;
    char* adText;
    int price;
};

struct ClickBox_t {
    int x;
    int y;
    int width;
    int height;
};

struct CarClickBoxes_t {
    ClickBox_t clickBoxRearBumper;
    ClickBox_t clickBoxFrontBumper;
    ClickBox_t clickBoxChoppedRoof;
    ClickBox_t clickBoxTransmission;
    ClickBox_t clickBoxEngine;
    ClickBox_t clickBoxRearWindow;
    ClickBox_t clickBoxFrontWindow;
    ClickBox_t clickBoxGasCap;
};

struct CarMetadata_t {
    BitmapOffset_t offsets;
    ClassifiedEntry_t classified;
    CarClickBoxes_t clickBoxes;
};

struct PartMetadata_t {
    ClassifiedEntry_t classified;
};

enum StockPalette {
    StockPaletteAmigaDiner = 0,
    StockPaletteAmigaGasStation = 1,
    StockPaletteAmigaJukebox = 2,
    StockPaletteAmigaTitleScreen = 3,
    StockPaletteAmigaDriving = 4,
};




// FIXME: Add all stock palettes
#define RLE         {{1, 0x0}, {1, 0xff}, {2, 0x0}, {3, 0x0}, {2, 0xff}, {4, 0x0}, {5, 0x0}, {3, 0xff},	{6, 0x0}, {4, 0xff}, {7, 0x0}, {5, 0xff}, {8, 0x0}, {6, 0xff}, {9, 0x0}, {7, 0xff}}
#define RLE_DOS     {{1, 0x00}, {1, 0xFF}, {2, 0x00}, {3, 0x00}, {4, 0x00}, {2, 0xFF}, {3, 0xFF}, {5, 0x00}, {6, 0xFF}, {6, 0x00}, {4, 0xFF}, {7, 0xFF}, {7, 0x00}, {5, 0xFF}, {8, 0xFF}, {8, 0x00}}
#define PAL1        {0x0000, 0x0810, 0x0911, 0x0B21, 0x0D22, 0x0F32, 0x0F44, 0x0F65, 0x0F87, 0x0F98, 0x0FFF, 0x0EEE, 0x0CCC, 0x0BBB, 0x0AAA, 0x0999, 0x0888, 0x0950, 0x0C71, 0x0F96, 0x0333, 0x0222, 0x005B, 0x007F, 0x00F0, 0x00D0, 0x0FE9, 0x0FD2, 0x0B21, 0x0F32, 0x009F, 0x00BF}
#define PAL_GAS     {0x0000, 0x0810, 0x0911, 0x0B21, 0x0D22, 0x0F32, 0x0F44, 0x0F65, 0x0F87, 0x0F98, 0x0FFF, 0x0EEE, 0x0CCC, 0x0BBB, 0x0AAA, 0x0999, 0x0888, 0x0950, 0x0C71, 0x0F96, 0x0333, 0x0222, 0x005B, 0x007F, 0x00F0, 0x00B0, 0x0FE9, 0x0FD2, 0x0B21, 0x0F32, 0x0F96, 0x0C63}
#define PAL_DRIVING {0x0000, 0x0640, 0x0999, 0x0666, 0x0333, 0x0111, 0x0F0C, 0x0B0A, 0x0F0C, 0x0F0C, 0x0EEE, 0x052F, 0x0FE0, 0x00B1, 0x0070, 0x0E00, 0x0CCC, 0x0330, 0x0970, 0x0860, 0x0777, 0x0666, 0x0555, 0x0444, 0x0A94, 0x0CB6, 0x0BBB, 0x0999, 0x0888, 0x0B00, 0x0900, 0x0AAA}
// Same as PAL_DRIVING, but some colors are swapped using copperlists
#define PAL_DRIVING_LOW {0x0000, 0x0640, 0x0999, 0x0666, 0x0333, 0x0111, 0x0F96, 0x0D94, 0x0D62, 0x0A50, 0x0EEE, 0x0841, 0x0730, 0x00B1, 0x0070, 0x0E00, 0x0CCC, 0x0330, 0x0970, 0x0860, 0x0777, 0x0666, 0x0555, 0x0444, 0x0A94, 0x0CB6, 0x0BBB, 0x0999, 0x0888, 0x0B00, 0x0900, 0x0AAA};
static const int kCarIndex[] = {
    CHEVROLET_2DR_COUPE_1940,
    CHEVROLET_ROADSTER_1940,
    CHEVROLET_MASTER_DELUXE_2DR_1938,
    CHEVROLET_CORVETTE_1956,
    CHEVROLET_CORVETTE_1961,
    CHEVROLET_CORVETTE_1963,
    DODGE_CUSTOM_ROYAL_LANCER_1955,
    FORD_DEUCE_COUPE_1932,
    DODGE_POLARA_1962,
    PLYMOUTH_SAVOY_2DR_1962,
    FORD_SEDAN_1932,
    FORD_DELUXE_2DR_1940,
    FORD_FAIRLANE_VICTORIA_1955,
    FORD_FAIRLANE_500_1956,
    MERCURY_MONTEREY_1954,
    MERCURY_CUSTOM_2DR_1956,
    CHEVROLET_STYLELINE_2DR_1949,
    CHEVROLET_IMPALA_1958,
    OLDSMOBILE_88_2DR_CUSTOM_1949,
    OLDSMOBILE_88_CUSTOM_1952,
    PONTIAC_SILVER_STREAK_1950,
    FORD_THUNDERBIRD_1957,
    VALIANT_V200_1961,
    VALIANT_V100_2DR_1963,
    CHEVROLET_BELAIR_SPORTS_COUPE,
}; // King: THE_KING_CHEVROLET_CORVETTE_1963
#define CAR_INDEX_COUNT CAR_PLAYER_MODEL_COUNT
#define CAR_PLAYER_MODEL_COUNT 25
#define DRIVER_COUNT 22

class VectorReader {
private:
    const std::vector<uint8_t>& data;
    size_t position;

public:
    VectorReader(const std::vector<uint8_t>& data) : data(data), position(0) {}

    uint8_t ReadUint8() {
        if (position >= data.size()) throw std::runtime_error("Read beyond vector bounds");
        return data[position++];
    }

    uint16_t ReadUint16BE() {
        if (position + 1 >= data.size()) throw std::runtime_error("Read beyond vector bounds");
        uint16_t value = (static_cast<uint16_t>(data[position]) << 8) | data[position + 1];
        position += 2;
        return value;
    }

    uint32_t ReadUint32BE() {
        if (position + 3 >= data.size()) throw std::runtime_error("Read beyond vector bounds");
        uint32_t value = (static_cast<uint32_t>(data[position]) << 24) |
                        (static_cast<uint32_t>(data[position + 1]) << 16) |
                        (static_cast<uint32_t>(data[position + 2]) << 8) |
                        data[position + 3];
        position += 4;
        return value;
    }

    uint16_t ReadUint16LE() {
        if (position + 1 >= data.size()) throw std::runtime_error("Read beyond vector bounds");
        uint16_t value = data[position] | (static_cast<uint16_t>(data[position + 1]) << 8);
        position += 2;
        return value;
    }

    uint32_t ReadUint32LE() {
        if (position + 3 >= data.size()) throw std::runtime_error("Read beyond vector bounds");
        uint32_t value = data[position] |
                        (static_cast<uint32_t>(data[position + 1]) << 8) |
                        (static_cast<uint32_t>(data[position + 2]) << 16) |
                        (static_cast<uint32_t>(data[position + 3]) << 24);
        position += 4;
        return value;
    }

    std::string ReadNullTerminatedString() {
        std::string result;
        while (position < data.size() && data[position] != 0) {
            result += static_cast<char>(data[position++]);
        }
        position++; // Skip the null terminator
        return result;
    }

    void Seek(size_t newPosition) {
        if (newPosition > data.size()) throw std::runtime_error("Seek beyond vector bounds");
        position = newPosition;
    }

    size_t Tell() const {
        return position;
    }

    std::vector<uint8_t> ReadBytes(size_t count) {
        if (position + count > data.size()) throw std::runtime_error("Read beyond vector bounds");
        std::vector<uint8_t> result(data.begin() + position, data.begin() + position + count);
        position += count;
        return result;
    }
};

/*
    Debug functions
*/

void SrLibDebugBitmapEntry(int number, BitmapEntry_t entry);
void SrLibDebugBitplanes(const uint8_t* data, int width, int height, int depth);


/*
    Bitmap functions
*/

class BitmapParser {

   public:

    std::pair<int, std::vector<BitmapEntry_t>>                ReadEntries(const std::vector<uint8_t>& data);
	std::pair<int, std::vector<BitmapEntry_t>>                ReadEntries(const std::string& filename);
    std::vector<SrLibColor>                                   PlanarToChunky(const std::vector<uint8_t>& data, BitmapEntry_t entry, const Palette& palette, const Palette& carPalette = {}, int assetIndex = -1);
    std::vector<uint8_t>                                 ChunkyToPlanar(const std::vector<SrLibColor>& pixels, BitmapEntry_t entry, const Palette& palette, std::string* error = nullptr);
    std::vector<uint8_t>                                 GetPackedData(const std::vector<uint8_t>& data, const BitmapEntry_t& bitmapEntry, int totalEntriesCount);
    std::vector<uint8_t>                                 GetPackedData(const std::string& filename, const BitmapEntry_t& bitmapEntry, int totalEntriesCount);
    std::vector<uint8_t>                                 PackData(const BitmapEntry_t& bitmapEntry, const std::vector<uint8_t>& planarData);
    std::vector<uint8_t>                                 BuildAmigaLibP(const std::vector<uint8_t>& originalData, const std::map<int, std::vector<uint8_t>>& replacementPlanarData);
    std::vector<uint8_t>                                 UnpackData(const std::vector<std::pair<int, uint8_t>> &rle, const BitmapEntry_t &sprite, const std::vector<uint8_t> &data);
    std::vector<uint8_t>                                 ReadImage(const std::vector<uint8_t>& data, BitmapEntry_t entry, int index, int totalEntriesCount);
    std::vector<uint8_t>                                 ReadImage(const std::string& filename, BitmapEntry_t entry, int index, int totalEntriesCount);
    std::vector<std::pair<BitmapEntry_t, std::vector<uint8_t>>>    ReadAllImages(const std::string& filename);

    Palette                                         PreparePalette(const std::vector<uint16_t>& palette, bool ehb);
    Palette                                         PrepareCarPalette(const std::vector<uint16_t>& palette);
    static SrLibColor                                    GetRGBFromRGB4(uint16_t rgb4);
    std::vector<uint16_t>                                GetPalette(SrLibFile location, int index);
    static std::vector<uint16_t>                         GetStockPalette(StockPalette palette);
    LibAssetType                                    GetAssetType(SrLibFile location, int index);
    bool                                            IsTransparent(SrLibFile location, int index);

    std::pair<int, int>                                  GetSpritesheetInfo(SrLibFile location, int index);
    void                                            SavePaletteToPNG(const Palette& palette, const std::string& filename);

    // DOS LIB bitmap support
    std::pair<int, std::vector<DOSBitmapEntry_t>>   ReadEntriesDOS(const std::vector<uint8_t>& data);
    std::vector<uint8_t>                            GetPackedDataDOS(const std::vector<uint8_t>& data, const DOSBitmapEntry_t& entry, int totalEntriesCount);
    std::vector<uint8_t>                            UnpackDataDOS(const DOSBitmapEntry_t& sprite, const std::vector<uint8_t>& data);
    std::vector<uint8_t>                            ReadImageDOS(const std::vector<uint8_t>& data, const DOSBitmapEntry_t& entry, int index, int totalEntriesCount);
    Palette                                         GetDOSPalette(int spriteType = 0);
    std::vector<SrLibColor>                         PlanarToChunky(const std::vector<uint8_t>& data, DOSBitmapEntry_t entry, const Palette& palette);
};

/*
    Amiga sprite functions
*/

enum SrLibAmigaSpriteAsset {
    SrLibAmigaSpriteAssetMousePointerWrench = 0,
    SrLibAmigaSpriteAssetMousePointerHand = 1,
    SrLibAmigaSpriteAssetMousePointerPump = 2,
    SrLibAmigaSpriteAssetMousePointerRag = 3,
    SrLibAmigaSpriteAssetMousePointerThumbsUp = 4,
    SrLibAmigaSpriteAssetMousePointerThumbsDown = 5,
    SrLibAmigaSpriteAssetCount = 6,
};

struct SrLibAmigaSpriteInfo {
    int offset;
    int width;
    int height;
};

const SrLibAmigaSpriteInfo SPRITE_INFO_LOOKUP[] = {
    // {offset, width, height}
    {0x7AA8, 16, 16},  // SrLibAmigaSpriteAssetMousePointerWrench
    {0x7AF0, 16, 12},  // SrLibAmigaSpriteAssetMousePointerHand
    {0x7B28, 11, 16},  // SrLibAmigaSpriteAssetMousePointerPump
    {0x7B70, 16, 14},  // SrLibAmigaSpriteAssetMousePointerRag
    {0x7BB0, 16, 17},  // SrLibAmigaSpriteAssetMousePointerThumbsUp
    {0x7BFC, 16, 17},  // SrLibAmigaSpriteAssetMousePointerThumbsDown
};

#define SPRITE_INFO_LOOKUP_COUNT (sizeof(SPRITE_INFO_LOOKUP) / sizeof(SPRITE_INFO_LOOKUP[0]))

int SrLibAmigaSpriteToRGBA(const uint16_t* data, int startOffset, const uint16_t* palette, int sprite_num, int width, int height, SrLibColor* out_buffer);
int SrLibAmigaSpriteReadMousePointer(const uint16_t* data, SrLibAmigaSpriteAsset sprite, SrLibColor* out_buffer);

/*
    Data functions
*/

void SrLibDataCleanupCarString(char* str);
void SrLibDataFreeClassified(ClassifiedEntry_t* entry);

// Transmission types enum
enum SrTransmissionType {
    SR_TRANS_AUTO_GM = 0,
    SR_TRANS_AUTO_FORD = 1,
    SR_TRANS_AUTO_CHRYSLER = 2,
    SR_TRANS_3SPD_GM = 3,
    SR_TRANS_3SPD_FORD = 4,
    SR_TRANS_3SPD_CHRYSLER = 5,
    SR_TRANS_4SPD_GM = 6,
    SR_TRANS_4SPD_FORD = 7,
    SR_TRANS_4SPD_CHRYSLER = 8,
    SR_TRANS_4SPD_UNIVERSAL = 9,
    SR_TRANS_RACING_GM = 10,
    SR_TRANS_RACING_FORD = 11,
    SR_TRANS_RACING_CHRYSLER = 12,
    SR_TRANS_COUNT = 13
};

// SR_data challenger dialog block: 22 entries × 10 bytes = 220 bytes.
// Byte layout confirmed identical between DOS HOT_DATA and Amiga SR_data runtime.
static constexpr int SR_DATA_DRIVER_ENTRY_COUNT  = 22;
static constexpr int SR_DATA_CARSPEC_ENTRY_COUNT = 26; // 25 regular + King at index 25
static constexpr int SR_DATA_KING_DRIVER_INDEX   = 21; // entry 21 = The King
static constexpr int SR_DATA_KING_CARSPEC_INDEX  = 25; // spec 25 = The King's car

#pragma pack(push, 1)
struct SrDriverData {
    uint8_t tier;       // [0] difficulty tier: 0=easy, 1=medium, 2=hard
    uint8_t reaction;   // [1] reaction time 0x0A (fast) – 0x14 (slow)
    uint8_t aggression; // [2] aggression 0x04 (low) – 0x0C (high)
    uint8_t pad0;       // [3] always 0x00
    char    name[6];    // [4..9] driver name, null-terminated within 6 bytes
};
static_assert(sizeof(SrDriverData) == 10, "SrDriverData must be 10 bytes");

// SR_data car spec block: 26 entries × 14 bytes = 364 bytes.
// TODO: verify exact field layout against Amiga FUN_002435f0 (ComputeCarPerformanceScore inner).
struct SrCarSpecEntry {
    uint8_t street_cred;    // [0] car class 0-3 (street credibility / tier grouping)
    uint8_t engine;         // [1] engine index (CarEngine_t, 0-8)
    uint8_t transmission;   // [2] transmission index (SrTransmissionType, 0-12)
    uint8_t carburetor;     // [3] carburetor index (0=2bbl, 1=4bbl, 2=racing)
    uint8_t tires;          // [4] tire index (0=stock, 1=brand, 2=racing)
    uint8_t condition;      // [5] body condition 0-3
    uint8_t upgrade_flags;  // [6] bits 0-1 = engine/body upgrade availability
    uint8_t pad[7];         // [7..13] unused
};
static_assert(sizeof(SrCarSpecEntry) == 14, "SrCarSpecEntry must be 14 bytes");
#pragma pack(pop)

// Transmission data structure
struct SrTransmissionData {
    char name[32];           // Transmission name (e.g., "Trnsmsn 4-spd. GM.")
    uint16_t price;          // Price in dollars
    uint8_t num_gears;       // Number of gears (1=auto, 3, or 4)
    uint8_t brand;           // 0=universal, 1=GM, 2=Ford, 4=Chrysler
    int16_t gear_ratios[4];  // Gear ratios as scaled integers
    uint16_t scale_factor;   // Scaling factor (100, 256, etc.)

    // Helper: Get actual ratio as float (for display/debugging)
    float GetRatio(int gear) const {
        if (gear < 0 || gear >= 4) return 0.0f;
        return static_cast<float>(gear_ratios[gear]) / static_cast<float>(scale_factor);
    }
};

class DataParser {
    public:
        CarMetadata_t*         SrLibDataReadDataFromFile(const char* filename, int* outCount);
        CarMetadata_t*         SrLibDataReadDataFromBuffer(const uint8_t* data, size_t dataSize, int* outCount);
        PartMetadata_t*        SrLibDataReadPartsFromBuffer(const uint8_t* data, size_t dataSize, int* outCount);
        std::vector<uint8_t>   SrLibDataWriteCarMetadataToBuffer(const std::vector<uint8_t>& data, const std::vector<CarMetadata_t>& metadata, std::string* error = nullptr);
        int                    GetCarIndex(AmigaLibAsset asset);

        // New: Transmission data parsing
        std::vector<SrTransmissionData> ParseTransmissions(const std::vector<uint8_t>& srDataFile);
        std::vector<SrTransmissionData> ParseTransmissions(const std::string& filename);

        // SR_data driver and car spec parsing.
        // sr_data must be the raw SR_data file bytes (driver block first, then car spec block).
        std::vector<SrDriverData>   ParseDriverData(const std::vector<uint8_t>& sr_data);
        std::vector<SrCarSpecEntry> ParseCarSpecTable(const std::vector<uint8_t>& sr_data);
};


/*
    Query functions
*/

class CarQuery {
    public:
        static bool SrLibQueryCanChopRoof(AmigaLibAsset carAsset);
        static bool SrLibQueryCanStripBumpers(AmigaLibAsset carAsset);
        static int SrLibQueryGetChoppedBumberRearIndex(AmigaLibAsset carAsset);
        static int SrLibQueryGetChoppedBumberFrontIndex(AmigaLibAsset carAsset);
};

// Track data reader class for loading Street Rod track data
class TrackReader {
private:
    std::vector<uint8_t> binaryData;
    bool dataLoaded = false;

    // Helper to read big-endian 16-bit value
    int16_t ReadBigEndian16(const uint8_t* data, size_t offset) const {
        return (static_cast<int16_t>(data[offset]) << 8) | data[offset + 1];
    }

    // Helper to read big-endian 16-bit unsigned value
    uint16_t ReadBigEndianU16(const uint8_t* data, size_t offset) const {
        return (static_cast<uint16_t>(data[offset]) << 8) | data[offset + 1];
    }

public:
    // Load Street Rod binary data from file
    bool LoadTrackData(const std::string& binaryPath);

    // Load Street Rod binary data from memory (AssetManager)
    bool LoadTrackData(const std::vector<uint8_t>& binaryData);

    // Extract raw coordinate segments from track
    std::vector<SrTrackCoordSegment> ExtractTrackCoordinates(SrTrackType trackType, int maxSegments = 500);

    // Extract roadside sprite placements
    std::vector<SrRoadSideSpriteRecord> ExtractRoadSideSprites(uint32_t offset = SR_ROADSIDE_SPRITES_OFFSET, int maxRecords = 200);

    // Convert coordinate segment to screen coordinates
    static void ScaleCoordinates(SrTrackCoordSegment& segment) {
        segment.xStart /= SR_COORDINATE_SCALE_FACTOR;
        segment.xEnd /= SR_COORDINATE_SCALE_FACTOR;
        segment.yStart /= SR_COORDINATE_SCALE_FACTOR;
        segment.yEnd /= SR_COORDINATE_SCALE_FACTOR;
    }

    // Convert sprite position to screen coordinates
    static void ScaleSpritePosition(SrRoadSideSpriteRecord& record) {
        record.trackPosition1 /= SR_COORDINATE_SCALE_FACTOR;
        record.trackPosition2 /= SR_COORDINATE_SCALE_FACTOR;
    }

    // Get sprite name for debugging
    static const char* GetSpriteName(uint16_t spriteId);

    // Analyze track for statistics
    struct TrackAnalysis {
        int segmentCount;
        int curvedSegments;
        int straightSegments;
        float totalLength;
        int minX, maxX, minY, maxY;
        std::vector<uint16_t> uniqueSprites;
    };

    TrackAnalysis AnalyzeTrack(const std::vector<SrTrackCoordSegment>& segments);
};

int SrLibQueryGetCarRoofYOffset(SrLibGameScene scene, int carIndex);
int SrLibQueryGetCarIdleX(SrLibGameScene scene);
CarVendor_t SrLibQueryGetCarVendor(int carIndex);

enum SrLibStatus {
    SrLibStatusSuccess = 0,
    SrLibStatusChecksumMismatch = 1,
    SrLibStatusFileNotFound = 2,
};

uint32_t    SrLibCalculateChecksum(const char *filename);
SrLibStatus SrLibVerifyChecksum(const char *filename, uint32_t checksum);






#endif
