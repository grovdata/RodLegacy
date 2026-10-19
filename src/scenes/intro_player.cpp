#include "scenes/intro_player.h"

#include "scenes/intro_frames.h"
#include "scenes/sr2_intro_frames.h"

#include <algorithm>

namespace {

// Palette numbers as the original's FadePalette takes them. Street Rod 2 indexes the nine in its
// executable, Street Rod the four tables below (0, 4, 5, 6).
constexpr int kPaletteBlack = -1;
constexpr int kPaletteCredits = 0;   // SR1's credits plate
constexpr int kPaletteSepia = 4;     // SR1's photo; SR2's title once the full picture is up
constexpr int kPaletteTitle = 5;
constexpr int kPaletteLogo = 6;
constexpr int kPaletteSr2Plate = 2;

constexpr std::array<uint16_t, 32> kPal0 = {
    0x000, 0x810, 0x911, 0xB21, 0xD22, 0xF32, 0xF44, 0xF65, 0xF87, 0xF98, 0xFFF,
    0xEEE, 0xCCC, 0xBBB, 0xAAA, 0x999, 0x888, 0x950, 0xC71, 0xF96, 0x333, 0x222,
    0x05B, 0x07F, 0x0F0, 0x0D0, 0xFE9, 0xFD2, 0xB21, 0xF32, 0x09F, 0x0BF};
constexpr std::array<uint16_t, 32> kPalSepia = {
    0x431, 0xFFD, 0x974, 0x864, 0x853, 0x532, 0xEDA, 0xCCA, 0xEDA, 0xDB9, 0x754,
    0x543, 0x432, 0xCB8, 0xBA7, 0xB97, 0x532, 0x532, 0xEEB, 0xCB8, 0xEEB, 0xA85,
    0x521, 0x000, 0x333, 0x555, 0x864, 0x876, 0xEDA, 0xEDA, 0xEEB, 0xEEC};
constexpr std::array<uint16_t, 32> kPalTitle = {
    0x000, 0xFFF, 0xEBE, 0xF8A, 0xD59, 0xB07, 0xEA8, 0xF97, 0xFC5, 0xFA5, 0xD00,
    0xB02, 0xA03, 0xF74, 0xF63, 0xF51, 0x099, 0x147, 0x09F, 0x5D2, 0x5FF, 0xB50,
    0x821, 0x000, 0x333, 0x555, 0x668, 0x876, 0xBBB, 0xCCC, 0xCEE, 0xDFF};
constexpr std::array<uint16_t, 32> kPalLogo = {
    0x000, 0xBBB, 0xFE3, 0xFD2, 0xFB2, 0xE80, 0x6FF, 0x0D0, 0x080, 0x050, 0xF00,
    0xD00, 0xB00, 0x900, 0x555, 0xEEE, 0x800, 0x600, 0x400, 0x000, 0x666, 0x663,
    0x266, 0x602, 0xD70, 0xC60, 0xB50, 0xA40, 0x710, 0xEF1, 0x030, 0x020};

std::array<uint16_t, 32> PaletteById(int id) {
    switch (id) {
        case kPaletteCredits: return kPal0;
        case kPaletteSepia: return kPalSepia;
        case kPaletteTitle: return kPalTitle;
        case kPaletteLogo: return kPalLogo;
        default: return {};
    }
}

// Index into the images given to Start (Street Rod, libstart.p 1 to 5 in order).
constexpr int kLogoImage = 1;
constexpr int kPlateImage = 2;
constexpr int kTitleScreenImage = 3;
constexpr int kTitleSheetImage = 4;
constexpr int kPhotoImage = 5;

// libstart.p indices in Street Rod 2.
constexpr int kSr2LogoImage = 30;
constexpr int kSr2TitleImage = 29;
constexpr int kSr2PlateImage = 31;

// Ticks, one per vblank. kFadeRate is the extra wait per step of the slow fades.
constexpr int kFadeRate = 5;
constexpr int kLogoHoldTicks = 150;
constexpr int kLogoStepTicks = 2;

// Guessed: the Amiga's own delays (2, 5, 5) played too fast here.
constexpr int kSr2RunDelays[3] = {4, 10, 10};

// ponytail: guessed at 2 vblanks a step, measure on hardware and change this one number.
constexpr int kTitleExtraTicks = 2;

std::vector<int> StepStarts(const int16_t* steps, int stepCount) {
    std::vector<int> starts;
    int i = 0;
    for (int k = 0; k < stepCount; k++) {
        starts.push_back(i);
        while (steps[i] != -1) i++;
        i++;
    }
    return starts;
}

const std::vector<int>& LogoStarts() {
    static const std::vector<int> s = StepStarts(kIntroLogoSteps, kIntroLogoStepCount);
    return s;
}

const std::vector<int>& TitleStarts() {
    static const std::vector<int> s = StepStarts(kIntroTitleSteps, kIntroTitleStepCount);
    return s;
}

int Sr2StepStart(int step) {
    static const std::vector<int> s = StepStarts(kSr2IntroSteps, kSr2IntroStepCount);
    return s[step];
}

}

