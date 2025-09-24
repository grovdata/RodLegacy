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

	// Car Curvature is accumulated left/right input, but inversely proportional to speed
	// i.e. it is harder to turn at high speed

	bool goLeft = false;
	bool goRight = false;
	bool accelerate = false;

	// Check current keyboard state for sustained input (like holding keys)
	const Uint8* keyboardState = SDL_GetKeyboardState(NULL);
	if (keyboardState) {
		goLeft = keyboardState[SDL_SCANCODE_LEFT];
		goRight = keyboardState[SDL_SCANCODE_RIGHT];
		accelerate = keyboardState[SDL_SCANCODE_UP];
	}

	if (accelerate)
		fSpeed += 2.0f * deltaTime;
	else
		fSpeed -= 1.0f * deltaTime;

	if (goLeft)
	{
		fPlayerCurvature -= 0.7f * deltaTime * (1.0f - fSpeed / 2.0f);
		nCarDirection = -1;
	}

	if (goRight)
	{
		fPlayerCurvature += 0.7f * deltaTime * (1.0f - fSpeed / 2.0f);
		nCarDirection = +1;
	}

	// If car curvature is too different to track curvature, slow down
	// as car has gone off track
	if (fabs(fPlayerCurvature - fTrackCurvature) >= 0.8f)
		fSpeed -= 5.0f * deltaTime;

	// Clamp Speed
	if (fSpeed < 0.0f)	fSpeed = 0.0f;
	if (fSpeed > 1.0f)	fSpeed = 1.0f;

	// Move car along track according to car speed
	fDistance += (70.0f * fSpeed) * deltaTime;
	
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
	float fTrackCurveDiff = (fTargetCurvature - fCurvature) * deltaTime * 0.5f; // Reduced interpolation speed

	// Accumulate player curvature
	fCurvature += fTrackCurveDiff;

	// Accumulate track curvature more gradually
	fTrackCurvature += (fCurvature) * deltaTime * fSpeed * 0.7f;

	// Draw sky
	for (int y = 0; y < GAME_RACING_HEIGHT / 2; y++)
		for (int x = 0; x < GAME_WIDTH; x++)
			PutPixel(pixels, x, y, black);

	// Draw Scenery - our hills are a rectified sine wave, where the phase is adjusted by the
	// accumulated track curvature
	for (int x = 0; x < GAME_WIDTH; x++)
	{
		int nHillHeight = (int)(fabs(sinf(x * 0.01f + fTrackCurvature) * 16.0f));
//			for (int y = (GAME_RACING_HEIGHT / 2) - nHillHeight; y < GAME_RACING_HEIGHT / 2; y++)
//				PutPixel(pixels, x, y, yellow);
	}

	if (SDL_LockSurface(pixels) != 0) {
		// Handle error: Unable to lock surface
		SDL_Log("Unable to lock surface: %s", SDL_GetError());
		return;
	}

	// Debug logging disabled to improve performance
	// SDL_Log("ACC: %d L: %d R: %d T: %.1f D: %f S: %d\n", accelerate, goLeft, goRight, fElapsedTime, fDistance, nTrackSection);

	// Draw Track - Each row is split into grass, clip-board and track
	for (int y = 0; y < GAME_RACING_HEIGHT / 2; y++) {
		for (int x = 0; x < GAME_WIDTH; x++) {

			// Perspective is used to modify the width of the track row segments
			float fPerspective = (float)y / (GAME_RACING_HEIGHT/2.0f);
			float fRoadWidth = 0.1f + fPerspective * 0.8f; // Min 10% Max 90%
			float fClipWidth = fRoadWidth * 0.15f;
			float fMidLineWidth = fRoadWidth * 0.03;
			fRoadWidth *= 0.5f;	// Halve it as track is symmetrical around center of track, but offset...

			// ...depending on where the middle point is, which is defined by the current
			// track curvature. Balanced curvature effect.
			float fMiddlePoint = 0.5f + fCurvature * 0.4f * powf((1.0f - fPerspective), 2.5f);

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

			// Road color depends on track type - make differences obvious
			SDL_Color nRoadColour;
			if (currentTrackType == SR_TRACK_ROAD_RACE) {
				nRoadColour = grey;  // Road race = grey road
			} else if (currentTrackType == SR_TRACK_DRAG_RACE) {
				nRoadColour = red;   // Drag race = red road  
			} else {
				nRoadColour = black; // Fallback = black road
			}

			// Draw the row segments
			if (x >= 0 && x < nLeftGrass)
				PutPixel(pixels, x, nRow, nGrassColour);
			if (x >= nLeftGrass && x < nLeftClip)
				PutPixel(pixels, x, nRow, nClipColour);
			if (x >= nLeftClip && x < nRightClip)
				PutPixel(pixels, x, nRow, nRoadColour);
			// Draw dashed center line instead of continuous
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

	// Draw track info overlay (debug) - before unlocking surface
	// Draw background for text
	for (int y = 5; y < 25; y++) {
		for (int x = 5; x < 200; x++) {
			PutPixel(pixels, x, y, black);
		}
	}
	
	// Draw simple text indicator
	if (currentTrackType == SR_TRACK_ROAD_RACE) {
		// Draw "ROAD" indicator (green bars)
		for (int x = 10; x < 50; x++) {
			PutPixel(pixels, x, 10, green);
			PutPixel(pixels, x, 15, green);
		}
	} else {
		// Draw "DRAG" indicator (red bars)
		for (int x = 10; x < 50; x++) {
			PutPixel(pixels, x, 10, red);
			PutPixel(pixels, x, 15, red);
		}
	}
	
	// Show track segment count as yellow dots
	int segmentCount = vecTrack.size();
	for (int i = 0; i < segmentCount && i < 100; i++) {
		PutPixel(pixels, 60 + i, 10, yellow);
	}
	
	SDL_UnlockSurface(pixels);
	
	// Draw sprite bitmaps after unlocking the surface
	DrawRoadsideSprites(fDistance);

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
        float scaledCurvature = curvature * 0.5f;  // Increased from 0.1f
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
                printf("🏁 PASSED: %s at position %.1f (distance: %.1f)\n", 
                       TrackReader::GetSpriteName(sprite.spriteId1), spritePos1, currentDistance);
            }
        }
        
        // Check if we're close to sprite 2
        if (sprite.spriteId2 != 0 && fabs(currentDistance - spritePos2) <= ENCOUNTER_DISTANCE) {
            int spriteKey = (i * 2) + 1; // Unique key for sprite 2 in this record
            if (passedSprites.find(spriteKey) == passedSprites.end()) {
                passedSprites.insert(spriteKey);
                printf("🏁 PASSED: %s at position %.1f (distance: %.1f)\n", 
                       TrackReader::GetSpriteName(sprite.spriteId2), spritePos2, currentDistance);
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
            }
        }
        
        // Draw sprite 2 if visible
        if (sprite.spriteId2 != 0) {
            float distance = spritePos2 - currentDistance;
            if (distance >= -50.0f && distance <= VISIBLE_DISTANCE) {
                DrawSpriteIndicator(distance, sprite.spriteId2, false); // false = right side
            }
        }
    }
}

