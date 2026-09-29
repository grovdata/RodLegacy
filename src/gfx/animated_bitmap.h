#pragma once
#include "gfx/bitmap.h"
#include "parsers/libp.h"
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
        std::vector<SDL_Rect> frameRects;
    };

    AnimatedBitmap();
    AnimatedBitmap(Bitmap* sourceBitmap, const SDL_Rect& rect);
    ~AnimatedBitmap() override;

    void Draw() override;
    void SetPosition(int x, int y) override;
    void SetDimensions(int w, int h) override;
    void SetVisible(bool visible) override;
    bool IsVisible() const override;

    void Play();
    void Stop();
    void SetFrame(int frame);
    void SetLoop(bool loop);
    void SetSpeed(float frameDuration);
    void Update(float deltaTime);

    void SetAnimationFrames(const std::vector<SDL_Rect>& frames);
    void SetAnimationFrames(int frameWidth, int frameHeight, int columns, int rows);
    void SetAnimationFromInfo(const SpriteAnimationInfo* animInfo);

    // Cell size is recomputed from the current surface on every selection, since some sheets are Crop()-ed after loading.
    void SetGrid(int rows, int columns);
    void SelectFrame(int row, int col);
    void SelectFrame(int index);
    void CycleNextFrame();
    void CyclePrevFrame();
    int GetRows() const { return gridRows; }
    int GetColumns() const { return gridColumns; }
    SDL_Rect GetCurrentFrameRect() const;

    bool IsPlaying() const { return animation.isPlaying; }
    int GetCurrentFrame() const { return animation.currentFrame; }

private:
    Animation animation;

    int gridRows = 0;
    int gridColumns = 0;
    int currentGridRow = 0;
    int currentGridCol = 0;
};