void IntroPlayer::Reset() {
    for (auto& b : buffers) b.assign(kWidth * kHeight, 0);
    front = 0;
    palette = {};
    pc = 0;
    wait = 0;
    fadeStep = -1;
    ops.clear();
}

void IntroPlayer::Start(const std::array<IntroImage, 6>& imgs) {
    images = &imgs;
    sr2Images = nullptr;
    logoImage = kLogoImage;
    Reset();
    Build();
}

void IntroPlayer::StartSr2(const std::vector<IntroImage>& libstart,
                           const std::vector<std::array<uint16_t, 32>>& palettes) {
    images = nullptr;
    sr2Images = &libstart;
    sr2Palettes = palettes;
    logoImage = kSr2LogoImage;
    Reset();
    BuildSr2();
}

const IntroImage& IntroPlayer::Image(int i) const {
    return sr2Images ? (*sr2Images)[i] : (*images)[i];
}

std::array<uint16_t, 32> IntroPlayer::PaletteOf(int id) const {
    if (!sr2Images) return PaletteById(id);
    if (id < 0 || id >= static_cast<int>(sr2Palettes.size())) return {};
    return sr2Palettes[id];
}

void IntroPlayer::BuildLogo() {
    Emit(OpKind::Clear);
    for (int k = 0; k < kIntroLogoStepCount; k++) {
        if (k == 0) Emit(OpKind::Fade, kPaletteBlack, kPaletteLogo, kFadeRate);
        if (k == 1) Emit(OpKind::Wait, kLogoHoldTicks);
        if (k == 4) EmitSound(IntroSound::ChimeA);
        if (k == 9) EmitSound(IntroSound::ChimeB);
        if (k == 15) EmitSound(IntroSound::ChimeC);
        Emit(OpKind::LogoCels, k);
        Emit(OpKind::Flip);
        Emit(OpKind::Wait, kLogoStepTicks);
    }
    Emit(OpKind::FadeToBlack);
}

void IntroPlayer::BuildSr2() {
    BuildLogo();

    Emit(OpKind::DrawImage, kSr2TitleImage);
    Emit(OpKind::SetPalette, kPaletteTitle);
    // The title animation (0x232f90) starts the music before its first step.
    EmitSound(IntroSound::StartMusic);

    // Three runs of steps with their own delay. The second ends on the full title picture, after
    // which the palette settles.
    int step = 0;
    for (int run = 0; run < 3; run++) {
        for (int i = 0; i < kSr2IntroRunLengths[run]; i++, step++) {
            Emit(OpKind::Sr2Cels, step);
            Emit(OpKind::Flip);
            if (run == 1 && i == kSr2IntroRunLengths[1] - 1) {
                Emit(OpKind::Fade, kPaletteTitle, kPaletteSepia, 0);
            }
            Emit(OpKind::Wait, kSr2RunDelays[run]);
        }
    }

    Emit(OpKind::FadeToBlack);
    Emit(OpKind::Clear);
    Emit(OpKind::DrawImage, kSr2PlateImage);
    Emit(OpKind::Fade, kPaletteBlack, kPaletteSr2Plate, 0);
}

