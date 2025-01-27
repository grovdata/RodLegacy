#pragma once

#include "scene.h"
#include "bitmap.h"
#include "ui/label.h"

class Newspaper : public Scene {
private:
    Bitmap exit;
    Bitmap headline;
    
    Bitmap previous;
    Bitmap next;

    Bitmap adsArea;

    Label date;
    Label headingLeft;
    Label headingRight;
    
    Label autoPartsHeader;
    Label usedCarsHeader;

    int page = 0;

    void InitPageOne();
    void InitPageAds();
    void Update();

    void AddDivider(int y);
    
public:
    Newspaper();
    ~Newspaper();

    void Init(bool debugMode);
    void Draw() override;
    void Action(ZoneId zone) override;
    void PushEvent(SDL_Event *event) override;
};
