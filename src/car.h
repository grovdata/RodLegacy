#pragma once

#include "global.h"
extern SDL_Renderer *renderer;
enum CarVendor { CHRYSLER, FORD, GM };

struct CarModel {
    CarVendor vendor;
    std::string name;
    std::string filename;
    int year;
    int baseSpeed;
    int width, height;
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
    bool m_frontBumperChopped;
    bool m_backBumperChopped;
    bool m_roofChopped;
    int m_tuning;

    CarModel m_model;
    CarWheelType m_tires;

    SDL_Surface *bodySurface;
    SDL_Texture *bodyTexture;
    SDL_Rect bodyRect;

    SDL_Surface *driverSurface;
    SDL_Texture *driverTexture;
    SDL_Rect driverRect;

    SDL_Point gasCapLocation;

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