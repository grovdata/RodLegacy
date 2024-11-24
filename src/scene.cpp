#include "scene.h"
#include "state.h"
#include "ui/zone.h"
#include "assets.h"
#include "state.h"

Scene::Scene(Asset backgroundAsset) {
    this->isSetup = false;
    this->backgroundAsset = backgroundAsset;
    //background->SetPosition(0, 0);
    //background->SetVisible(true);
    debugOverlay = nullptr;
    debugMode = false;
}

Scene::Scene(std::string backgroundName) {
    this->isSetup = false;
    this->backgroundName = backgroundName;
    debugOverlay = nullptr;
    debugMode = false;
}

Scene::~Scene() {
    SDL_FreeSurface(fontSurface);
    SDL_DestroyTexture(fontTexture);
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
    SDL_FreeSurface(fadeSurface);
    SDL_DestroyTexture(fadeTexture);
    if (debugOverlay) {
        delete debugOverlay;
        debugOverlay = nullptr;
    }
}

MousePointer Scene::GetPointerType() {
    return PointerHand;
}

void Scene::Init(bool debugMode) {
    this->debugMode = debugMode;
    background = state.assets.GetBitmapAsset(backgroundAsset);
    background->SetVisible(true);
    sceneTransition = SCENE_SOLID;
    surface = SDL_CreateRGBSurface(0, GAME_WIDTH, GAME_HEIGHT, 32,
                                    0x00FF0000,
                                    0x0000FF00,
                                    0x000000FF,
                                    0xFF000000);
    if (backgroundName == "") {
    } else {
        SDL_Surface *img = IMG_Load(backgroundName.c_str());
        SDL_Rect destRect = {0, 0, img->w, img->h};
        SDL_BlitSurface(img, nullptr, surface, &destRect);
    }
    texture = SDL_CreateTextureFromSurface(renderer, surface);
    textures.push_back(texture);
    fontSurface = SDL_CreateRGBSurface(0, GAME_WIDTH, GAME_HEIGHT, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    m_backgroundRect.x = 0;
    m_backgroundRect.y = 0;
    m_backgroundRect.w = GAME_WIDTH;
    m_backgroundRect.h = GAME_HEIGHT;

    fadeSurface = SDL_CreateRGBSurface(0, GAME_WIDTH, GAME_HEIGHT, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    SDL_FillRect(fadeSurface, &m_backgroundRect, SDL_MapRGB(fadeSurface->format, 0, 0, 0));
    fadeTexture = SDL_CreateTextureFromSurface(renderer, fadeSurface);
    SDL_SetTextureBlendMode(fadeTexture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureAlphaMod(fadeTexture, SDL_ALPHA_OPAQUE);

    m_statusBarRect.x = 0;
    m_statusBarRect.y = GAME_HEIGHT - GAME_STATUSBAR_HEIGHT;
    m_statusBarRect.w = GAME_WIDTH;
    m_statusBarRect.h = GAME_STATUSBAR_HEIGHT;

    m_statusBarMiddleRect = m_statusBarRect;
    m_statusBarMiddleRect.x = GAME_WIDTH/2;

    if (debugMode) {
        debugOverlay = new DebugOverlay(renderer, state.assets.GetDebugFont());
    }
}

void Scene::SetStatusBar(char *text) {
    char bankroll[30];
    snprintf(bankroll, 30, "Bankroll: $%d", state.GetBankroll());
    SFont_Write(fontSurface, state.assets.GetMainFont(), 2, GAME_HEIGHT - GAME_STATUSBAR_HEIGHT, bankroll);
    SFont_Write(fontSurface, state.assets.GetMainFont(), 160, GAME_HEIGHT - GAME_STATUSBAR_HEIGHT, text);
    fontTexture = SDL_CreateTextureFromSurface(renderer, fontSurface);
    SDL_SetTextureColorMod(fontTexture, 119, 102, 17);
}

void Scene::ClearStatusBar() {
    SDL_FillRect(fontSurface, &m_statusBarMiddleRect, 0);
    fontTexture = SDL_CreateTextureFromSurface(renderer, fontSurface);
    SDL_SetTextureColorMod(fontTexture, 119, 102, 17);
}

ZoneId Scene::CheckMouseHover(int x, int y) {
    for (size_t i = 0; i < zones.size(); i++) {
        Zone zone = zones.at(i);
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
   for (size_t i = 0; i < zones.size(); i++) {
        Zone zone = zones.at(i);
        if (zone.GetId() == id) {
            zone.SetText(text);
            return;
        }
    }
}

SDL_Texture *Scene::GetTexture() {
    return texture;
}

void Scene::Draw() {
    background->Draw();
    SDL_RenderCopy(renderer, texture, NULL, &m_backgroundRect);
    SDL_RenderCopy(renderer, fontTexture, NULL, &m_backgroundRect);

    if (debugMode) {
        UpdateDebugInfo();
        debugOverlay->Draw();
    }
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
    SDL_RemoveTimer(fadeTimerID);
    SDL_SetTextureAlphaMod(fadeTexture, SDL_ALPHA_OPAQUE);
    fadeTimerID = SDL_AddTimer(2, FadeCallback, this);
    sceneTransition = SCENE_FADE_IN;
}

void Scene::FadeOut() {
    SDL_RemoveTimer(fadeTimerID);
    SDL_SetTextureAlphaMod(fadeTexture, SDL_ALPHA_TRANSPARENT);
    fadeTimerID = SDL_AddTimer(2, FadeCallback, this);
    sceneTransition = SCENE_FADE_OUT;
}

Uint32 Scene::FadeCallback(Uint32 interval, void* param) {
    Scene* scene = static_cast<Scene*>(param);
    Uint8 alpha;
    SDL_GetTextureAlphaMod(scene->fadeTexture, &alpha);
    // printf("alpha %d\n",alpha);
    int rate = 1;
    int retval = interval;

    if (scene->sceneTransition == SCENE_FADE_IN) {
        if (alpha <= SDL_ALPHA_TRANSPARENT + rate) {
            scene->sceneTransition = SCENE_SOLID;
            scene->OnFadeComplete();
            return 0;
        }
        alpha -= rate;
    } else if (scene->sceneTransition == SCENE_FADE_OUT) {
        if (alpha >= SDL_ALPHA_TRANSPARENT - rate) {
            scene->OnFadeComplete();
            return 0;
        }
        alpha += rate;
    }

    SDL_SetTextureAlphaMod(scene->fadeTexture, alpha);
    return retval;
}

void Scene::OnFadeComplete() {
    if (sceneTransition == SCENE_FADE_OUT) {
        sceneTransition = SCENE_BLACK;
        state.MoveToScene(sceneTransitioningTo);
        // SDL_SetTextureAlphaMod(this->m_Texture, SDL_ALPHA_TRANSPARENT);
    } else if (sceneTransition == SCENE_FADE_IN) {
        sceneTransition = SCENE_SOLID;
        // SDL_SetTextureAlphaMod(this->m_Texture, SDL_ALPHA_OPAQUE);
    }
}

void Scene::UpdateDebugInfo() {
    // Base implementation - can be overridden by derived classes
    if (debugOverlay) {
        debugOverlay->Clear();
        debugOverlay->AddLine("Scene: " + std::string(typeid(*this).name()));
        debugOverlay->AddLine("Time: " + std::to_string(SDL_GetTicks64()));
    }
}


CarScene::CarScene(std::string backgroundName): Scene(backgroundName) {
}

CarScene::CarScene(Asset backgroundAsset): Scene(backgroundAsset) {
}

void CarScene::Init(bool debugMode) {
    Scene::Init(debugMode);
}

//void CarScene::Draw() {
//    if (!isSetup) {
//        Init();
//        isSetup = true;
//    }
//    Scene::Draw();
//}

void CarScene::StartAnimateCar(Car *car, Driver *driver, bool keepDriving) {
}

SDL_Point CarScene::GetCarIdlePosition() {
    return {0, 0};
}

SDL_Point CarScene::GetCarOffscreenPosition() {
    return {0, 0};
}

void CarScene::DrawMaskAfterCar() {
    SDL_RenderCopy(renderer, fadeTexture, NULL, &m_backgroundRect);
}

void CarScene::UpdateDebugInfo() {
    if (!debugMode) return;
    Scene::UpdateDebugInfo();
}
