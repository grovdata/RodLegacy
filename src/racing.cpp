/*
https://raw.githubusercontent.com/OneLoneCoder/Javidx9/master/ConsoleGameEngine/SmallerProjects/OneLoneCoder_RetroArcadeRacer.cpp
*/
#include "global.h"
#include "racing.h"
#include "state.h"
#include <cmath>
#include <memory>

Racing::Racing() {

        #if SDL_BYTEORDER == SDL_BIG_ENDIAN
            const Uint32 rmask = 0xff000000;
            const Uint32 gmask = 0x00ff0000;
            const Uint32 bmask = 0x0000ff00;
            const Uint32 amask = 0x000000ff;
        #else
            const Uint32 rmask = 0x000000ff;
            const Uint32 gmask = 0x0000ff00;
            const Uint32 bmask = 0x00ff0000;
            const Uint32 amask = 0xff000000;
        #endif
	pixels = SDL_CreateRGBSurface(0, GAME_WIDTH, GAME_RACING_HEIGHT, 32, rmask, gmask, bmask, amask);
	
	// Initialize HUD
	// Palette indices: 4 = 0x0333 (dark gray #303030), 24 = 0x0A94 (tan #a09040)
	hud = new HUD(StockPaletteAmigaDriving, 4, 24);
	hud->SetVisible(true);
	
	// Initialize track reader
		trackReader = std::make_unique<TrackReader>();
		
		// Load sprite bitmaps for roadside objects
		LoadSpriteBitmaps();
		
		// OLD: Hardcoded track definition (now replaced by Street Rod track data)
		// This will be populated by LoadTrackData() when called from Driving scene
		// Temporary fallback track for testing
		if (vecTrack.empty()) {
			vecTrack.push_back(std::make_pair(0.0f, 100.0f));		// Default straight section
			printf("Racing constructor: Using fallback track (Street Rod data not loaded yet)\n");
		} else {
			printf("Racing constructor: vecTrack already has %zu segments\n", vecTrack.size());
		}

		// Calculate total track distance, so we can set lap times
		fTrackDistance = 0.0f;
		for (auto t : vecTrack)
			fTrackDistance += t.second;

		listLapTimes = { 0,0,0,0,0 };
		fCurrentLapTime = 0.0f;


	}

Racing::~Racing() {
	delete hud;
}

