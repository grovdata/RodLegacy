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

	// Timing management
	Uint64 lastUpdateTime = 0;		// Last update time in milliseconds
	bool firstUpdate = true;		// Flag to handle first update
	
	// Layer visibility toggles
	bool layerRoadVisible = true;		// Layer 0: Road + roadside objects + arches
	bool layerMirrorVisible = true;		// Layer 1: Rearview mirror
	bool layerDashVisible = true;		// Layer 2: Car dash/HUD

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
	
	// Layer rendering methods
	void DrawLayer0_Road(float deltaTime, int nTrackSection);
	void DrawLayer1_Mirror();
	void DrawLayer2_Dash(float distanceInMiles, int speedMph, int nTrackSection);
	
	// Input handling
	void HandleLayerToggleInput(SDL_Event* event);

public:

    Racing();
    ~Racing();
	void Draw(float fElapsedTime, SDL_Event *event);
    SDL_Surface* GetSurface();
    
    // Street Rod track loading
    void LoadTrackData(const std::vector<uint8_t>& binaryData, SrTrackType trackType = SR_TRACK_ROAD_RACE);
    void SetTrackType(SrTrackType trackType);
    
    // Get current car speed for animations
    float GetSpeed() const { return fSpeed; }
    
    // Layer visibility controls
    void ToggleLayer0() { layerRoadVisible = !layerRoadVisible; printf("Layer 0 (Road): %s\n", layerRoadVisible ? "VISIBLE" : "HIDDEN"); }
    void ToggleLayer1() { layerMirrorVisible = !layerMirrorVisible; printf("Layer 1 (Mirror): %s\n", layerMirrorVisible ? "VISIBLE" : "HIDDEN"); }
    void ToggleLayer2() { layerDashVisible = !layerDashVisible; printf("Layer 2 (Dash/HUD): %s\n", layerDashVisible ? "VISIBLE" : "HIDDEN"); }
    
    // Layer visibility getters
    bool IsLayer2Visible() const { return layerDashVisible; }
};
