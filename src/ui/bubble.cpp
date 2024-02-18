#include "bubble.h"

using namespace std;

Bubble::Bubble(string text, BubbleStyle style) {
    string filename = "";
    switch (style) {
        case BUBBLE_LEFT:
            filename = "lib_p_0.gif";
            break;
        case BUBBLE_RIGHT:
            filename = "lib_p_1.gif";
            break;
        case BUBBLE_RIGHT_TWO_LINES:
            filename = "lib_p_2.gif";
            break;
    }

    // FIXME
    filename = "../data/res/bubbles" + filename;

    LoadBitmap(filename);
}

void Bubble::Draw() {
    Bitmap::Draw();
}