void Racing::Draw(float fElapsedTime, SDL_Event *event) {
	// Calculate proper delta time
	Uint64 currentTime = SDL_GetTicks64();
	float deltaTime = 0.0f;
	
	if (firstUpdate) {
		deltaTime = 1.0f / 60.0f; // Default to 60 FPS on first update
		firstUpdate = false;
	} else {
		deltaTime = (currentTime - lastUpdateTime) / 1000.0f;
		// Cap delta time to prevent huge jumps
		if (deltaTime > 0.1f) deltaTime = 0.1f; // Max 100ms
		if (deltaTime < 0.0f) deltaTime = 1.0f / 60.0f; // Fallback to 60 FPS
	}
	lastUpdateTime = currentTime;

	// Handle control input
	int nCarDirection = 0;

	// Handle crash recovery
	if (bCrashed) {
		fCrashRecoveryTime -= deltaTime;
		if (fCrashRecoveryTime <= 0.0f) {
			bCrashed = false;
			velocity = 1500; // Reset to idle RPM
			fSpeed = 0.0f;
			fCarPos *= 0.5f; // Move car back toward center
		} else {
			// During crash, no control and car slows down (faster than normal deceleration)
			velocity -= static_cast<int16_t>(deceleration * 2.0f);
			if (velocity < 1500) velocity = 1500;
		}
	}

	// Car Curvature is accumulated left/right input, but inversely proportional to speed
	// i.e. it is harder to turn at high speed

	bool goLeft = false;
	bool goRight = false;
	bool accelerate = false;

	// Check current keyboard state for sustained input (like holding keys)
	const Uint8* keyboardState = SDL_GetKeyboardState(NULL);
	if (keyboardState && !bCrashed) {
		goLeft = keyboardState[SDL_SCANCODE_LEFT];
		goRight = keyboardState[SDL_SCANCODE_RIGHT];
		accelerate = keyboardState[SDL_SCANCODE_UP];
		
		// Handle gear shifting (original game controls: Space=up, Space+Down=down)
		bool spacePressed = keyboardState[SDL_SCANCODE_SPACE];
		bool downPressed = keyboardState[SDL_SCANCODE_DOWN];
		
		// Determine shift intention based on combination
		bool shiftUpNow = spacePressed && !downPressed;      // Space alone = shift up
		bool shiftDownNow = spacePressed && downPressed;     // Space + Down = shift down
		
		// Trigger shift on press (with debounce)
		if (shiftUpNow && !shiftUpPressed) {
			ShiftGear(1);
		}
		if (shiftDownNow && !shiftDownPressed) {
			ShiftGear(-1);
		}
		
		shiftUpPressed = shiftUpNow;
		shiftDownPressed = shiftDownNow;
	}

	// Update physics with new system
	UpdatePhysics(deltaTime, accelerate);

	// Update player curvature (steering input)
	if (!bCrashed) {
		if (goLeft) {
			fPlayerCurvature -= 0.7f * deltaTime * (1.0f - fSpeed / 2.0f);
			nCarDirection = -1;
		}
		else if (goRight) {
			fPlayerCurvature += 0.7f * deltaTime * (1.0f - fSpeed / 2.0f);
			nCarDirection = +1;
		}
		// No auto-centering - player must steer to stay on road
	}

	// Calculate car position relative to road center
	// In first-person view, difference between player and track curvature = off-road amount
	fCarPos = fPlayerCurvature - fTrackCurvature;

	// Decrease grace period timer
	if (raceStartGracePeriod > 0.0f) {
		raceStartGracePeriod -= deltaTime;
	}
	
	// Check if car is too far off road (crash condition)
	// When steering doesn't match the road curve, you go off-road
	// Add grace period at race start to prevent immediate crashes
	if (!bCrashed && fabs(fCarPos) > 1.2f && fSpeed > 0.3f && raceStartGracePeriod <= 0.0f) {
		SDL_Log("⚠️  Crash triggered! fCarPos=%.3f, fPlayerCurv=%.3f, fTrackCurv=%.3f, fSpeed=%.3f",
		        fCarPos, fPlayerCurvature, fTrackCurvature, fSpeed);
		HandleCrash();
	}

	// Clamp Speed
	if (fSpeed < 0.0f)	fSpeed = 0.0f;
	if (fSpeed > 1.0f)	fSpeed = 1.0f;

	// Move car along track according to car speed
	// Higher gears = faster track progress at same RPM
	// Calculate effective speed based on velocity and gear ratio
	int16_t currentRatio = gearRatios[currentGear];
	float gearMultiplier = 100.0f / currentRatio; // Higher gear (lower ratio) = faster
	fDistance += (105.0f * fSpeed * gearMultiplier) * deltaTime;
	
	// Check for sprite encounters
	CheckForSpriteEncounters(fDistance);

	// Get Point on track
	float fOffset = 0;
	int nTrackSection = 0;

	// Lap Timing and counting
	fCurrentLapTime += deltaTime;
	if (fDistance >= fTrackDistance)
	{
		fDistance -= fTrackDistance;
		listLapTimes.push_front(fCurrentLapTime);
		listLapTimes.pop_back();
		fCurrentLapTime = 0.0f;
	}

	// Find position on track (could optimise)
	while (nTrackSection < static_cast<int>(vecTrack.size()) && fOffset <= fDistance)
	{
		fOffset += vecTrack[nTrackSection].second;
		nTrackSection++;
	}
	
	// Log track progress
	LogTrackProgress(fDistance, nTrackSection);

	// Interpolate towards target track curvature more smoothly (with bounds checking)
	float fTargetCurvature = 0.0f; // Default to straight
	if (nTrackSection > 0 && nTrackSection <= static_cast<int>(vecTrack.size()) && !vecTrack.empty()) {
		int segmentIndex = nTrackSection - 1;
		if (segmentIndex >= 0 && segmentIndex < static_cast<int>(vecTrack.size())) {
			fTargetCurvature = vecTrack[segmentIndex].first;
		}
	}
	// Only update track curvature when moving
	if (fSpeed > 0.01f) {
		float fTrackCurveDiff = (fTargetCurvature - fCurvature) * deltaTime * 0.5f; // Reduced interpolation speed
		fCurvature += fTrackCurveDiff;
		fTrackCurvature += (fCurvature) * deltaTime * fSpeed * 0.7f;
	}

	// Draw sky
	for (int y = 0; y < GAME_RACING_HEIGHT / 2; y++)
		for (int x = 0; x < GAME_WIDTH; x++)
			PutPixel(pixels, x, y, black);

	// Draw Scenery - commented out for performance
	// Hills would be a rectified sine wave, where the phase is adjusted by accumulated track curvature
	// for (int x = 0; x < GAME_WIDTH; x++)
	// {
	// 	int nHillHeight = (int)(fabs(sinf(x * 0.01f + fTrackCurvature) * 16.0f));
	// 	for (int y = (GAME_RACING_HEIGHT / 2) - nHillHeight; y < GAME_RACING_HEIGHT / 2; y++)
	// 		PutPixel(pixels, x, y, yellow);
	// }

	if (SDL_LockSurface(pixels) != 0) {
		// Handle error: Unable to lock surface
		SDL_Log("Unable to lock surface: %s", SDL_GetError());
		return;
	}


	// Debug logging disabled to improve performance
	// SDL_Log("ACC: %d L: %d R: %d T: %.1f D: %f S: %d\n", accelerate, goLeft, goRight, fElapsedTime, fDistance, nTrackSection);

	// Draw cloud layer with parallax scrolling based on steering while moving
	if (cloudSprite && cloudSprite->GetSurface()) {
		SDL_Surface* cloudSurface = cloudSprite->GetSurface();
		int startY = 0;
		
		// Calculate horizontal offset with gradual decay when not turning
		static float cloudVelocity = 0.0f;
		if (fSpeed > 0.01f && fabs(fPlayerCurvature) > 0.01f) { // Actively turning AND moving
			cloudVelocity = fPlayerCurvature * fSpeed * 5.0f; // Much slower
		} else {
			cloudVelocity *= 0.85f; // Stronger decay when not turning (15% reduction per frame)
		}
		static float cloudOffsetAccum = 0.0f;
		cloudOffsetAccum += cloudVelocity * deltaTime;
		int cloudOffset = (int)cloudOffsetAccum % (cloudSurface->w);
		if (cloudOffset < 0) cloudOffset += cloudSurface->w;
		
		// Draw clouds with wrapping - draw twice to handle wrap-around seamlessly
		for (int pass = 0; pass < 2; pass++) {
			int drawX = -cloudOffset + (pass * cloudSurface->w);
			
			// Only draw if this pass is visible on screen
			if (drawX < GAME_WIDTH && (drawX + cloudSurface->w) > 0) {
				for (int y = 0; y < cloudSurface->h && (startY + y) < GAME_RACING_HEIGHT; y++) {
					for (int x = 0; x < cloudSurface->w; x++) {
						int screenX = drawX + x;
						if (screenX >= 0 && screenX < GAME_WIDTH) {
							// Get pixel from cloud sprite
							Uint32* cloudPixels = (Uint32*)cloudSurface->pixels;
							Uint32 pixel = cloudPixels[y * cloudSurface->w + x];
							
							// Convert to SDL_Color
							Uint8 r, g, b, a;
							SDL_GetRGBA(pixel, cloudSurface->format, &r, &g, &b, &a);
							
							// Only draw non-transparent pixels
							if (a > 0) {
								SDL_Color cloudColor = {r, g, b, a};
								PutPixel(pixels, screenX, startY + y, cloudColor);
							}
						}
					}
				}
			}
		}
	}

	// Draw mountain horizon as background with parallax scrolling based on car velocity
	if (mountainSprite && mountainSprite->GetSurface()) {
		SDL_Surface* mountainSurface = mountainSprite->GetSurface();
		int startY = 47;
		
		// Calculate horizontal offset with gradual decay when not turning
		static float mountainVelocity = 0.0f;
		if (fSpeed > 0.01f && fabs(fPlayerCurvature) > 0.01f) { // Actively turning AND moving
			mountainVelocity = fPlayerCurvature * fSpeed * 2.5f; // Much slower than clouds
		} else {
			mountainVelocity *= 0.85f; // Stronger decay when not turning (15% reduction per frame)
		}
		static float mountainOffsetAccum = 0.0f;
		mountainOffsetAccum += mountainVelocity * deltaTime;
		int scrollOffset = (int)mountainOffsetAccum % mountainSurface->w;
		if (scrollOffset < 0) scrollOffset += mountainSurface->w;
		
		// Draw mountains with wrapping - draw twice to handle wrap-around seamlessly
		for (int pass = 0; pass < 2; pass++) {
			int drawX = -scrollOffset + (pass * mountainSurface->w);
			
			// Only draw if this pass is visible on screen
			if (drawX < GAME_WIDTH && (drawX + mountainSurface->w) > 0) {
				for (int y = 0; y < mountainSurface->h && (startY + y) < GAME_RACING_HEIGHT; y++) {
					for (int x = 0; x < mountainSurface->w; x++) {
						int screenX = drawX + x;
						if (screenX >= 0 && screenX < GAME_WIDTH) {
							// Get pixel from mountain sprite
							Uint32* mountainPixels = (Uint32*)mountainSurface->pixels;
							Uint32 pixel = mountainPixels[y * mountainSurface->w + x];
							
							// Convert to SDL_Color
							Uint8 r, g, b, a;
							SDL_GetRGBA(pixel, mountainSurface->format, &r, &g, &b, &a);
							
							// Only draw non-transparent pixels
							if (a > 0) {
								SDL_Color mountainColor = {r, g, b, a};
								PutPixel(pixels, screenX, startY + y, mountainColor);
							}
						}
					}
				}
			}
		}
	}

	// Draw Track - Each row is split into grass, clip-board and track
	// Render from horizon to bottom of screen (inclusive)
	for (int y = 0; y <= GAME_RACING_HEIGHT / 2; y++) {
		for (int x = 0; x < GAME_WIDTH; x++) {

			// Perspective is used to modify the width of the track row segments
			float fPerspective = (float)y / (GAME_RACING_HEIGHT/2.0f);
			// Original Street Rod: horizon width 68px, bottom width 313px out of 320px screen
			// Need much wider road: horizon 21%, bottom 98% (before halving)
			float fRoadWidth = 0.21f + fPerspective * 0.98f; // Min 21% Max 98% to match original
			float fClipWidth = 0.0f; // Remove clip border - no red/white stripes
			float fMidLineWidth = fRoadWidth * 0.03;
			fRoadWidth *= 0.8f;	// Halve it as track is symmetrical around center of track, but offset...

		// ...depending on where the middle point is, which is defined by the current
		// track curvature. Balanced curvature effect.
		float fMiddlePoint = 0.5f + fCurvature * 0.4f * powf((1.0f - fPerspective), 2.5f);
		
		// Shift road laterally based on car position (first-person view)
		// This is a simple horizontal shift, not a curve
		// When fCarPos is positive (turning right), road shifts left (you see more grass on left)
		// When fCarPos is negative (turning left), road shifts right (you see more grass on right)
		fMiddlePoint -= fCarPos * 0.25f; // Linear shift, no perspective effect

			// Work out segment boundaries
			int nLeftGrass = (fMiddlePoint - fRoadWidth - fClipWidth) * GAME_WIDTH;
			int nLeftClip = (fMiddlePoint - fRoadWidth) * GAME_WIDTH;
			int nRightClip = (fMiddlePoint + fRoadWidth) * GAME_WIDTH;
			int nRightGrass = (fMiddlePoint + fRoadWidth + fClipWidth) * GAME_WIDTH;
			int nLeftMidline = (fMiddlePoint - fMidLineWidth) * GAME_WIDTH;
			int nRightMidline = (fMiddlePoint + fMidLineWidth) * GAME_WIDTH;

			int nRow = GAME_RACING_HEIGHT / 2 + y;

			// Using periodic oscillatory functions to give lines, where the phase is controlled
			// by the distance around the track. These take some fine tuning to give the right "feel"
			SDL_Color nGrassColour = sinf(20.0f *  powf(1.0f - fPerspective,3) + fDistance * 0.1f) > 0.0f ? green : darkGreen;
			SDL_Color nClipColour = sinf(80.0f *  powf(1.0f - fPerspective, 2) + fDistance) > 0.0f ? red : white;

            // Flash red if crashed
            SDL_Color nRoadColour = bCrashed ? SDL_Color{170, 0, 0, 255} : black;

			// Draw the row segments
			if (x >= 0 && x < nLeftGrass)
				PutPixel(pixels, x, nRow, nGrassColour);
			if (x >= nLeftGrass && x < nLeftClip)
				PutPixel(pixels, x, nRow, nClipColour);
			if (x >= nLeftClip && x < nRightClip)
				PutPixel(pixels, x, nRow, nRoadColour);
			// Draw dashed center line
			if (x >= nLeftMidline && x < nRightMidline) {
				// Create dash pattern based on distance and perspective
				float dashPhase = -fDistance * 0.3f + y * 0.3f; // Higher multiplier for shorter dashes
				bool drawDash = sinf(dashPhase) > 0.7f; // Higher threshold for longer gaps between dashes
				if (drawDash) {
					PutPixel(pixels, x, nRow, yellow);
				}
			}
			if (x >= nRightClip && x < nRightGrass)
				PutPixel(pixels, x, nRow, nClipColour);
			if (x >= nRightGrass && x < GAME_WIDTH)
				PutPixel(pixels, x, nRow, nGrassColour);


		}
	}

	// Draw off-road indicator bar at bottom of screen
	// Shows how far off-road the car is (player curvature vs track curvature)
	int indicatorY = GAME_RACING_HEIGHT - 5;
	int centerX = GAME_WIDTH / 2;
	
	// Draw center line (road center)
	for (int y = indicatorY - 8; y < indicatorY; y++) {
		PutPixel(pixels, centerX, y, white);
	}
	
	// Draw car position indicator
	int carIndicatorX = centerX + (int)(fCarPos * 50.0f); // Scale for visibility
	SDL_Color carColor = bCrashed ? SDL_Color{255, 0, 0, 255} : 
	                     (fabs(fCarPos) > 1.0f) ? SDL_Color{255, 165, 0, 255} : // Orange warning
	                     SDL_Color{0, 255, 0, 255}; // Green = safe
	
	// Draw car indicator (larger dot)
	for (int dy = -3; dy <= 3; dy++) {
		for (int dx = -3; dx <= 3; dx++) {
			int px = carIndicatorX + dx;
			int py = indicatorY - 4 + dy;
			if (px >= 0 && px < GAME_WIDTH && py >= 0 && py < GAME_RACING_HEIGHT) {
				PutPixel(pixels, px, py, carColor);
			}
		}
	}
	
	// Draw overhead signs LAST (after clouds, mountains, road, indicators)
	// This ensures arch posts are visible in sky AND yellow ground is on top of road
	DrawOverheadSigns(fDistance);
	
	SDL_UnlockSurface(pixels);
	
	// Draw sprite bitmaps after unlocking the surface
	DrawRoadsideSprites(fDistance);
	
	// Draw overhead sign bitmaps after unlocking
	DrawOverheadSignBitmaps(fDistance);

	// Draw Car - car position on road is proportional to difference between
	// current accumulated track curvature, and current accumulated player curvature
	// i.e. if they are similar, the car will be in the middle of the track
	// fCarPos = fPlayerCurvature - fTrackCurvature;
	// int nCarPos = GAME_WIDTH / 2 + ((int)(GAME_WIDTH * fCarPos) / 2.0) - 7; // Offset for sprite

	// auto disp_time = [](float t) // Little lambda to turn floating point seconds into minutes:seconds:millis string
	// {
	// 	int nMinutes = t / 60.0f;
	// 	int nSeconds = t - (nMinutes * 60.0f);
	// 	int nMilliSeconds = (t - (float)nSeconds) * 1000.0f;
	// 	return to_wstring(nMinutes) + L"." + to_wstring(nSeconds) + L":" + to_wstring(nMilliSeconds);
	// };

	// Display current laptime
	// DrawString(10, 8, disp_time(fCurrentLapTime));

	// Display last 5 lap times
	// int j = 10;
	// for (auto l : listLapTimes)
	// {
	// 	DrawString(10, j, disp_time(l));
	// 	j++;
	// }
	
	// Draw rearview mirror as the absolute last thing (after all sprites)
	DrawRearviewMirror(6); // Lower value to leave room for mini road

	// Update and draw HUD
	// Convert fDistance from track units to miles (using scale factor from track analysis)
	// Scale factor: 2.200 feet per segment = 0.000417 miles per unit
	float distanceInMiles = fDistance * 0.000417f;
	
	// Convert velocity to mph
	// Speed depends on both RPM (velocity) and gear ratio
	// Higher gear = faster at same RPM
	// Reuse gearMultiplier calculated earlier for distance
	int speedMph = static_cast<int>((velocity / 70.0f) * gearMultiplier);
	
	// Format gear display
	char gearText[32];
	if (isAutomatic) {
		snprintf(gearText, sizeof(gearText), "Gear: D");
	} else {
		snprintf(gearText, sizeof(gearText), "Gear: %d", currentGear + 1);
	}
	
	// Format HUD text with fixed-width fields to prevent jumping
	char hudText[128];
	const char* warning = "";
	if (transmissionDamage > 0) {
		warning = " ⚠️ OVER-REV!";
	}
	snprintf(hudText, sizeof(hudText), "Distance:%5.2f miles  Speed:%3d mph  RPM:%4d%s", 
	         distanceInMiles, speedMph, rpm, warning);
	
	hud->Clear();
	// Add Gear text at position (6, 0) relative to HUD bar (absolute screen pos 6, 191)
	hud->AddText(6, 0, gearText);
	// Main text starts after Gear text (around X=55 to leave space)
	hud->AddText(55, 0, hudText);
	hud->Draw();

//		return true;
}