void IntroPlayer::Build() {
    BuildLogo();

    Emit(OpKind::DrawImage, kPhotoImage);
    Emit(OpKind::Fade, kPaletteBlack, kPaletteSepia, kFadeRate);   // black to sepia
    Emit(OpKind::Fade, kPaletteSepia, kPaletteTitle, kFadeRate);   // and on to colour

    Emit(OpKind::TitleSetup);
    for (int k = 0; k < kIntroTitleStepCount; k++) {
        switch (k) {
            case 4:  EmitSound(IntroSound::ChimeB); break;
            case 12: EmitSound(IntroSound::ChimeC); break;
            case 19: EmitSound(IntroSound::ChimeA); break;
            case 31:
            case 38: EmitSound(IntroSound::Ignition); break;
            case 43: EmitSound(IntroSound::Hit100); break;
            case 44: EmitSound(IntroSound::Hit200); break;
            case 45: EmitSound(IntroSound::Hit150); break;
            case 48: EmitSound(IntroSound::Screech); break;
            case 50: EmitSound(IntroSound::StartMusic); break;
            default: break;
        }
        Emit(OpKind::TitleCels, k);
        Emit(OpKind::Flip);
        Emit(OpKind::Wait, kTitleExtraTicks);
    }

    Emit(OpKind::FadeToBlack);
    Emit(OpKind::Clear);
    Emit(OpKind::DrawImage, kPlateImage);
    Emit(OpKind::Fade, kPaletteBlack, kPaletteCredits, 0);
}

void IntroPlayer::Tick(std::vector<IntroSound>& sounds) {
    if (wait > 0) {
        wait--;
        return;
    }
    while (pc < ops.size()) {
        if (Step(sounds)) return;
    }
}

void IntroPlayer::Blit(const IntroImage& src, int sx, int sy, int w, int h,
                       int dx, int dy, bool opaque) {
    std::vector<uint8_t>& dst = buffers[1 - front];
    for (int y = 0; y < h; y++) {
        const int ty = dy + y;
        const int fy = sy + y;
        if (ty < 0 || ty >= kHeight || fy < 0 || fy >= src.h) continue;
        for (int x = 0; x < w; x++) {
            const int tx = dx + x;
            const int fx = sx + x;
            if (tx < 0 || tx >= kWidth || fx < 0 || fx >= src.w) continue;
            const uint8_t p = src.px[fy * src.w + fx];
            if (opaque || p != 0) dst[ty * kWidth + tx] = p;
        }
    }
}

// The Amiga redraws each step into both buffers, so one pass here goes to both. xorBlit is minterm 0x60.
void IntroPlayer::BlitBoth(const IntroImage& src, int w, int h, int dx, int dy, bool xorBlit) {
    w = std::min(w, src.w);
    h = std::min(h, src.h);
    for (auto& dst : buffers) {
        for (int y = 0; y < h; y++) {
            const int ty = dy + y;
            if (ty < 0 || ty >= kHeight) continue;
            for (int x = 0; x < w; x++) {
                const int tx = dx + x;
                if (tx < 0 || tx >= kWidth) continue;
                const uint8_t p = src.px[y * src.w + x];
                uint8_t& d = dst[ty * kWidth + tx];
                d = xorBlit ? d ^ p : p;
            }
        }
    }
}

