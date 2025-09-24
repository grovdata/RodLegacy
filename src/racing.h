#pragma once  // Add header guard

/*
https://raw.githubusercontent.com/OneLoneCoder/Javidx9/master/ConsoleGameEngine/SmallerProjects/OneLoneCoder_RetroArcadeRacer.cpp
*/

#include <iostream>
#include <string>
#include <list>
#include <SDL2/SDL.h>
#include <vector>
#include "scene.h"

class Racing {

private:

	SDL_Color black = { 0, 0, 0, 255 };
	SDL_Color yellow = { 255, 238, 0, 255 };
	SDL_Color white = { 255, 255, 255, 255 };
	SDL_Color red = { 170, 0, 0, 255 };
	SDL_Color green = { 0, 119, 0, 255 };
	SDL_Color darkGreen = { 0, 119, 0, 255 };
	SDL_Color grey = { 200, 200, 200, 255 };


	SDL_Surface* pixels;

	float fDistance = 0.0f;			// Distance car has travelled around track
	float fCurvature = 0.0f;		// Current track curvature, lerped between track sections
	float fTrackCurvature = 0.0f;	// Accumulation of track curvature
	float fTrackDistance = 0.0f;	// Total distance of track

	float fCarPos = 0.0f;			// Current car position
	float fPlayerCurvature = 0.0f;			// Accumulation of player curvature
	float fSpeed = 0.0f;			// Current player speed

	// Timing management
	Uint64 lastUpdateTime = 0;		// Last update time in milliseconds
	bool firstUpdate = true;		// Flag to handle first update

	std::vector<std::pair<float, float>> vecTrack; // Track sections, sharpness of bend, length of section

	std::list<float> listLapTimes;		// List of previous lap times
	float fCurrentLapTime;			// Current lap time

	// Move to generic
	void PutPixel(SDL_Surface* s, int x, int y, SDL_Color c);

public:

    Racing();
	void Draw(float fElapsedTime, SDL_Event *event);
    SDL_Surface* GetSurface();
};