void Racing::PutPixel(SDL_Surface* s, int x, int y, SDL_Color c) {
	// Check if the surface is valid and within bounds
	if (s == nullptr) {
		return;
	}

	// Check if the pixel coordinates are within the surface bounds
	if (x < 0 || x >= s->w || y < 0 || y >= s->h) {
		return;
	}

	// Get the pixel format of the surface
	SDL_PixelFormat* format = s->format;

	// Calculate the pixel offset based on the coordinates
	int pixelOffset = y * s->pitch + x * format->BytesPerPixel;

	// Get a pointer to the pixel data at the calculated offset
	Uint8* pixel = (Uint8*)s->pixels + pixelOffset;

	// Set the pixel color
	*(pixel + format->Rshift / 8) = c.r;   // Red component
	*(pixel + format->Gshift / 8) = c.g;   // Green component
	*(pixel + format->Bshift / 8) = c.b;   // Blue component
	if (format->BytesPerPixel > 3) {
		*(pixel + format->Ashift / 8) = c.a; // Alpha component (if exists)
	}
}

// Street Rod track integration methods
void Racing::LoadTrackData(const std::vector<uint8_t>& binaryData, SrTrackType trackType) {
    printf("Racing::LoadTrackData() - Loading %s track\n", 
           trackType == SR_TRACK_ROAD_RACE ? "Road Race" : "Drag Race");
    
    currentTrackType = trackType;
    
    if (trackReader && trackReader->LoadTrackData(binaryData)) {
        // Extract track coordinates 
        int maxSegments = (trackType == SR_TRACK_ROAD_RACE) ? 500 : 200;
        currentTrackSegments = trackReader->ExtractTrackCoordinates(trackType, maxSegments);
        
        printf("Loaded %zu track segments\n", currentTrackSegments.size());
        
        if (!currentTrackSegments.empty()) {
            // Convert to racing format
            ConvertTrackCoordinatesToRacing();
            printf("Converted to %zu racing track sections\n", vecTrack.size());
            
            // Load roadside sprites for this track
            currentRoadsideSprites = trackReader->ExtractRoadSideSprites(SR_ROADSIDE_SPRITES_OFFSET, 100);
            printf("Loaded %zu roadside sprites\n", currentRoadsideSprites.size());
            
            // Reset sprite tracking
            passedSprites.clear();
            lastLoggedDistance = -1.0f;
            
            // Reset progress tracking
            lastProgressLogDistance = -1.0f;
            lastLoggedSegment = -1;
            
            // Recalculate total track distance
            fTrackDistance = 0.0f;
            for (auto t : vecTrack) {
                fTrackDistance += t.second;
            }
            printf("Total track distance: %.1f units\n", fTrackDistance);
        } else {
            printf("ERROR: No track segments loaded!\n");
        }
    } else {
        printf("ERROR: Failed to load track data!\n");
    }
}

void Racing::SetTrackType(SrTrackType trackType) {
    // This will be used to switch tracks dynamically if needed
    currentTrackType = trackType;
}

void Racing::ConvertTrackCoordinatesToRacing() {
    vecTrack.clear();
    
    if (currentTrackSegments.empty()) {
        printf("ERROR: No track segments to convert!\n");
        return;
    }
    
    printf("Converting %zu segments to racing format...\n", currentTrackSegments.size());
    
    for (size_t i = 0; i < currentTrackSegments.size(); ++i) {
        const auto& segment = currentTrackSegments[i];
        
        // Calculate segment length
        float length = CalculateSegmentLength(segment);
        
        // Calculate curvature based on direction change
        float curvature = 0.0f;
        if (i > 0) {
            curvature = CalculateSegmentCurvature(currentTrackSegments[i-1], segment);
        }
        
        // Scale length to reasonable racing units - fix excessive scaling
        float scaledLength = length * 2.0f;  // Reduced from 15.0f - was way too large
        if (scaledLength < 5.0f) scaledLength = 5.0f; // Reduced minimum segment length
        
        // Scale curvature more aggressively to make track differences visible
        float scaledCurvature = curvature * 0.8f;  // Increased from 0.1f
        if (scaledCurvature > 0.4f) scaledCurvature = 0.4f;   // Increased limits
        if (scaledCurvature < -0.4f) scaledCurvature = -0.4f;
        
        vecTrack.push_back(std::make_pair(scaledCurvature, scaledLength));
        
        // Debug output for first few segments
        if (i < 5) {
            printf("Segment %zu: length=%.1f, curvature=%.3f (scaled: %.1f, %.3f)\n", 
                   i, length, curvature, scaledLength, scaledCurvature);
        }
    }
}

float Racing::CalculateSegmentLength(const SrTrackCoordSegment& segment) {
    int dx = segment.xEnd - segment.xStart;
    int dy = segment.yEnd - segment.yStart;
    return sqrt(dx * dx + dy * dy);
}

float Racing::CalculateSegmentCurvature(const SrTrackCoordSegment& segment1, const SrTrackCoordSegment& segment2) {
    // Calculate direction vectors
    float dx1 = segment1.xEnd - segment1.xStart;
    float dy1 = segment1.yEnd - segment1.yStart;
    float dx2 = segment2.xEnd - segment2.xStart;
    float dy2 = segment2.yEnd - segment2.yStart;
    
    // Normalize direction vectors
    float len1 = sqrt(dx1 * dx1 + dy1 * dy1);
    float len2 = sqrt(dx2 * dx2 + dy2 * dy2);
    
    if (len1 == 0 || len2 == 0) return 0.0f;
    
    dx1 /= len1; dy1 /= len1;
    dx2 /= len2; dy2 /= len2;
    
    // Calculate angle between vectors using cross product
    // Positive = right turn, negative = left turn
    float cross = dx1 * dy2 - dy1 * dx2;
    
    // Scale the curvature based on the magnitude of direction change
    return cross;
}

void Racing::CheckForSpriteEncounters(float currentDistance) {
    if (currentRoadsideSprites.empty()) {
        return; // No sprites loaded
    }
    
    // Only check every few units to avoid spam
    if (fabs(currentDistance - lastLoggedDistance) < 10.0f) {
        return;
    }
    
    // Look for sprites near current position
    const float ENCOUNTER_DISTANCE = 15.0f; // How close we need to be to "encounter" a sprite
    
    for (size_t i = 0; i < currentRoadsideSprites.size(); ++i) {
        const auto& sprite = currentRoadsideSprites[i];
        
        // Check both sprite positions in the record
        float spritePos1 = static_cast<float>(sprite.trackPosition1);
        float spritePos2 = static_cast<float>(sprite.trackPosition2);
        
        // Check if we're close to sprite 1
        if (fabs(currentDistance - spritePos1) <= ENCOUNTER_DISTANCE) {
            int spriteKey = (i * 2); // Unique key for sprite 1 in this record
            if (passedSprites.find(spriteKey) == passedSprites.end()) {
                passedSprites.insert(spriteKey);
                printf("🏁 PASSED: %s (ID:%d, Hex:0x%02X, Bin:", 
                       TrackReader::GetSpriteName(sprite.spriteId1), sprite.spriteId1, sprite.spriteId1);
                for (int bit = 7; bit >= 0; bit--) {
                    printf("%d", (sprite.spriteId1 >> bit) & 1);
                }
                printf(", LEFT SIDE) at position %.1f (distance: %.1f)\n", spritePos1, currentDistance);
            }
        }
        
        // Check if we're close to sprite 2
        if (sprite.spriteId2 != 0 && fabs(currentDistance - spritePos2) <= ENCOUNTER_DISTANCE) {
            int spriteKey = (i * 2) + 1; // Unique key for sprite 2 in this record
            if (passedSprites.find(spriteKey) == passedSprites.end()) {
                passedSprites.insert(spriteKey);
                printf("🏁 PASSED: %s (ID:%d, Hex:0x%02X, Bin:", 
                       TrackReader::GetSpriteName(sprite.spriteId2), sprite.spriteId2, sprite.spriteId2);
                for (int bit = 7; bit >= 0; bit--) {
                    printf("%d", (sprite.spriteId2 >> bit) & 1);
                }
                printf(", RIGHT SIDE) at position %.1f (distance: %.1f)\n", spritePos2, currentDistance);
            }
        }
    }
    
    lastLoggedDistance = currentDistance;
    
    // Reset passed sprites when completing a lap
    if (currentDistance < lastLoggedDistance - 100.0f) {
        printf("🏁 LAP COMPLETED - Resetting sprite tracking\n");
        passedSprites.clear();
    }
}

