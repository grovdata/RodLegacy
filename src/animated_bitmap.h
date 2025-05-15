#pragma once
#include "bitmap.h"
#include <vector>
#include <string>

class AnimatedBitmap : public Bitmap {
public:
    struct Animation {
        int frameCount = 1;
        int currentFrame = 0;
        float frameDuration = 0.0f;
        float elapsedTime = 0.0f;
        bool loop = true;
        bool isPlaying = false;
        std::vector<SDL_Rect> frameRects;  // Source rects for each frame
    };

    AnimatedBitmap(Bitmap* sourceBitmap, const SDL_Rect& rect);
    ~AnimatedBitmap() override;

    // Override Bitmap methods
    void Draw() override;
    void SetPosition(int x, int y);
    void SetDimensions(int w, int h);
    void SetVisible(bool visible);
    bool IsVisible() const;

    // Animation control
    void Play();
    void Stop();
    void SetFrame(int frame);
    void SetLoop(bool loop);
    void SetSpeed(float frameDuration);
    void Update(float deltaTime);

    // Animation setup
    void SetAnimationFrames(const std::vector<SDL_Rect>& frames);
    void SetAnimationFrames(int frameWidth, int frameHeight, int columns, int rows);

    // Getters
    bool IsPlaying() const { return animation.isPlaying; }
    int GetCurrentFrame() const { return animation.currentFrame; }

private:
    Bitmap* sourceBitmap;  // Original bitmap we're animating
    Animation animation;
}; 