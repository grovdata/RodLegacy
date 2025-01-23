#pragma once

#include "scene.h"
#include "bitmap.h"
#include "ui/label.h"

class Newspaper : public Scene {
private:
    Bitmap exit;
    Bitmap headline;
    Label date;
    Label headingLeft;
    Label headingRight;
    
public:
    Newspaper();
    ~Newspaper();

    void Init(bool debugMode);

    
    void Draw() override;
};