void Racing::LogTrackProgress(float currentDistance, int currentSegment) {
    // Log progress every 100 distance units or when changing segments significantly
    const float PROGRESS_LOG_INTERVAL = 100.0f;
    const int SEGMENT_LOG_INTERVAL = 10; // Log every 10 segments
    
    bool shouldLogDistance = (fabs(currentDistance - lastProgressLogDistance) >= PROGRESS_LOG_INTERVAL);
    bool shouldLogSegment = (abs(currentSegment - lastLoggedSegment) >= SEGMENT_LOG_INTERVAL);
    
    if (shouldLogDistance || shouldLogSegment) {
        // Calculate completion percentage
        float completionPercentage = (fTrackDistance > 0) ? (currentDistance / fTrackDistance) * 100.0f : 0.0f;
        
        // Get current segment info if available
        const char* trackTypeName = (currentTrackType == SR_TRACK_ROAD_RACE) ? "Road Race" : "Drag Race";
        
        // Determine if current segment is curved or straight (with bounds checking)
        const char* segmentType = "UNKNOWN";
        if (currentSegment > 0 && currentSegment <= static_cast<int>(vecTrack.size()) && !vecTrack.empty()) {
            int segmentIndex = currentSegment - 1;
            if (segmentIndex >= 0 && segmentIndex < static_cast<int>(vecTrack.size())) {
                float curvature = vecTrack[segmentIndex].first;
                if (fabs(curvature) < 0.05f) {
                    segmentType = "STRAIGHT";
                } else if (curvature > 0) {
                    segmentType = "RIGHT TURN";
                } else {
                    segmentType = "LEFT TURN";
                }
            }
        }
        
        printf("📍 PROGRESS: %.1f%% complete | Distance: %.1f/%.1f | Segment: %d/%zu (%s) | Track: %s\n",
               completionPercentage,
               currentDistance,
               fTrackDistance,
               currentSegment,
               vecTrack.size(),
               segmentType,
               trackTypeName);
        
        lastProgressLogDistance = currentDistance;
        lastLoggedSegment = currentSegment;
    }
    
    // Special log for lap completion
    if (currentDistance < lastProgressLogDistance - 50.0f) {
        printf("🏁 LAP COMPLETED! Starting new lap on %s track\n",
               (currentTrackType == SR_TRACK_ROAD_RACE) ? "Road Race" : "Drag Race");
        lastProgressLogDistance = 0.0f;
        lastLoggedSegment = 0;
    }
}

void Racing::DrawRoadsideSprites(float currentDistance) {
    if (currentRoadsideSprites.empty()) {
        return;
    }
    
    // Look for sprites near current position and render them as simple visual indicators
    const float VISIBLE_DISTANCE = 200.0f; // How far ahead/behind we can see sprites
    
    for (const auto& sprite : currentRoadsideSprites) {
        // Check both sprite positions in the record
        float spritePos1 = static_cast<float>(sprite.spriteId1 != 0 ? sprite.trackPosition1 : 0);
        float spritePos2 = static_cast<float>(sprite.spriteId2 != 0 ? sprite.trackPosition2 : 0);
        
        // Draw sprite 1 if visible
        if (sprite.spriteId1 != 0) {
            float distance = spritePos1 - currentDistance;
            if (distance >= -50.0f && distance <= VISIBLE_DISTANCE) {
                DrawSpriteIndicator(distance, sprite.spriteId1, true); // true = left side
                
                // Check collision with sprite (when sprite is very close)
                if (!bCrashed && distance >= -5.0f && distance <= 10.0f) {
                    if (CheckCarCollisionWithSprite(distance, true)) {
                        HandleCrash();
                    }
                }
            }
        }
        
        // Draw sprite 2 if visible
        if (sprite.spriteId2 != 0) {
            float distance = spritePos2 - currentDistance;
            if (distance >= -50.0f && distance <= VISIBLE_DISTANCE) {
                DrawSpriteIndicator(distance, sprite.spriteId2, false); // false = right side
                
                // Check collision with sprite (when sprite is very close)
                if (!bCrashed && distance >= -5.0f && distance <= 10.0f) {
                    if (CheckCarCollisionWithSprite(distance, false)) {
                        HandleCrash();
                    }
                }
            }
        }
    }
}

void Racing::DrawSpriteIndicator(float distance, uint16_t spriteId, bool leftSide) {
    // Convert distance to screen Y position (perspective)
    if (distance < 0) return; // Don't draw sprites behind us
    
    float perspective = 1.0f - (distance / 100.0f); // Reduce distance scaling for better perspective range
    if (perspective <= 0) return;
    
    int screenY = static_cast<int>((GAME_RACING_HEIGHT / 2) + perspective * (GAME_RACING_HEIGHT / 2));
    if (screenY >= GAME_RACING_HEIGHT) return;
    
    // Calculate road width at this perspective (same as road rendering)
    float fRoadWidth = 0.21f + perspective * 0.98f;
    fRoadWidth *= 0.8f;
    
    // Middle point with curvature effect
    float fMiddlePoint = 0.5f + fCurvature * 0.4f * powf((1.0f - perspective), 2.5f);
    
    // Shift road laterally based on car position (must match road rendering)
    fMiddlePoint -= fCarPos * 0.25f; // Linear shift, no perspective effect
    
    // Calculate sprite position (clearly off the road - at roadside)
    int spriteX;
    if (leftSide) {
        spriteX = static_cast<int>((fMiddlePoint - fRoadWidth - 0.05f) * GAME_WIDTH); // Just left of road edge
    } else {
        spriteX = static_cast<int>((fMiddlePoint + fRoadWidth + 0.05f) * GAME_WIDTH); // Just right of road edge
    }
    
    // Choose the appropriate sprite bitmap and handle animation
    Bitmap* spriteBitmap = nullptr;
    SDL_Rect animFrameRect = {0, 0, 0, 0}; // For animated sprites
    bool useAnimation = false;
    spriteId = 32;
    
    // Check if this sprite has animation data (don't override spriteId)
    const SpriteAnimationInfo* animInfo = GetSpriteAnimationInfo(spriteId);
    
    switch (spriteId) {
        case 30: // Telephone Pole (RACING_TELEPHONE_POLES)
            spriteBitmap = telephonePoleSprite.get();
            break;
        case 33: // Road Posts
        case 22: // Road Posts (alt)
            spriteBitmap = roadPostSprite.get();
            break;
        case 31: // Tree 31 (RACING_TREE1) - use TREE2 sprite for consistency
            spriteBitmap = treeSprite.get();
            break;
        case 32: // Tree 32 (RACING_TREE2) - ANIMATED!
            spriteBitmap = treeSprite.get();
            if (animInfo && animInfo->frameCount > 1) {
                // Distance thresholds for each frame transition (11 frames total)
                // Frame 0 = smallest (far away), Frame 10 = largest (close up)
                static const float frameDistances[] = {
                    100.0f, 85.0f, 70.0f, 58.0f, 48.0f, 40.0f, 33.0f, 27.0f, 22.0f, 18.0f, 0.0f
                };
                
                uint8_t frame = 0;
                for (int i = 0; i < animInfo->frameCount; i++) {
                    if (distance < frameDistances[i]) {
                        frame = i;
                        break;
                    }
                }
                frame = std::min(frame, static_cast<uint8_t>(animInfo->frameCount - 1));
                
                SpriteRect frameRect = GetAnimationFrameRect(animInfo, frame);
                // Convert SpriteRect to SDL_Rect manually
                animFrameRect = {
                    static_cast<int>(frameRect.x),
                    static_cast<int>(frameRect.y),
                    static_cast<int>(frameRect.w),
                    static_cast<int>(frameRect.h)
                };
                useAnimation = true;
                
                // Debug output for tree animation
                static int debugCount = 0;
                if (debugCount < 20) {
                    printf("Tree #%d: distance=%.1f, perspective=%.3f, frame=%d/%d, rect=(%d,%d,%d,%d)\n", 
                           spriteId, distance, perspective, frame, animInfo->frameCount,
                           animFrameRect.x, animFrameRect.y, animFrameRect.w, animFrameRect.h);
                    debugCount++;
                }
            }
            break;
        case 25: // Bush (RACING_BUSH)
            spriteBitmap = bushSprite.get();
            break;
        default:
            // Use road posts as default for unknown sprites
            spriteBitmap = roadPostSprite.get();
            break;
    }
    
    if (spriteBitmap && spriteBitmap->GetSurface()) {
        SDL_Surface* spriteSurface = spriteBitmap->GetSurface();
        // Bitmap sprite rendering active
        
        // Determine source rectangle (animation frame or full sprite)
        SDL_Rect srcRect;
        if (useAnimation) {
            // Use the specific animation frame
            srcRect = animFrameRect;
        } else {
            // Use the entire sprite
            srcRect = {0, 0, spriteSurface->w, spriteSurface->h};
        }
        
        // Use sprite frames at their intended pixel dimensions (no scaling)
        int spriteWidth = srcRect.w;
        int spriteHeight = srcRect.h;
        
        // Center the sprite horizontally and place it on the ground
        int drawX = spriteX - spriteWidth / 2;
        int drawY = screenY - spriteHeight;
        
        // Set up destination rectangle (1:1 pixel mapping)
        SDL_Rect dstRect = {drawX, drawY, spriteWidth, spriteHeight};
        
        // Store original source rectangle for proper clamping with animation frames
        SDL_Rect originalSrcRect = srcRect;
        
        // Clamp destination rectangle to screen bounds (1:1 pixel mapping, no scaling)
        if (dstRect.x < 0) {
            int leftClip = -dstRect.x;
            srcRect.x = originalSrcRect.x + leftClip;
            srcRect.w = originalSrcRect.w - leftClip;
            dstRect.w += dstRect.x;
            dstRect.x = 0;
        }
        if (dstRect.x + dstRect.w > GAME_WIDTH) {
            int overflow = (dstRect.x + dstRect.w) - GAME_WIDTH;
            srcRect.w = originalSrcRect.w - overflow;
            dstRect.w -= overflow;
        }
        if (dstRect.y < 0) {
            int topClip = -dstRect.y;
            srcRect.y = originalSrcRect.y + topClip;
            srcRect.h = originalSrcRect.h - topClip;
            dstRect.h += dstRect.y;
            dstRect.y = 0;
        }
        if (dstRect.y + dstRect.h > GAME_RACING_HEIGHT) {
            int overflow = (dstRect.y + dstRect.h) - GAME_RACING_HEIGHT;
            srcRect.h = originalSrcRect.h - overflow;
            dstRect.h -= overflow;
        }
        
        // Only draw if there's something visible
        if (srcRect.w > 0 && srcRect.h > 0 && dstRect.w > 0 && dstRect.h > 0) {
            int result = SDL_BlitSurface(spriteBitmap->GetSurface(), &srcRect, pixels, &dstRect);
            if (result != 0) {
                printf("SDL_BlitSurface failed: %s\n", SDL_GetError());
            }
        }
    }
}

