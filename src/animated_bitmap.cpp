#include "animated_bitmap.h"

AnimatedBitmap::AnimatedBitmap(Bitmap* sourceBitmap, const SDL_Rect& rect)
    : Bitmap(*sourceBitmap), sourceBitmap(sourceBitmap) {
    SetPosition(rect.x, rect.y);
    SetDimensions(rect.w, rect.h);
    SetVisible(true);
}

AnimatedBitmap::~AnimatedBitmap() {
    // Note: We don't delete sourceBitmap as it's owned by the asset manager
}

void AnimatedBitmap::Draw() {
    if (!IsVisible()) return;

    if (animation.frameCount > 1) {
        SDL_Rect srcRect = animation.frameRects[animation.currentFrame];
        SetSourceRect(srcRect);
    }
    Bitmap::Draw();
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
    if (!animation.isPlaying || animation.frameCount <= 1) return;

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
} 