/*
https://raw.githubusercontent.com/OneLoneCoder/Javidx9/master/ConsoleGameEngine/SmallerProjects/OneLoneCoder_RetroArcadeRacer.cpp
*/
#include "global.h"
#include "racing.h"

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
		//pixels = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBX8888, SDL_TEXTUREACCESS_STREAMING, GAME_WIDTH, GAME_RACING_HEIGHT);
		// Define track
		vecTrack.push_back(make_pair(0.0f, 10.0f));		// Short section for start/finish line
		vecTrack.push_back(make_pair(0.0f, 200.0f));
		vecTrack.push_back(make_pair(1.0f, 200.0f));
		vecTrack.push_back(make_pair(0.0f, 400.0f));
		vecTrack.push_back(make_pair(-1.0f, 100.0f));
		vecTrack.push_back(make_pair(0.0f, 200.0f));
		vecTrack.push_back(make_pair(-1.0f, 200.0f));
		vecTrack.push_back(make_pair(1.0f, 200.0f));
		vecTrack.push_back(make_pair(0.0f, 200.0f));
		vecTrack.push_back(make_pair(0.2f, 500.0f));
		vecTrack.push_back(make_pair(0.0f, 200.0f));

		// Calculate total track distance, so we can set lap times
		for (auto t : vecTrack)
			fTrackDistance += t.second;

		listLapTimes = { 0,0,0,0,0 };
		fCurrentLapTime = 0.0f;


	}