void Racing::LoadSpriteBitmaps() {
    printf("Racing: Loading sprite bitmaps from AssetManager...\n");
    
    // Load telephone pole sprite (asset ID 30)
    auto* telephonePoleAsset = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_TELEPHONE_POLES});
    if (telephonePoleAsset) {
        telephonePoleSprite = std::make_unique<Bitmap>(*telephonePoleAsset);
        if (telephonePoleSprite->GetSurface()) {
            printf("Racing: Loaded telephone pole sprite (%dx%d)\n", 
                   telephonePoleSprite->GetSurface()->w, telephonePoleSprite->GetSurface()->h);
        }
    } else {
        printf("Racing: Failed to load telephone pole sprite\n");
    }
    
    // Load tree sprite (asset ID 32 - RACING_TREE2 for animation)
    auto* treeAsset = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_TREE2});
    if (treeAsset) {
        treeSprite = std::make_unique<Bitmap>(*treeAsset);
        if (treeSprite->GetSurface()) {
            printf("Racing: Loaded tree sprite RACING_TREE2 (%dx%d)\n", 
                   treeSprite->GetSurface()->w, treeSprite->GetSurface()->h);
        }
    } else {
        printf("Racing: Failed to load tree sprite RACING_TREE2\n");
    }
    
    // Load bush sprite (asset ID 25)
    auto* bushAsset = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_BUSH});
    if (bushAsset) {
        bushSprite = std::make_unique<Bitmap>(*bushAsset);
        if (bushSprite->GetSurface()) {
            printf("Racing: Loaded bush sprite (%dx%d)\n", 
                   bushSprite->GetSurface()->w, bushSprite->GetSurface()->h);
        }
    } else {
        printf("Racing: Failed to load bush sprite\n");
    }
    
    // Load road post sprite (asset ID 22)
    auto* roadPostAsset = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_ROAD_POST});
    if (roadPostAsset) {
        roadPostSprite = std::make_unique<Bitmap>(*roadPostAsset);
        if (roadPostSprite->GetSurface()) {
            printf("Racing: Loaded road post sprite (%dx%d)\n", 
                   roadPostSprite->GetSurface()->w, roadPostSprite->GetSurface()->h);
        }
    } else {
        printf("Racing: Failed to load road post sprite\n");
    }
    
    // Load mountain sprite (asset ID 13) for horizon
    auto* mountainAsset = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_MOUNTAINS});
    if (mountainAsset) {
        mountainSprite = std::make_unique<Bitmap>(*mountainAsset);
        if (mountainSprite->GetSurface()) {
            printf("Racing: Loaded mountain sprite (%dx%d)\n", 
                   mountainSprite->GetSurface()->w, mountainSprite->GetSurface()->h);
        }
    } else {
        printf("Racing: Failed to load mountain sprite\n");
    }
    
    // Load cloud sprite (asset ID 12) for sky with parallax scrolling
    auto* cloudAsset = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_CLOUDS});
    if (cloudAsset) {
        cloudSprite = std::make_unique<Bitmap>(*cloudAsset);
        if (cloudSprite->GetSurface()) {
            printf("Racing: Loaded cloud sprite (%dx%d)\n", 
                   cloudSprite->GetSurface()->w, cloudSprite->GetSurface()->h);
        }
    } else {
        printf("Racing: Failed to load cloud sprite\n");
    }
    
    // Load rearview mirror horizon sprite (asset ID 82)
    auto* rearviewAsset = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_HORIZON_REARVIEWMIRROR});
    if (rearviewAsset) {
        rearviewMirrorSprite = std::make_unique<Bitmap>(*rearviewAsset);
        if (rearviewMirrorSprite->GetSurface()) {
            printf("Racing: Loaded rearview mirror sprite (%dx%d)\n", 
                   rearviewMirrorSprite->GetSurface()->w, rearviewMirrorSprite->GetSurface()->h);
        }
    } else {
        printf("Racing: Failed to load rearview mirror sprite\n");
    }
    
    // Load overhead sign sprites (these are rendered as pseudo-3D trapezoids)
    // Discovery: Original game renders these as trapezoids ABOVE the road, not as blitted sprites
    printf("Racing: Loading overhead sign sprites...\n");
    
    auto* countryLineAsset = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_SIGN_COUNTY_LINE});
    if (countryLineAsset) {
        signCountryLineSprite = std::make_unique<Bitmap>(*countryLineAsset);
        printf("Racing: Loaded COUNTRY LINE sign (ID 14) - finish line arch\n");
    }
    
    auto* route69Asset = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_SIGN_ROUTE_69});
    if (route69Asset) {
        signRoute69Sprite = std::make_unique<Bitmap>(*route69Asset);
        printf("Racing: Loaded ROUTE 69 sign (ID 33) - overhead arch\n");
    }
    
    auto* cityLimitsAsset = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_SIGN_CITY_LIMITS});
    if (cityLimitsAsset) {
        signCityLimitsSprite = std::make_unique<Bitmap>(*cityLimitsAsset);
        printf("Racing: Loaded CITY LIMITS sign (ID 34) - overhead arch\n");
    }
    
    auto* exit48Asset = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_SIGN_EXIT_48});
    if (exit48Asset) {
        signExit48Sprite = std::make_unique<Bitmap>(*exit48Asset);
        printf("Racing: Loaded EXIT 48 sign (ID 35) - overhead arch\n");
    }
}

void Racing::DrawRearviewMirror(int mirrorLevel) {
    
    // Draw a fully opaque red rectangle for testing - adjust Y for racing surface
    // Original coordinates: 242,23 (screen coords) -> 242,9 (racing surface coords)  
    SDL_Rect mirrorRect = {242, 9, 76, 21}; // Y=23-14=9 (racing surface offset)
    Uint32 black = SDL_MapRGBA(pixels->format, 0, 0, 0, 255);
    SDL_FillRect(pixels, &mirrorRect, black);
    
    // Add horizon sprite if available
    if (rearviewMirrorSprite && rearviewMirrorSprite->GetSurface()) {
        SDL_Surface* horizonSprite = rearviewMirrorSprite->GetSurface();
        
        // Set clipping rectangle to mirror bounds
        SDL_Rect clipRect = {242, 9, 76, 21};
        SDL_SetClipRect(pixels, &clipRect);
        
        // Draw horizon sprite - SDL will automatically clip it
        SDL_Rect srcRect = {1, 0, 76, 11};
        SDL_Rect dstRect = {242, 9 + mirrorLevel, 76, 11};
        SDL_BlitSurface(horizonSprite, &srcRect, pixels, &dstRect);
        
        // Draw mini road below horizon sprite
        const int MIRROR_X = 242;
        const int MIRROR_Y = 9;
        const int MIRROR_WIDTH = 76;
        const int MIRROR_HEIGHT = 21;
        
        int roadStartY = mirrorLevel + 11; // Start after horizon sprite
        int roadEndY = MIRROR_Y + MIRROR_HEIGHT; // Bottom of mirror
        
        if (roadStartY < roadEndY) {
            float roadHeight = (float)(roadEndY - roadStartY);
            
            for (int y = roadStartY; y < roadEndY; y++) {
                // Calculate perspective (0.0 at top, 1.0 at bottom)
                float perspective = (y - roadStartY) / roadHeight;
                
                // Road width with perspective (narrower at top, wider at bottom)
                float roadWidthRatio = 0.4f + perspective * 0.5f; // 40% to 90% of mirror width
                int roadWidth = (int)(MIRROR_WIDTH * roadWidthRatio);
                
                // Apply reverse curvature (opposite of main road for rearview)
                int curveShift = (int)(-fCurvature * perspective * 20.0f);
                
                // Calculate road boundaries
                int roadCenter = MIRROR_X + MIRROR_WIDTH / 2 + curveShift;
                int roadLeft = roadCenter - roadWidth / 2;
                int roadRight = roadCenter + roadWidth / 2;
                
                // Draw the row
                for (int x = MIRROR_X; x < MIRROR_X + MIRROR_WIDTH; x++) {
                    Uint32 color;
                    
                    if (x >= roadLeft && x <= roadRight) {
                        // Road surface - black
                        color = SDL_MapRGBA(pixels->format, 0, 0, 0, 255);
                        
                        // Animated center line dashes (opposite direction for rearview)
                        if (abs(x - roadCenter) <= 1) {
                            float dashPhase = fDistance * 0.3f + y * 0.3f; // Positive fDistance for reverse
                            if (sinf(dashPhase) > 0.7f) {
                                color = SDL_MapRGBA(pixels->format, 255, 255, 0, 255);
                            }
                        }
                    } else {
                        // Grass - green
                        color = SDL_MapRGBA(pixels->format, 0, 119, 0, 255);
                    }
                    
                    Uint32* pixelPtr = (Uint32*)((Uint8*)pixels->pixels + y * pixels->pitch + x * pixels->format->BytesPerPixel);
                    *pixelPtr = color;
                }
            }
        }
        
        // Remove clipping
        SDL_SetClipRect(pixels, nullptr);
    }
}

