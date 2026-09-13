#include "gfx/animated_bitmap.h"

AnimatedBitmap::AnimatedBitmap() {
}

AnimatedBitmap::AnimatedBitmap(Bitmap* sourceBitmap, const SDL_Rect& rect)
    : Bitmap(*sourceBitmap) {
    SetPosition(rect.x, rect.y);
    SetDimensions(rect.w, rect.h);
    SetVisible(true);
}

AnimatedBitmap::~AnimatedBitmap() {
}

void AnimatedBitmap::Draw() {
    if (!IsVisible()) return;

    // Only draw if we have animation frames - don't call Bitmap::Draw()
    if (animation.frameCount > 0 && static_cast<size_t>(animation.currentFrame) < animation.frameRects.size() && texture != NULL) {
        SDL_Rect srcRect = animation.frameRects[animation.currentFrame];
        SDL_RenderCopy(renderer, texture.get(), &srcRect, &rect);
    }
}

void AnimatedBitmap::SetPosition(int x, int y) {
    Bitmap::SetPosition(x, y);
}

void AnimatedBitmap::SetDimensions(int w, int h) {
    Bitmap::SetDimensions(w, h);
}

void AnimatedBitmap::SetVisible(bool visible) {
    Bitmap::SetVisible(visible);
}

bool AnimatedBitmap::IsVisible() const {
    return Bitmap::IsVisible();
}

void AnimatedBitmap::Play() {
    animation.isPlaying = true;
}

void AnimatedBitmap::Stop() {
    animation.isPlaying = false;
}

void AnimatedBitmap::SetFrame(int frame) {
    if (frame >= 0 && frame < animation.frameCount) {
        animation.currentFrame = frame;
    }
}

void AnimatedBitmap::SetLoop(const bool loop) {
    animation.loop = loop;
}

void AnimatedBitmap::SetSpeed(float frameDuration) {
    animation.frameDuration = frameDuration;
}

void AnimatedBitmap::Update(float deltaTime) {
    if (!animation.isPlaying || int(animation.frameCount) <= 1) return;

    animation.elapsedTime += deltaTime;
    if (animation.elapsedTime >= animation.frameDuration) {
        animation.elapsedTime = 0;
        animation.currentFrame++;
        if (animation.currentFrame >= animation.frameCount) {
            if (animation.loop) {
                animation.currentFrame = 0;
            } else {
                animation.currentFrame = animation.frameCount - 1;
                animation.isPlaying = false;
            }
        }
    }
}

void AnimatedBitmap::SetAnimationFrames(const std::vector<SDL_Rect>& frames) {
    animation.frameRects = frames;
    animation.frameCount = frames.size();
}

void AnimatedBitmap::SetAnimationFrames(int frameWidth, int frameHeight, int columns, int rows) {
    animation.frameRects.clear();
    animation.frameCount = columns * rows;
    animation.currentFrame = 0; // Reset to first frame

    for (int y = 0; y < rows; y++) {
        for (int x = 0; x < columns; x++) {
            SDL_Rect frame = {
                x * frameWidth,
                y * frameHeight,
                frameWidth,
                frameHeight
            };
            animation.frameRects.push_back(frame);
        }
    }

    // Immediately set the source rect to the first frame
    if (animation.frameCount > 0) {
        SetSourceRect(animation.frameRects[0]);
    }
}

void AnimatedBitmap::SetAnimationFromInfo(const SpriteAnimationInfo* animInfo) {
    if (!animInfo) return;

    // Convert SpriteRect array to SDL_Rect vector
    animation.frameRects.clear();
    for (uint8_t i = 0; i < animInfo->frameCount; i++) {
        const SpriteRect& rect = animInfo->frames[i];
        animation.frameRects.push_back({rect.x, rect.y, rect.w, rect.h});
    }

    animation.frameCount = animInfo->frameCount;
    animation.loop = animInfo->isLooping;
    animation.currentFrame = 0;

    // Set speed based on milliseconds per frame
    animation.frameDuration = animInfo->animationSpeed / 1000.0f;  // Convert ms to seconds

    // Set source rect to first frame
    if (animation.frameCount > 0) {
        SetSourceRect(animation.frameRects[0]);
    }
}

void AnimatedBitmap::SetGrid(int rows, int columns) {
    gridRows = rows;
    gridColumns = columns;
    currentGridRow = 0;
    currentGridCol = 0;
}

void AnimatedBitmap::SelectFrame(int row, int col) {
    if (gridRows <= 0 || gridColumns <= 0 || !surface) return;

    currentGridRow = row;
    currentGridCol = col;

    int cellW = surface->w / gridColumns;
    int cellH = surface->h / gridRows;
    SDL_Rect frame = {col * cellW, row * cellH, cellW, cellH};

    animation.frameRects = {frame};
    animation.frameCount = 1;
    animation.currentFrame = 0;

    // Default to drawing the cell unscaled; callers wanting a different
    // on-screen size can override with SetDimensions after selecting.
    SetDimensions(cellW, cellH);
}

void AnimatedBitmap::SelectFrame(int index) {
    if (gridColumns <= 0) return;
    SelectFrame(index / gridColumns, index % gridColumns);
}

void AnimatedBitmap::CycleNextFrame() {
    if (gridRows <= 0 || gridColumns <= 0) return;

    if (currentGridCol == gridColumns - 1) {
        currentGridCol = 0;
        currentGridRow = (currentGridRow == gridRows - 1) ? 0 : currentGridRow + 1;
    } else {
        currentGridCol++;
    }
    SelectFrame(currentGridRow, currentGridCol);
}

void AnimatedBitmap::CyclePrevFrame() {
    if (gridRows <= 0 || gridColumns <= 0) return;

    if (currentGridCol == 0) {
        currentGridCol = gridColumns - 1;
        currentGridRow = (currentGridRow == 0) ? gridRows - 1 : currentGridRow - 1;
    } else {
        currentGridCol--;
    }
    SelectFrame(currentGridRow, currentGridCol);
}

SDL_Rect AnimatedBitmap::GetCurrentFrameRect() const {
    if (animation.frameRects.empty()) return {0, 0, 0, 0};
    return animation.frameRects[animation.currentFrame];
}