void Racing::Draw(float fElapsedTime, SDL_Event *event) {
	// Handle control input
	int nCarDirection = 0;

	// Car Curvature is accumulated left/right input, but inversely proportional to speed
	// i.e. it is harder to turn at high speed


	bool goLeft = false;
	bool goRight = false;
	bool accelerate = true;

	if (event != NULL) {
		goLeft  = event->key.keysym.sym == SDLK_LEFT;
		goRight = event->key.keysym.sym == SDLK_RIGHT;
		accelerate = event->key.keysym.sym == SDLK_UP;
	}

	if (accelerate)
		fSpeed += 2.0f * fElapsedTime;
	else
		fSpeed -= 1.0f * fElapsedTime;

	if (goLeft)
	{
		fPlayerCurvature -= 0.7f * fElapsedTime * (1.0f - fSpeed / 2.0f);
		nCarDirection = -1;
	}

	if (goRight)
	{
		fPlayerCurvature += 0.7f * fElapsedTime * (1.0f - fSpeed / 2.0f);
		nCarDirection = +1;
	}

	// If car curvature is too different to track curvature, slow down
	// as car has gone off track
	if (fabs(fPlayerCurvature - fTrackCurvature) >= 0.8f)
		fSpeed -= 5.0f * fElapsedTime;

	// Clamp Speed
	if (fSpeed < 0.0f)	fSpeed = 0.0f;
	if (fSpeed > 1.0f)	fSpeed = 1.0f;

	// Move car along track according to car speed
	fDistance += (70.0f * fSpeed) * fElapsedTime;

	// Get Point on track
	float fOffset = 0;
	int nTrackSection = 0;

	// Lap Timing and counting
	fCurrentLapTime += fElapsedTime;
	if (fDistance >= fTrackDistance)
	{
		fDistance -= fTrackDistance;
		listLapTimes.push_front(fCurrentLapTime);
		listLapTimes.pop_back();
		fCurrentLapTime = 0.0f;
	}

	// Find position on track (could optimise)
	while (nTrackSection < vecTrack.size() && fOffset <= fDistance)
	{
		fOffset += vecTrack[nTrackSection].second;
		nTrackSection++;
	}

	// Interpolate towards target track curvature
	float fTargetCurvature = vecTrack[nTrackSection - 1].first;
	float fTrackCurveDiff = (fTargetCurvature - fCurvature) * fElapsedTime * fSpeed;

	// Accumulate player curvature
	fCurvature += fTrackCurveDiff;

	// Accumulate track curvature
	fTrackCurvature += (fCurvature) * fElapsedTime * fSpeed;

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

	SDL_Log("Time: %.2f Distance: %f Section: %d\n", fElapsedTime, fDistance, nTrackSection);

	// Draw Track - Each row is split into grass, clip-board and track
	for (int y = 0; y < GAME_RACING_HEIGHT / 2; y++) {
		for (int x = 0; x < GAME_WIDTH; x++) {

			// Perspective is used to modify the width of the track row segments
			float fPerspective = (float)y / (GAME_RACING_HEIGHT/2.0f);
			float fRoadWidth = 0.1f + fPerspective * 0.8f; // Min 10% Max 90%
			float fClipWidth = fRoadWidth * 0.15f;
			float fMidLineWidth = fRoadWidth * 0.1;
			fRoadWidth *= 0.5f;	// Halve it as track is symmetrical around center of track, but offset...

			// ...depending on where the middle point is, which is defined by the current
			// track curvature.
			float fMiddlePoint = 0.5f + fCurvature * powf((1.0f - fPerspective), 3);

			// Work out segment boundaries
			int nLeftGrass = (fMiddlePoint - fRoadWidth - fClipWidth) * GAME_WIDTH;
			int nLeftClip = (fMiddlePoint - fRoadWidth) * GAME_WIDTH;
			int nRightClip = (fMiddlePoint + fRoadWidth) * GAME_WIDTH;
			int nRightGrass = (fMiddlePoint + fRoadWidth + fClipWidth) * GAME_WIDTH;
			int nLeftMidline = (fMiddlePoint - fMidLineWidth);
			int nRightMidline = (fMiddlePoint + fMidLineWidth);

			int nRow = GAME_RACING_HEIGHT / 2 + y;

			// Using periodic oscillatory functions to give lines, where the phase is controlled
			// by the distance around the track. These take some fine tuning to give the right "feel"
			SDL_Color nGrassColour = sinf(20.0f *  powf(1.0f - fPerspective,3) + fDistance * 0.1f) > 0.0f ? green : darkGreen;
			SDL_Color nClipColour = sinf(80.0f *  powf(1.0f - fPerspective, 2) + fDistance) > 0.0f ? red : white;

			// Start finish straight changes the road colour to inform the player lap is reset
//                SDL_Color nRoadColour = (nTrackSection-1) == 0 ? black : grey;
			SDL_Color nRoadColour = black;

			// Draw the row segments
			if (x >= 0 && x < nLeftGrass)
				PutPixel(pixels, x, nRow, nGrassColour);
			if (x >= nLeftGrass && x < nLeftClip)
				PutPixel(pixels, x, nRow, nClipColour);
			if (x >= nLeftClip && x < nRightClip)
				PutPixel(pixels, x, nRow, nRoadColour);
			if (x >= 0 && x < nRightMidline)
				PutPixel(pixels, x, nRow, yellow);
			if (x >= nRightClip && x < nRightGrass)
				PutPixel(pixels, x, nRow, nClipColour);
			if (x >= nRightGrass && x < GAME_WIDTH)
				PutPixel(pixels, x, nRow, nGrassColour);


		}
	}

	SDL_UnlockSurface(pixels);

	// Draw Car - car position on road is proportional to difference between
	// current accumulated track curvature, and current accumulated player curvature
	// i.e. if they are similar, the car will be in the middle of the track
	// fCarPos = fPlayerCurvature - fTrackCurvature;
	// int nCarPos = GAME_WIDTH / 2 + ((int)(GAME_WIDTH * fCarPos) / 2.0) - 7; // Offset for sprite

	// Draw Stats

	// DrawString(0, 0, L"Distance: " + to_wstring(fDistance));
	// DrawString(0, 1, L"Target Curvature: " + to_wstring(fCurvature));
	// DrawString(0, 2, L"Player Curvature: " + to_wstring(fPlayerCurvature));
	// DrawString(0, 3, L"Player Speed    : " + to_wstring(fSpeed));
	// DrawString(0, 4, L"Track Curvature : " + to_wstring(fTrackCurvature));

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

SDL_Surface* Racing::GetSurface() {
	return pixels;
}