bool Racing::CheckCarCollisionWithSprite(float spriteDistance, bool leftSide) {
    // Check if car trajectory will hit the roadside sprite
    // In first-person view, fCarPos = fPlayerCurvature - fTrackCurvature
    // Negative fCarPos = drifting left, Positive = drifting right
    
    const float COLLISION_THRESHOLD = 0.8f; // How far off-road before hitting sprites
    const float COLLISION_DISTANCE = 5.0f; // Must be within this distance
    
    if (spriteDistance > COLLISION_DISTANCE) return false;
    
    // Check if car is drifting toward the sprite's side
    if (leftSide && fCarPos < -COLLISION_THRESHOLD) {
        return true; // Drifting left into left-side sprite
    }
    
    if (!leftSide && fCarPos > COLLISION_THRESHOLD) {
        return true; // Drifting right into right-side sprite
    }
    
    return false;
}

void Racing::HandleCrash() {
	if (bCrashed) return; // Already crashed
	
	bCrashed = true;
	fCrashRecoveryTime = 2.0f; // 2 second crash recovery time
	
	SDL_Log("💥 CRASH! Speed: %.2f, Car Position: %.2f", fSpeed, fCarPos);
}

// ============================================================================
// PHYSICS IMPLEMENTATION
// Based on Street Rod assembly code analysis (racing.md lines 49084-49620)
// ============================================================================

void Racing::UpdatePhysics(float deltaTime, bool accelerating) {
	// Constants from assembly analysis
	// NOTE: velocity variable represents engine RPM, not road speed
	const int16_t MIN_RPM = 1500;           // Idle RPM
	const int16_t MAX_RPM_MANUAL = 7000;    // Max over-rev
	const int16_t MAX_RPM_AUTO = 5690;      // Auto transmission limit
	
	// Original game ran at ~60 FPS (racing.md line 49: "60 FPS on 7MHz 68000")
	// Scale our per-frame values to match that timing
	const float TARGET_FPS = 60.0f;
	const float frameScale = deltaTime * TARGET_FPS;  // 1.0 at 60fps, 0.5 at 120fps, etc.
	
	// Initialize intermediate on first update (matches original game: velocity * gear_ratio)
	static bool initialized = false;
	if (!initialized) {
		intermediate = (velocity * gearRatios[currentGear]) / 100;
		initialized = true;
		SDL_Log("🏁 Physics initialized: velocity=%d, intermediate=%d, gear=%d (target: %.0f fps)", 
		        velocity, intermediate, currentGear + 1, TARGET_FPS);
	}
	
	// Original game logic from assembly (racing.md lines 1106-1111):
	// - Acceleration is added to INTERMEDIATE (constant per frame @ 60fps)
	// - Deceleration is subtracted from VELOCITY (not intermediate!)
	int16_t currentRatio = gearRatios[currentGear];
	
	if (accelerating && !bCrashed) {
		// ADD.W D0,19424(A4) - add acceleration to intermediate
		// Scale by frameScale to maintain consistent timing
		intermediate += static_cast<int16_t>(acceleration * frameScale);
	}
	
	// Calculate velocity from intermediate BEFORE applying deceleration
	// velocity = intermediate / gear_ratio
	velocity = (intermediate * 100) / currentRatio;
	
	// Apply deceleration to VELOCITY (not intermediate) - matches original
	if (!accelerating && !bCrashed) {
		// SUB.W D4,19410(A4) - subtract deceleration from velocity
		// Scale by frameScale for frame-rate independence
		velocity -= static_cast<int16_t>(deceleration * frameScale);
		// Recalculate intermediate to stay in sync
		intermediate = (velocity * currentRatio) / 100;
	}
	
	// Crash recovery - faster deceleration applied to velocity
	if (bCrashed) {
		velocity -= static_cast<int16_t>(deceleration * 2.0f * frameScale);
		intermediate = (velocity * currentRatio) / 100;
	}
	
	// Clamp velocity (RPM)
	if (velocity < MIN_RPM) {
		velocity = MIN_RPM;
		// Also clamp intermediate to match
		intermediate = (velocity * currentRatio) / 100;
	}
	
	int16_t maxRpm = isAutomatic ? MAX_RPM_AUTO : MAX_RPM_MANUAL;
	if (velocity > maxRpm) {
		velocity = maxRpm;
		// Also clamp intermediate to match
		intermediate = (velocity * currentRatio) / 100;
	}
	
	// Update RPM (same as velocity in this game)
	UpdateRPM();
	
	// Check for transmission damage
	UpdateTransmissionDamage();
	
	// Automatic transmission logic
	if (isAutomatic) {
		AutoUpshift();   // Shift up near redline
		AutoDownshift(); // Shift down at low RPM
	} else if (rpm < 2500) {
		// Manual transmission can still auto-downshift at very low RPM (stall prevention)
		AutoDownshift();
	}
	
	// Update display speed (normalized 0-1 for compatibility)
	fSpeed = (velocity - MIN_RPM) / static_cast<float>(MAX_RPM_MANUAL - MIN_RPM);
	if (fSpeed < 0.0f) fSpeed = 0.0f;
	if (fSpeed > 1.0f) fSpeed = 1.0f;
}

void Racing::ShiftGear(int direction) {
	if (isAutomatic) return; // Can't manually shift automatic
	
	int16_t newGear = currentGear + direction;
	
	// Clamp to valid gear range
	if (newGear < 0) newGear = 0;
	if (newGear > maxGear) newGear = maxGear;
	
	if (newGear != currentGear) {
		int16_t oldGear = currentGear;
		int16_t oldRPM = rpm;
		int16_t oldRatio = gearRatios[currentGear];
		int16_t newRatio = gearRatios[newGear];
		
		// When shifting, keep intermediate constant and recalculate velocity
		// This simulates the car maintaining its road speed while engine RPM changes
		// From real physics: RPM = wheel_speed * gear_ratio
		// When gear ratio decreases (shift UP), RPM decreases
		// new_velocity = old_velocity * (new_ratio / old_ratio)
		velocity = (velocity * newRatio) / oldRatio;
		
		// Recalculate intermediate for new gear
		intermediate = (velocity * newRatio) / 100;
		
		currentGear = newGear;
		UpdateRPM();
		
		SDL_Log("⚙️  SHIFT: Gear %d→%d (ratio %.2f→%.2f) RPM: %d→%d", 
		        oldGear + 1, currentGear + 1,
		        oldRatio / 100.0f, newRatio / 100.0f,
		        oldRPM, rpm);
	}
}

void Racing::UpdateRPM() {
	// In Street Rod, velocity IS the engine RPM
	// The gear ratios affect how fast the car moves, not the displayed RPM
	// RPM is simply the current velocity value
	rpm = velocity;
	
	// Clamp RPM to reasonable range
	if (rpm < 1500) rpm = 1500;
	if (rpm > 7000) rpm = 7000;
}

void Racing::UpdateTransmissionDamage() {
	const int16_t REDLINE = 5700;
	const int16_t DANGER_ZONE = 6000;    // Damage starts here, not at redline
	const int16_t DAMAGE_RPM_THRESH = 6200;
	
	if (velocity >= DANGER_ZONE) {
		// Damage increases faster at higher RPM
		int16_t damageRate = 1;
		if (velocity >= DAMAGE_RPM_THRESH) {
			damageRate = (velocity / 1000) - 4; // More damage at very high RPM
		}
		transmissionDamage += damageRate;
		
		// Calculate damage threshold based on current RPM
		// More forgiving - need to stay over-revved longer to blow transmission
		int16_t damageThreshold = (velocity / 100) + 50;
		
		if (transmissionDamage > damageThreshold) {
			// TRANSMISSION BLOWN - RACE OVER
			bCrashed = true;
			fCrashRecoveryTime = 5.0f; // Longer recovery for transmission failure
			velocity = 0;
			fSpeed = 0.0f;
			SDL_Log("💥 TRANSMISSION FAILURE! Over-revved at %d RPM (damage: %d/%d)", 
			        rpm, transmissionDamage, damageThreshold);
		} else if (transmissionDamage > damageThreshold / 2) {
			// Warning - getting close to failure
			static int warningCount = 0;
			if (warningCount % 60 == 0) {
				SDL_Log("⚠️  WARNING: Transmission damage at %d/%d (RPM: %d)", 
				       transmissionDamage, damageThreshold, rpm);
			}
			warningCount++;
		}
	} else {
		// Reset damage when below danger zone
		transmissionDamage = 0;
	}
}

