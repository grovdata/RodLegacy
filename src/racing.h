#pragma once  // Add header guard

/*
https://raw.githubusercontent.com/OneLoneCoder/Javidx9/master/ConsoleGameEngine/SmallerProjects/OneLoneCoder_RetroArcadeRacer.cpp
*/

#include <iostream>
#include <string>
#include <list>
#include <set>
#include <memory>
#include <SDL2/SDL.h>
#include <vector>
#include "scene.h"
#include "parsers/libp.h"
#include "bitmap.h"
#include "ui/hud.h"

class Racing {

private:

	SDL_Color black = { 0, 0, 0, 255 };
	SDL_Color yellow = { 255, 238, 0, 255 };
	SDL_Color white = { 255, 255, 255, 255 };
	SDL_Color red = { 170, 0, 0, 255 };
	SDL_Color green = { 0, 119, 0, 255 };
	SDL_Color darkGreen = { 0, 119, 0, 255 };
	SDL_Color grey = { 200, 200, 200, 255 };

	HUD* hud;
	SDL_Surface* pixels;

	float fDistance = 0.0f;			// Distance car has travelled around track
	float fCurvature = 0.0f;		// Current track curvature, lerped between track sections
	float fTrackCurvature = 0.0f;	// Accumulation of track curvature
	float fTrackDistance = 0.0f;	// Total distance of track

	float fCarPos = 0.0f;			// Current car position (-1.0 to 1.0, 0 = center)
	float fPlayerCurvature = 0.0f;			// Accumulation of player curvature
	float fSpeed = 0.0f;			// Current player speed
	bool bCrashed = false;			// Whether the car has crashed
	float fCrashRecoveryTime = 0.0f;	// Time to recover from crash
	
	// Physics variables (based on Street Rod assembly analysis)
	int16_t velocity = 1500;		// Current velocity/RPM value (1500 = idle, 5700 = redline)
	int16_t intermediate = 0;		// velocity * gear_ratio (represents road speed, preserved across shifts)
	int16_t rpm = 1500;				// Current engine RPM (same as velocity - this IS the engine speed)
	int16_t currentGear = 0;		// Current gear (0 = 1st, 1 = 2nd, 2 = 3rd, 3 = 4th)
	int16_t maxGear = 3;			// Maximum gear (3 = 4-speed transmission)
	int16_t gearRatios[4] = {280, 190, 135, 100};  // Gear ratios scaled by 100 (2.80, 1.90, 1.35, 1.00)
	int16_t transmissionDamage = 0;	// Over-rev damage accumulator
	bool isAutomatic = false;		// Transmission type
	float acceleration = 15.0f;		// Acceleration rate (added to intermediate per frame @ 60fps)
	float deceleration = 10.0f;		// Deceleration rate (subtracted from velocity per frame @ 60fps)
	
	// Input state
	bool shiftUpPressed = false;	// Track shift up button state
	bool shiftDownPressed = false;	// Track shift down button state

	// Timing management
	Uint64 lastUpdateTime = 0;		// Last update time in milliseconds
	bool firstUpdate = true;		// Flag to handle first update
	float raceStartGracePeriod = 3.0f;	// Grace period at race start (no crashes)

	std::vector<std::pair<float, float>> vecTrack; // Track sections, sharpness of bend, length of section
	
	// Street Rod track data integration
	std::unique_ptr<TrackReader> trackReader;
	std::vector<SrTrackCoordSegment> currentTrackSegments;
	std::vector<SrRoadSideSpriteRecord> currentRoadsideSprites;
	SrTrackType currentTrackType = SR_TRACK_ROAD_RACE;
	
	// Sprite tracking for logging
	std::set<int> passedSprites;  // Track which sprites we've already passed
	float lastLoggedDistance = -1.0f;
	
	// Track progress logging
	float lastProgressLogDistance = -1.0f;
	int lastLoggedSegment = -1;
	
	// Sprite bitmaps for rendering
	std::unique_ptr<Bitmap> telephonePoleSprite;
	std::unique_ptr<Bitmap> treeSprite;
	std::unique_ptr<Bitmap> bushSprite;
	std::unique_ptr<Bitmap> roadPostSprite;
	std::unique_ptr<Bitmap> mountainSprite;
	std::unique_ptr<Bitmap> cloudSprite;
	std::unique_ptr<Bitmap> rearviewMirrorSprite;
	
	// Overhead sign sprites (rendered as pseudo-3D trapezoids spanning the road)
	std::unique_ptr<Bitmap> signCountryLineSprite;  // ID 14 - finish line
	std::unique_ptr<Bitmap> signRoute69Sprite;      // ID 33
	std::unique_ptr<Bitmap> signCityLimitsSprite;   // ID 34
	std::unique_ptr<Bitmap> signExit48Sprite;       // ID 35

	std::list<float> listLapTimes;		// List of previous lap times
	float fCurrentLapTime;			// Current lap time

	// Move to generic
	void PutPixel(SDL_Surface* s, int x, int y, SDL_Color c);
	
	// Street Rod track integration methods
	void LoadStreetRodTrack(const std::vector<uint8_t>& binaryData, SrTrackType trackType);
	void ConvertTrackCoordinatesToRacing();
	float CalculateSegmentCurvature(const SrTrackCoordSegment& segment1, const SrTrackCoordSegment& segment2);
	float CalculateSegmentLength(const SrTrackCoordSegment& segment);
	void CheckForSpriteEncounters(float currentDistance);
	void LogTrackProgress(float currentDistance, int currentSegment);
	void DrawRoadsideSprites(float currentDistance);
	void DrawSpriteIndicator(float distance, uint16_t spriteId, bool leftSide);
	void LoadSpriteBitmaps();
	void DrawRearviewMirror(int mirrorLevel);
	bool CheckCarCollisionWithSprite(float spriteDistance, bool leftSide);
	void HandleCrash();
	
	// Overhead sign rendering (pseudo-3D trapezoids)
	void DrawOverheadSigns(float currentDistance);
	void DrawOverheadSignTrapezoid(float distance, uint16_t signId);
	void DrawOverheadSignBitmaps(float currentDistance);
	
	// Physics methods
	void UpdatePhysics(float deltaTime, bool accelerating);
	void ShiftGear(int direction);
	void UpdateRPM();
	void UpdateTransmissionDamage();
	void AutoUpshift();
	void AutoDownshift();
	
	// Gauge needle helper
	void DrawGaugeNeedle(SDL_Renderer* renderer, int centerX, int centerY, 
	                     float value, float minValue, float maxValue);
	
public:

    Racing();
    ~Racing();
	void Draw(float fElapsedTime, SDL_Event *event);
    SDL_Surface* GetSurface();
    
    // Gauges (public - called by Driving scene to draw on top of dashboard)
    void DrawTachometerNeedleToRenderer(SDL_Renderer* renderer);
    void DrawSpeedometerNeedleToRenderer(SDL_Renderer* renderer);
    
    // Street Rod track loading
    void LoadTrackData(const std::vector<uint8_t>& binaryData, SrTrackType trackType = SR_TRACK_ROAD_RACE);
    void SetTrackType(SrTrackType trackType);
    
    // Car configuration
    void SetTransmissionType(bool automatic) { isAutomatic = automatic; }
    
    // Get current car speed for animations
    float GetSpeed() const { return fSpeed; }
    
    // Get physics state
    int16_t GetCurrentGear() const { return currentGear; }
    int16_t GetRPM() const { return rpm; }
    int16_t GetVelocity() const { return velocity; }
    bool IsTransmissionDamaged() const { return transmissionDamage > 0; }
    bool IsAutomatic() const { return isAutomatic; }
    
};
