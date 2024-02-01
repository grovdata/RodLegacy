#pragma once

#include "global.h"

class Spritesheet;
extern SDL_Renderer *renderer;
enum CarVendor { CHRYSLER, FORD, GM };

struct CarModel {
    CarVendor vendor;
    std::string name;
    std::string filename;
    int year;
    int baseSpeed;
    int width, height;
    int driverX, driverY;
    int rearWheelX, rearWheelY;
    int frontWheelX, frontWheelY;
    int gasCapX, gasCapY;
};

enum CarWheelType { STOCK, BRAND, RACING };

class Car {
    /*
        Car positions
         Garage: 42, 82
         Diner :
         Gas station:
    */

   private:
    [[maybe_unused]] bool isFrontBumperChopped;
    [[maybe_unused]] bool isBackBumperChopped;
    [[maybe_unused]] bool isRoofChopped;
    [[maybe_unused]] bool isMoving;

    int tuning;

    CarModel model;
    CarWheelType tires;

    // Body
    SDL_Surface *bodySurface;
    SDL_Texture *bodyTexture;
    SDL_Rect bodyRect;

    // Driver
    SDL_Surface *driverSurface;
    SDL_Texture *driverTexture;
    SDL_Rect driverRect;

    // Wheels
    Spritesheet *rearWheel;
    Spritesheet *frontWheel;


    SDL_Rect rearWheelRect;
    SDL_Rect frontWheelRect;

    // Gas cap
    SDL_Point gasCapLocation;

    // Hood scoop

    Uint64 t;

    void UpdateTextures();

   public:
    Car(CarModel model);
    ~Car();
    SDL_Texture *GetTexture();
    void Draw();
    void Move(SDL_Point p);
    void MoveLeft();
    void MoveRight();
};