void Racing::AutoUpshift() {
	// For automatic transmission: shift up before hitting redline
	const int16_t AUTO_UPSHIFT_RPM = 5400;  // Shift at ~5400 RPM (300 below redline)
	
	// Only upshift if we have more gears available
	if (rpm >= AUTO_UPSHIFT_RPM && currentGear < maxGear) {
		int16_t oldGear = currentGear;
		int16_t oldRPM = rpm;
		int16_t oldRatio = gearRatios[currentGear];
		int16_t newRatio = gearRatios[currentGear + 1];
		
		// Apply gear shift logic
		velocity = (velocity * newRatio) / oldRatio;
		intermediate = (velocity * newRatio) / 100;
		currentGear++;
		UpdateRPM();
		
		SDL_Log("⚙️  AUTO UPSHIFT: Gear %d→%d (ratio %.2f→%.2f) RPM: %d→%d", 
		        oldGear + 1, currentGear + 1,
		        oldRatio / 100.0f, newRatio / 100.0f,
		        oldRPM, rpm);
	}
}

void Racing::AutoDownshift() {
	const int16_t LOW_RPM_THRESH = 2000;
	
	// Auto downshift when RPM is too low for current gear
	if (rpm < LOW_RPM_THRESH && currentGear > 0) {
		int16_t oldGear = currentGear;
		int16_t oldRPM = rpm;
		int16_t oldRatio = gearRatios[currentGear];
		int16_t newRatio = gearRatios[currentGear - 1];
		
		// Apply gear shift logic
		velocity = (velocity * newRatio) / oldRatio;
		intermediate = (velocity * newRatio) / 100;
		currentGear--;
		UpdateRPM();
		
		SDL_Log("⚙️  AUTO DOWNSHIFT: Gear %d→%d (ratio %.2f→%.2f) RPM: %d→%d", 
		        oldGear + 1, currentGear + 1,
		        oldRatio / 100.0f, newRatio / 100.0f,
		        oldRPM, rpm);
	}
}

void Racing::DrawGaugeNeedle(SDL_Renderer* renderer, int centerX, int centerY, 
                              float value, float minValue, float maxValue) {
	if (!renderer) return;
	
	const int NEEDLE_RADIUS = 14;
	const int CENTERPOINT_RADIUS = 5; // Clear space for centerpoint decoration
	
	// Angle sweep: clockwise from 7 o'clock (240°) to 5 o'clock (300°) = 240° sweep
	const float START_ANGLE = 240.0f * M_PI / 180.0f;  // 7 o'clock position
	const float SWEEP_ANGLE = -240.0f * M_PI / 180.0f; // 240° clockwise
	
	// Calculate needle angle based on value
	float t = (value - minValue) / (maxValue - minValue);
	t = std::max(0.0f, std::min(1.0f, t));
	float angle = START_ANGLE + SWEEP_ANGLE * t;
	
	// Calculate needle start position (outside centerpoint) and end position
	int needleStartX = centerX + static_cast<int>(CENTERPOINT_RADIUS * cos(angle));
	int needleStartY = centerY - static_cast<int>(CENTERPOINT_RADIUS * sin(angle));
	int needleEndX = centerX + static_cast<int>(NEEDLE_RADIUS * cos(angle));
	int needleEndY = centerY - static_cast<int>(NEEDLE_RADIUS * sin(angle));
	
	// Draw the needle (brown/orange color #604000)
	SDL_SetRenderDrawColor(renderer, 96, 64, 0, 255);
	SDL_RenderDrawLine(renderer, needleStartX, needleStartY, needleEndX, needleEndY);
}

void Racing::DrawTachometerNeedleToRenderer(SDL_Renderer* renderer) {
	// Tachometer: center at (145, 129), range 1500-7000 RPM
	DrawGaugeNeedle(renderer, 145, 129, static_cast<float>(rpm), 1500.0f, 7000.0f);
}

void Racing::DrawSpeedometerNeedleToRenderer(SDL_Renderer* renderer) {
	// Speedometer: center at (196, 129), range 0-140 MPH
	// Calculate MPH from RPM and gear ratio
	int16_t currentRatio = gearRatios[currentGear];
	float gearMultiplier = 100.0f / currentRatio;
	float speedMph = (velocity / 70.0f) * gearMultiplier;
	
	DrawGaugeNeedle(renderer, 196, 129, speedMph, 0.0f, 140.0f);
}

SDL_Surface* Racing::GetSurface() {
	return pixels;
}

// Overhead Sign Rendering Implementation
// Based on reverse engineering of Street Rod Amiga assembly code
//
// KEY DISCOVERY: Overhead signs are ARCH STRUCTURES that the player drives UNDER!
// They consist of vertical posts on road edges + horizontal bar + sign bitmap on bar.
//
// Original Amiga Implementation:
// - Track data contains flag $0005 for special segments (overhead arches)
// - Dispatcher routes to handlers based on segment index
// - Handlers render multiple trapezoids:
//   1. Left vertical post (trapezoid at left road edge)
//   2. Right vertical post (trapezoid at right road edge)
//   3. Horizontal bar connecting posts at top
//   4. Sign bitmap rendered ON the horizontal bar
// - Uses same polygon renderer (SECSTRT_26 → LAB_0296 → Amiga Blitter)
//
// Overhead Arch Signs (verified in-game):
//   ID 14: COUNTRY LINE (last arch = finish line)
//   ID 33: ROUTE 69
//   ID 34: CITY LIMITS
//   ID 35: EXIT 48
//
// Arch Structure:
//   [SIGN TEXT]
//   ============  <- horizontal bar (trapezoid)
//   |          |  <- vertical posts (trapezoids)
//   |   ROAD   |
//   |          |
//  YOU DRIVE UNDER THIS!

void Racing::DrawOverheadSigns(float currentDistance) {
    // NOTE: We need track segment data with flags to detect $0005 (overhead arch) segments
    // For now, hardcode test positions based on typical track layout
    // TODO: Parse track flags from binary data to detect actual overhead sign positions
    
    // Estimated overhead sign positions (distance along track)
    // These would normally come from track data with flag $0005
    struct OverheadSign {
        float position;
        uint16_t signId;
    };
    
    // Example positions - replace with actual track data parsing
    static const OverheadSign signs[] = {
        {150.0f, 33},  // ROUTE 69
        {350.0f, 34},  // CITY LIMITS
        {550.0f, 35},  // EXIT 48
        {fTrackDistance * 0.95f, 14}, // COUNTRY LINE near finish
    };
    
    const float VISIBLE_DISTANCE = 300.0f;
    
    for (const auto& sign : signs) {
        float distance = sign.position - currentDistance;
        
        // Handle wraparound for lap completion
        if (distance < -fTrackDistance / 2) {
            distance += fTrackDistance;
        }
        
        // Only draw if sign is ahead and visible
        if (distance > 0 && distance <= VISIBLE_DISTANCE) {
            DrawOverheadSignTrapezoid(distance, sign.signId);
        }
    }
}