bool IntroPlayer::Step(std::vector<IntroSound>& sounds) {
    const Op& op = ops[pc];
    switch (op.kind) {
        case OpKind::Clear:
            for (auto& b : buffers) std::fill(b.begin(), b.end(), 0);
            break;

        case OpKind::DrawImage: {
            const IntroImage& img = Image(op.a);
            const int w = std::min(img.w, kWidth);
            const int h = std::min(img.h, kHeight);
            for (auto& b : buffers) {
                for (int y = 0; y < h; y++) {
                    std::copy_n(img.px.begin() + y * img.w, w, b.begin() + y * kWidth);
                }
            }
            break;
        }

        case OpKind::Fade:
            if (fadeStep < 0) {
                palette = PaletteOf(op.a);
                fadeTarget = PaletteOf(op.b);
                fadeStep = 0;
            }
            for (int i = 0; i < 32; i++) {
                uint16_t v = 0;
                for (int shift = 0; shift <= 8; shift += 4) {
                    int cur = (palette[i] >> shift) & 0xF;
                    const int target = (fadeTarget[i] >> shift) & 0xF;
                    cur += (cur < target) - (cur > target);
                    v |= cur << shift;
                }
                palette[i] = v;
            }
            wait = op.c;
            if (++fadeStep == 16) {
                fadeStep = -1;
                pc++;
            }
            return true;

        case OpKind::FadeToBlack: {
            int m = 0;
            for (uint16_t c : palette) {
                m = std::max({m, c >> 8, (c >> 4) & 0xF, c & 0xF});
            }
            if (m == 0) break;
            for (uint16_t& c : palette) {
                uint16_t v = 0;
                for (int shift = 0; shift <= 8; shift += 4) {
                    int ch = (c >> shift) & 0xF;
                    if (ch == m) ch = m - 1;
                    v |= ch << shift;
                }
                c = v;
            }
            return true;
        }

        case OpKind::Wait:
            pc++;
            if (op.a <= 0) return false;
            wait = op.a - 1;
            return true;

        case OpKind::Sound:
            sounds.push_back(static_cast<IntroSound>(op.a));
            break;

        case OpKind::LogoCels:
            for (int i = LogoStarts()[op.a]; kIntroLogoSteps[i] != -1; i++) {
                const IntroCel& c = kIntroLogoCels[kIntroLogoSteps[i]];
                Blit(Image(logoImage), c.sx, c.sy, c.w, c.h, c.dx, c.dy, c.flags & 2);
            }
            break;

        case OpKind::TitleSetup: {
            // Keep only the car out of the photo: two rects to the cleared
            // back buffer, then the whole back buffer to the front.
            std::vector<uint8_t>& back = buffers[1 - front];
            const std::vector<uint8_t>& shown = buffers[front];
            std::fill(back.begin(), back.end(), 0);
            const int rects[2][4] = {{59, 2, 242, 195}, {18, 45, 41, 152}};
            for (const auto& r : rects) {
                for (int y = r[1]; y < r[1] + r[3]; y++) {
                    std::copy_n(shown.begin() + y * kWidth + r[0], r[2],
                                back.begin() + y * kWidth + r[0]);
                }
            }
            buffers[front] = back;
            break;
        }

        case OpKind::TitleCels:
            for (int i = TitleStarts()[op.a]; kIntroTitleSteps[i] != -1; i++) {
                const IntroCel& c = kIntroTitleCels[kIntroTitleSteps[i]];
                const int img = (c.flags & 1) ? kTitleSheetImage : kTitleScreenImage;
                Blit((*images)[img], c.sx, c.sy, c.w, c.h, c.dx, c.dy, c.flags & 2);
            }
            break;

        case OpKind::SetPalette:
            palette = PaletteOf(op.a);
            break;

        case OpKind::Sr2Cels:
            for (int i = Sr2StepStart(op.a); kSr2IntroSteps[i] != -1; i++) {
                const Sr2IntroCel& c = kSr2IntroCels[kSr2IntroSteps[i]];
                BlitBoth(Image(c.image), c.w, c.h, c.dx, c.dy, c.xorBlit);
            }
            break;

        case OpKind::Flip:
            front = 1 - front;
            pc++;
            return true;
    }
    pc++;
    return false;
}