void Racing::DrawSpriteIndicator(float distance, uint16_t spriteId, bool leftSide) {
    // Convert distance to screen Y position (perspective)
    if (distance < 0) return; // Don't draw sprites behind us
    
    float perspective = 1.0f - (distance / 200.0f); // Normalize to 0-1 range
    if (perspective <= 0) return;
    
    int screenY = static_cast<int>((GAME_RACING_HEIGHT / 2) + perspective * (GAME_RACING_HEIGHT / 2));
    if (screenY >= GAME_RACING_HEIGHT) return;
    
    // Calculate road width at this perspective
    float fRoadWidth = 0.1f + perspective * 0.8f;
    fRoadWidth *= 0.5f;
    
    // Middle point with curvature effect
    float fMiddlePoint = 0.5f + fCurvature * 0.4f * powf((1.0f - perspective), 2.5f);
    
    // Calculate sprite position (left or right side of road)
    int spriteX;
    if (leftSide) {
        spriteX = static_cast<int>((fMiddlePoint - fRoadWidth - 0.1f) * GAME_WIDTH);
    } else {
        spriteX = static_cast<int>((fMiddlePoint + fRoadWidth + 0.1f) * GAME_WIDTH);
    }
    
    // Choose the appropriate sprite bitmap
    Bitmap* spriteBitmap = nullptr;
    switch (spriteId) {
        case 30: // Telephone Pole
            spriteBitmap = telephonePoleSprite.get();
            break;
        case 33: // Road Posts
        case 22: // Road Posts (alt)
            spriteBitmap = roadPostSprite.get();
            break;
        case 31: // Tree 31
        case 32: // Tree 32
            spriteBitmap = treeSprite.get();
            break;
        case 25: // Bush
            spriteBitmap = bushSprite.get();
            break;
        default:
            // Use road posts as default for unknown sprites instead of colored sticks
            spriteBitmap = roadPostSprite.get();
            break;
    }
    
    if (spriteBitmap && spriteBitmap->GetSurface()) {
        SDL_Surface* spriteSurface = spriteBitmap->GetSurface();
        // Bitmap sprite rendering active
        
        // Calculate scaled sprite dimensions based on perspective
        float scale = perspective * 0.5f + 0.2f; // Scale from 0.2 to 0.7
        int scaledWidth = static_cast<int>(spriteSurface->w * scale);
        int scaledHeight = static_cast<int>(spriteSurface->h * scale);
        
        // Center the sprite horizontally and place it on the ground
        int drawX = spriteX - scaledWidth / 2;
        int drawY = screenY - scaledHeight;
        
        // Draw the scaled sprite bitmap
        SDL_Rect srcRect = {0, 0, spriteSurface->w, spriteSurface->h};
        SDL_Rect dstRect = {drawX, drawY, scaledWidth, scaledHeight};
        
        // Clamp destination rectangle to screen bounds
        if (dstRect.x < 0) {
            srcRect.x = -dstRect.x;
            srcRect.w += dstRect.x;
            dstRect.w += dstRect.x;
            dstRect.x = 0;
        }
        if (dstRect.x + dstRect.w > GAME_WIDTH) {
            int overflow = (dstRect.x + dstRect.w) - GAME_WIDTH;
            srcRect.w -= overflow;
            dstRect.w -= overflow;
        }
        if (dstRect.y < 0) {
            srcRect.y = -dstRect.y;
            srcRect.h += dstRect.y;
            dstRect.h += dstRect.y;
            dstRect.y = 0;
        }
        if (dstRect.y + dstRect.h > GAME_RACING_HEIGHT) {
            int overflow = (dstRect.y + dstRect.h) - GAME_RACING_HEIGHT;
            srcRect.h -= overflow;
            dstRect.h -= overflow;
        }
        
        // Only draw if there's something visible
        if (srcRect.w > 0 && srcRect.h > 0 && dstRect.w > 0 && dstRect.h > 0) {
            int result = SDL_BlitScaled(spriteBitmap->GetSurface(), &srcRect, pixels, &dstRect);
            if (result != 0) {
                printf("SDL_BlitScaled failed: %s\n", SDL_GetError());
            }
        }
    } else {
        // Fallback to simple colored indicator for unknown sprites or if bitmap failed to load
        // Using fallback rendering for unmapped sprites
        
        SDL_Color spriteColor = yellow; // Unknown sprites
        int spriteHeight = static_cast<int>(perspective * 20.0f + 3.0f); // Scale with distance
        for (int y = 0; y < spriteHeight && (screenY - y) >= (GAME_RACING_HEIGHT / 2); y++) {
            if (spriteX >= 0 && spriteX < GAME_WIDTH) {
                PutPixel(pixels, spriteX, screenY - y, spriteColor);
                // Make wider for closer sprites
                if (perspective > 0.5f && spriteX + 1 < GAME_WIDTH) {
                    PutPixel(pixels, spriteX + 1, screenY - y, spriteColor);
                }
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
    
    // Load tree sprite (asset ID 31)
    auto* treeAsset = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_TREE1});
    if (treeAsset) {
        treeSprite = std::make_unique<Bitmap>(*treeAsset);
        if (treeSprite->GetSurface()) {
            printf("Racing: Loaded tree sprite (%dx%d)\n", 
                   treeSprite->GetSurface()->w, treeSprite->GetSurface()->h);
        }
    } else {
        printf("Racing: Failed to load tree sprite\n");
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
}

SDL_Surface* Racing::GetSurface() {
	return pixels;
}

