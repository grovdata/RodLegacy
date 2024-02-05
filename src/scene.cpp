#include "scene.h"
#include "state.h"
#include "ui/zone.h"

Scene::Scene(std::string backgroundName) {
    sceneTransition = SCENE_SOLID;
    m_Surface = IMG_Load(backgroundName.c_str());
    m_Texture = SDL_CreateTextureFromSurface(renderer, m_Surface);
    SDL_SetTextureBlendMode(m_Texture, SDL_BLENDMODE_BLEND);
    m_FontSurface = SDL_CreateRGBSurface(0, 320, 200, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    m_backgroundRect.x = 0;
    m_backgroundRect.y = 0;
    m_backgroundRect.w = 320;
    m_backgroundRect.h = 200;

    m_statusBarRect.x = 0;
    m_statusBarRect.y = 200 - 9;
    m_statusBarRect.w = 320;
    m_statusBarRect.h = 9;

    m_statusBarMiddleRect = m_statusBarRect;
    m_statusBarMiddleRect.x = 160;
}

Scene::~Scene() {
    SDL_FreeSurface(m_FontSurface);
    SDL_DestroyTexture(m_FontTexture);
    SDL_FreeSurface(m_Surface);
    SDL_DestroyTexture(m_Texture);
}

SDL_Point Scene::GetCarIdlePosition() {
    return {0, 0};
}

SDL_Point Scene::GetCarOffscreenPosition() {
    return {0, 0};
}

void Scene::SetStatusBar(char *text) {
    char bankroll[30];
    snprintf(bankroll, 30, "Bankroll: $%d", state.GetBankroll());
    SFont_Write(m_FontSurface, font, 2, 200 - 9, bankroll);
    SFont_Write(m_FontSurface, font, 160, 200 - 9, text);
    m_FontTexture = SDL_CreateTextureFromSurface(renderer, m_FontSurface);
    SDL_SetTextureColorMod(m_FontTexture, 119, 102, 17);
}

void Scene::ClearStatusBar() {
    SDL_FillRect(m_FontSurface, &m_statusBarMiddleRect, 0);
    m_FontTexture = SDL_CreateTextureFromSurface(renderer, m_FontSurface);
    SDL_SetTextureColorMod(m_FontTexture, 119, 102, 17);
}

ZoneId Scene::CheckMouseHover(int x, int y) {
    for (size_t i = 0; i < m_zones.size(); i++) {
        Zone zone = m_zones.at(i);
        if (zone.IsInsideZone(x, y)) {
            ClearStatusBar();
            SetStatusBar(zone.GetText());
            return zone.GetId();
        }
    }
    // If no hover, clear bar
    ClearStatusBar();
    return ZONE_NULL;
}

void Scene::SetZoneText(ZoneId id, char* text) {
   for (size_t i = 0; i < m_zones.size(); i++) {
        Zone zone = m_zones.at(i);
        if (zone.GetId() == id) {
            zone.SetText(text);
            return;
        }
    }
}

SDL_Texture *Scene::GetTexture() {
    return m_Texture;
}

void Scene::Draw() {
    SDL_RenderCopy(renderer, m_Texture, NULL, &m_backgroundRect);
    SDL_RenderCopy(renderer, m_FontTexture, NULL, &m_backgroundRect);
}

void Scene::DrawMaskAfterCar() {
}

void Scene::PushEvent(SDL_Event *event) {
    ZoneId z = CheckMouseHover(event->motion.x, event->motion.y);
    if (event->type == SDL_MOUSEBUTTONDOWN || event->type == SDL_MOUSEBUTTONUP) {
        Action(z);
    }
}

void Scene::Action(ZoneId zone) {
    printf("Zone was selected: %d \n", zone);
}


void Scene::FadeIn() {
    if (sceneTransition == SCENE_FADE_OUT) {
        printf("fadein\n");
        fadeTimerID = SDL_AddTimer(4, FadeCallback, this);
        sceneTransition = SCENE_FADE_IN;
    }
}

void Scene::FadeOut() {
    if (sceneTransition == SCENE_SOLID) {
        printf("fadeout\n");
        fadeTimerID = SDL_AddTimer(4, FadeCallback, this);
        sceneTransition = SCENE_FADE_OUT;
    }
}

Uint32 Scene::FadeCallback(Uint32 interval, void* param) {
    Scene* scene = static_cast<Scene*>(param);
    Uint8 alpha;
    SDL_GetTextureAlphaMod(scene->m_Texture, &alpha);
    printf("alpha %d\n",alpha);
    int rate = 2;

    if (scene->sceneTransition == SCENE_FADE_IN) {
        if (alpha >= SDL_ALPHA_OPAQUE) {
            scene->sceneTransition = SCENE_SOLID;
            scene->OnFadeComplete();
            return 0;
        }
        alpha += rate;
    } else if (scene->sceneTransition == SCENE_FADE_OUT) {
        if (alpha <= SDL_ALPHA_TRANSPARENT) {
            scene->OnFadeComplete();
            return 0;
        }
        alpha -= rate;
    }

    SDL_SetTextureAlphaMod(scene->m_Texture, alpha);
    return interval;
}

void Scene::OnFadeComplete() {

}

/// @brief Make a car pull into the scene
/// @param car
/// @param driver

void Scene::StartAnimateCar(Car *car, Driver *driver, bool keepDriving) {
}