void Racing::DrawOverheadSignTrapezoid(float distance, uint16_t signId) {
    // Get the appropriate sign sprite
    Bitmap* signBitmap = nullptr;
    const char* signName = "Unknown";
    
    switch (signId) {
        case 14: // COUNTRY LINE (finish)
            signBitmap = signCountryLineSprite.get();
            signName = "COUNTRY LINE";
            break;
        case 33: // ROUTE 69
            signBitmap = signRoute69Sprite.get();
            signName = "ROUTE 69";
            break;
        case 34: // CITY LIMITS
            signBitmap = signCityLimitsSprite.get();
            signName = "CITY LIMITS";
            break;
        case 35: // EXIT 48
            signBitmap = signExit48Sprite.get();
            signName = "EXIT 48";
            break;
        default:
            return;
    }

    // Until all frames added
    signBitmap = signCountryLineSprite.get();
    
    if (!signBitmap || !signBitmap->GetSurface()) {
        return; // Sign sprite not loaded
    }
    
    // Calculate perspective for this distance
    // Allow arches to be visible from further away
    float perspective = 1.0f - (distance / 200.0f);
    if (perspective <= 0 || perspective > 1.0f) return;
    
    // Original Amiga approach: Draw an ARCH structure with posts and horizontal bar
    // Structure:
    //   [SIGN TEXT]
    //   ============  <- horizontal bar
    //   |          |  <- vertical posts on each side
    //   |   ROAD   |
    
    int horizonY = GAME_RACING_HEIGHT / 2;
    int roadBottomY = horizonY + (int)(perspective * (GAME_RACING_HEIGHT / 2));
    
    // Calculate road width at this perspective
    float fRoadWidth = 0.21f + perspective * 0.98f;
    fRoadWidth *= 0.8f;
    
    // Middle point with curvature effect (same as road)
    float fMiddlePoint = 0.5f + fCurvature * 0.4f * powf((1.0f - perspective), 2.5f);
    fMiddlePoint -= fCarPos * 0.25f; // Shift with car position
    
    // Calculate post positions at road edges
    int roadCenterX = (int)(fMiddlePoint * GAME_WIDTH);
    int roadWidthPixels = (int)(fRoadWidth * GAME_WIDTH);
    int leftPostX = roadCenterX - roadWidthPixels;
    int rightPostX = roadCenterX + roadWidthPixels;
    
    // Post dimensions scale with perspective
    // Posts need to be TALL arches you drive under!
    int postWidth = (int)(8.0f * perspective + 3.0f); // 3-11 pixels wide
    int postHeight = (int)(120.0f * perspective + 30.0f); // 30-150 pixels tall (MUCH taller!)
    int barHeight = (int)(12.0f * perspective + 4.0f); // 4-16 pixels tall
    
    // Minimum visible size - allow tiny arches in distance
    if (postHeight < 3) return;
    
    // Position arch above road
    int archTopY = roadBottomY - postHeight;
    int barTopY = archTopY;
    int barBottomY = barTopY + barHeight;
    
    SDL_Color archColor = {255, 0, 0, 255}; // RED for arch structure
    SDL_Color yellowGroundColor = {255, 255, 0, 255}; // YELLOW ground marking
    
    // NOTE: Surface is already locked by caller
    
    // Draw YELLOW GROUND SURFACE under the arch (road marking)
    // This is a warning zone painted on the road BEFORE the arch
    // Zone ENDS at the arch position (where you pass under it)
    // TODO: Extract exact zone start/end distances from track data flag $0005
    
    // Yellow zone ends where arch posts are (at roadBottomY perspective)
    // and extends backwards on the road surface
    int zoneEndY = roadBottomY;  // End AT the arch
    int zoneStartY = zoneEndY - (int)(20.0f * perspective + 10.0f); // Shorter zone: 10-30px
    
    for (int y = zoneStartY; y <= zoneEndY && y >= horizonY && y < GAME_RACING_HEIGHT; y++) {
        // Calculate road width at THIS scanline's perspective
        float linePerspective = (float)(y - horizonY) / (float)(GAME_RACING_HEIGHT / 2);
        float lineRoadWidth = (0.21f + linePerspective * 0.98f) * 0.8f;
        float lineMiddlePoint = 0.5f + fCurvature * 0.4f * powf((1.0f - linePerspective), 2.5f);
        lineMiddlePoint -= fCarPos * 0.25f;
        
        int lineCenterX = (int)(lineMiddlePoint * GAME_WIDTH);
        int lineWidthPixels = (int)(lineRoadWidth * GAME_WIDTH);
        int lineLeftX = lineCenterX - lineWidthPixels;
        int lineRightX = lineCenterX + lineWidthPixels;
        
        // Draw yellow marking across road width
        for (int x = lineLeftX; x <= lineRightX && x >= 0 && x < GAME_WIDTH; x++) {
            PutPixel(pixels, x, y, yellowGroundColor);
        }
    }
    
    // Draw LEFT POST (vertical trapezoid)
    for (int y = archTopY; y < roadBottomY && y < GAME_RACING_HEIGHT; y++) {
        // Draw posts even ABOVE horizon (into sky area) for tall arches!
        if (y >= 0) {  // Only check screen bounds, not horizon
            // Slight perspective taper
            float postPerspective = (float)(y - archTopY) / (float)postHeight;
            int width = (int)(postWidth * (1.0f + postPerspective * 0.3f));
            for (int x = leftPostX; x < leftPostX + width && x >= 0 && x < GAME_WIDTH; x++) {
                PutPixel(pixels, x, y, archColor);
            }
        }
    }
    
    // Draw RIGHT POST (vertical trapezoid)
    for (int y = archTopY; y < roadBottomY && y < GAME_RACING_HEIGHT; y++) {
        // Draw posts even ABOVE horizon (into sky area) for tall arches!
        if (y >= 0) {  // Only check screen bounds, not horizon
            float postPerspective = (float)(y - archTopY) / (float)postHeight;
            int width = (int)(postWidth * (1.0f + postPerspective * 0.3f));
            for (int x = rightPostX - width; x < rightPostX && x >= 0 && x < GAME_WIDTH; x++) {
                PutPixel(pixels, x, y, archColor);
            }
        }
    }
    
    // Draw HORIZONTAL BAR connecting the posts
    for (int y = barTopY; y < barBottomY && y >= 0 && y < GAME_RACING_HEIGHT; y++) {
        for (int x = leftPostX; x <= rightPostX && x >= 0 && x < GAME_WIDTH; x++) {
            PutPixel(pixels, x, y, archColor);
        }
    }
    
    // Debug: Log arch rendering details
    static int lastLoggedSign = -1;
    static int frameCount = 0;
    frameCount++;
    // if ((signId != lastLoggedSign && distance < 100.0f) || frameCount % 60 == 0) {
    //     printf("🏛️  RED ARCH: %s at distance %.1f (perspective %.3f)\n", signName, distance, perspective);
    //     printf("    Posts: Left=%d Right=%d, width=%dpx, HEIGHT=%dpx 🔺\n", 
    //            leftPostX, rightPostX, postWidth, postHeight);
    //     printf("    Arch spans: y=%d (top) to y=%d (road), total %dpx tall\n", 
    //            archTopY, roadBottomY, roadBottomY - archTopY);
    //     printf("    Bar: y=%d-%d (height=%dpx)\n", barTopY, barBottomY, barHeight);
    //     printf("    Yellow zone: y=%d to y=%d (%dpx)\n", 
    //            zoneStartY, zoneEndY, zoneEndY - zoneStartY);
    //     printf("    Horizon: y=%d, RoadBottom/Arch: y=%d\n", horizonY, roadBottomY);
    //     lastLoggedSign = signId;
    // }
    
    // NOTE: Surface will be unlocked by caller, then sign bitmap will be drawn separately
}

void Racing::DrawOverheadSignBitmaps(float currentDistance) {
    // Draw sign bitmaps using frame-based scaling (like trees)
    // This must match the arch positions calculated in DrawOverheadSigns
    
    struct OverheadSign {
        float position;
        uint16_t signId;
    };
    
    // Same positions as in DrawOverheadSigns
    static const OverheadSign signs[] = {
        {150.0f, 33},  // ROUTE 69
        {350.0f, 34},  // CITY LIMITS
        {550.0f, 35},  // EXIT 48
        {fTrackDistance * 0.95f, 14}, // COUNTY LINE near finish
    };
    
    const float VISIBLE_DISTANCE = 300.0f;
    
    for (const auto& sign : signs) {
        float distance = sign.position - currentDistance;
        
        // Handle wraparound for lap completion
        if (distance < -fTrackDistance / 2) {
            distance += fTrackDistance;
        }
        
        // Only draw if sign is ahead and visible
        if (distance > 0 && distance <= VISIBLE_DISTANCE) {
            // TODO: Use different sprites when frames are added for all signs
            // For now, always use COUNTY LINE sprite with frames
            Bitmap* signBitmap = signCountryLineSprite.get();
            const SpriteAnimationInfo* animInfo = GetSpriteAnimationInfo(RACING_SIGN_COUNTY_LINE);
            
            if (!signBitmap || !signBitmap->GetSurface()) {
                continue;
            }
            
            // Calculate same perspective and positions as DrawOverheadSignTrapezoid
            float perspective = 1.0f - (distance / 200.0f);
            if (perspective <= 0 || perspective > 1.0f) continue;
            
            int horizonY = GAME_RACING_HEIGHT / 2;
            int roadBottomY = horizonY + (int)(perspective * (GAME_RACING_HEIGHT / 2));
            
            float fRoadWidth = 0.21f + perspective * 0.98f;
            fRoadWidth *= 0.8f;
            
            float fMiddlePoint = 0.5f + fCurvature * 0.4f * powf((1.0f - perspective), 2.5f);
            fMiddlePoint -= fCarPos * 0.25f;
            
            int roadCenterX = (int)(fMiddlePoint * GAME_WIDTH);
            int postHeight = (int)(120.0f * perspective + 30.0f);
            int barHeight = (int)(12.0f * perspective + 4.0f);
            int archTopY = roadBottomY - postHeight;
            int barTopY = archTopY;
            
            // Use frame-based rendering if animation info exists
            SDL_Rect srcRect = {0, 0, 0, 0};
            
            if (animInfo && animInfo->frameCount > 1) {
                // Select frame based on distance (11 frames, far to near)
                // Increased distances so larger frames appear earlier
                // Frame 0 = smallest (far away, distance >= 200)
                // Frame 10 = largest (very close, distance < 60)
                static const float frameDistances[] = {
                    200.0f, 180.0f, 160.0f, 140.0f, 120.0f, 100.0f, 85.0f, 70.0f, 60.0f, 50.0f, 0.0f
                };
                
                uint8_t frame = 0;
                for (int i = 0; i < animInfo->frameCount; i++) {
                    if (distance >= frameDistances[i]) {
                        frame = i;
                        break; // Found the first threshold we exceed
                    }
                }
                
                SpriteRect frameRect = GetAnimationFrameRect(animInfo, frame);
                srcRect = {
                    static_cast<int>(frameRect.x),
                    static_cast<int>(frameRect.y),
                    static_cast<int>(frameRect.w),
                    static_cast<int>(frameRect.h)
                };
            } else {
                // No animation, use full sprite
                SDL_Surface* surface = signBitmap->GetSurface();
                srcRect = {0, 0, surface->w, surface->h};
            }
            
            // Position sign centered on horizontal bar
            int signX = roadCenterX - srcRect.w / 2;
            int signY = barTopY + (barHeight - srcRect.h) / 2;
            
            SDL_Rect dstRect = {signX, signY, srcRect.w, srcRect.h};
            
            // Clip to screen bounds
            SDL_Rect originalSrcRect = srcRect;
            if (dstRect.x < 0) {
                int leftClip = -dstRect.x;
                srcRect.x = originalSrcRect.x + leftClip;
                srcRect.w = originalSrcRect.w - leftClip;
                dstRect.w += dstRect.x;
                dstRect.x = 0;
            }
            if (dstRect.x + dstRect.w > GAME_WIDTH) {
                int overflow = (dstRect.x + dstRect.w) - GAME_WIDTH;
                srcRect.w = originalSrcRect.w - overflow;
                dstRect.w -= overflow;
            }
            if (dstRect.y < 0) {
                int topClip = -dstRect.y;
                srcRect.y = originalSrcRect.y + topClip;
                srcRect.h = originalSrcRect.h - topClip;
                dstRect.h += dstRect.y;
                dstRect.y = 0;
            }
            if (dstRect.y + dstRect.h > GAME_RACING_HEIGHT) {
                int overflow = (dstRect.y + dstRect.h) - GAME_RACING_HEIGHT;
                srcRect.h = originalSrcRect.h - overflow;
                dstRect.h -= overflow;
            }
            
            if (srcRect.w > 0 && srcRect.h > 0 && dstRect.w > 0 && dstRect.h > 0) {                
                SDL_BlitSurface(signBitmap->GetSurface(), &srcRect, pixels, &dstRect);
            }
        }
    }
}
