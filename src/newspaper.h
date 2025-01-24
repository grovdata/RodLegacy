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

    Label date;
    Label headingLeft;
    Label headingRight;

    int page = 0;
    
public:
    Newspaper();
    ~Newspaper();

    void Init(bool debugMode);

    
    void Draw() override;
};