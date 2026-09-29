#include "debug/debug_ui.h"

#ifdef RODLEGACY_DEBUG_UI

#include "core/log.h"

#include "imgui.h"
// DockBuilder* is not in the public header.
#include "imgui_internal.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_sdlrenderer2.h"

#include <algorithm>
#include <cstdint>

namespace {
DebugUi* gDebugUi = nullptr;
}

bool DebugUiWantsKeyboard() {
    return gDebugUi != nullptr && gDebugUi->WantsKeyboard();
}

namespace {
void DrawPanelBody(const DebugPanel& panel) {
    if (panel.image != nullptr) {
        int w = 0;
        int h = 0;
        if (SDL_QueryTexture(panel.image, nullptr, nullptr, &w, &h) == 0 && w > 0 && h > 0) {
            const float scale = static_cast<float>(std::max(1, panel.imageScale));
            ImGui::Image(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(panel.image)),
                         ImVec2(static_cast<float>(w) * scale, static_cast<float>(h) * scale));
        }
    }
    for (const DebugToggle& toggle : panel.toggles) {
        if (toggle.value != nullptr) {
            ImGui::Checkbox(toggle.label.c_str(), toggle.value);
        }
    }
    for (const DebugButton& button : panel.buttons) {
        if (ImGui::Button(button.label.c_str()) && button.onClick) {
            button.onClick();
        }
    }
    if ((!panel.toggles.empty() || !panel.buttons.empty()) && !panel.rows.empty()) {
        ImGui::Separator();
    }
    if (!panel.rows.empty() &&
        ImGui::BeginTable("##kv", 2,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
                          ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("key", ImGuiTableColumnFlags_WidthStretch, 0.45f);
        ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch, 0.55f);
        for (const DebugRow& row : panel.rows) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (row.value.empty()) {
                ImGui::TextDisabled("%s", row.key.c_str());
                continue;
            }
            ImGui::TextUnformatted(row.key.c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(row.value.c_str());
        }
        ImGui::EndTable();
    }
    if (!panel.indicators.empty()) {
        if (!panel.rows.empty()) {
            ImGui::Separator();
        }
        for (size_t i = 0; i < panel.indicators.size(); ++i) {
            if (i != 0) {
                ImGui::SameLine();
            }
            const DebugIndicator& lamp = panel.indicators[i];
            const ImVec4 lit(0.16f, 0.70f, 0.28f, 1.0f);
            const ImVec4 dark(0.14f, 0.14f, 0.16f, 1.0f);
            const ImVec4 fill = lamp.active ? lit : dark;
            // Hover and active colours match the base: these are read-outs, not
            // buttons, and should not respond to the mouse.
            ImGui::PushStyleColor(ImGuiCol_Button, fill);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, fill);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, fill);
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  lamp.active ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f)
                                              : ImVec4(0.45f, 0.45f, 0.48f, 1.0f));
            ImGui::Button(lamp.label.c_str());
            ImGui::PopStyleColor(4);
        }
    }
    for (const std::string& line : panel.lines) {
        ImGui::TextUnformatted(line.c_str());
    }
}
}

DebugUi::~DebugUi() {
    Shutdown();
}

bool DebugUi::Init(SDL_Window* win, SDL_Renderer* ren, int gameWidth, int gameHeight) {
    if (!win || !ren || gameWidth <= 0 || gameHeight <= 0) {
        return false;
    }
    window = win;
    renderer = ren;
    gameW = gameWidth;
    gameH = gameHeight;

    gameTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                    SDL_TEXTUREACCESS_TARGET, gameW, gameH);
    if (!gameTexture) {
        LOG_INFO("DebugUi::Init - could not create game texture: %s", SDL_GetError());
        return false;
    }
    SDL_SetTextureBlendMode(gameTexture, SDL_BLENDMODE_NONE);

    IMGUI_CHECKVERSION();
    if (ImGui::CreateContext() == nullptr) {
        LOG_INFO("DebugUi::Init - ImGui::CreateContext failed");
        SDL_DestroyTexture(gameTexture);
        gameTexture = nullptr;
        return false;
    }
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = "debug_ui.ini";
    ImGui::StyleColorsDark();

    if (!ImGui_ImplSDL2_InitForSDLRenderer(window, renderer) ||
        !ImGui_ImplSDLRenderer2_Init(renderer)) {
        LOG_INFO("DebugUi::Init - ImGui SDL backend init failed");
        ImGui::DestroyContext();
        SDL_DestroyTexture(gameTexture);
        gameTexture = nullptr;
        return false;
    }

    active = true;
    gDebugUi = this;
    LOG_INFO("DebugUi::Init - debug UI active (game texture %dx%d)", gameW, gameH);
    return true;
}

void DebugUi::Shutdown() {
    if (gDebugUi == this) {
        gDebugUi = nullptr;
    }
    if (active) {
        ImGui_ImplSDLRenderer2_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
        active = false;
    }
    if (gameTexture) {
        SDL_DestroyTexture(gameTexture);
        gameTexture = nullptr;
    }
}

bool DebugUi::ProcessEvent(const SDL_Event& event) {
    if (!active) {
        return false;
    }
    ImGui_ImplSDL2_ProcessEvent(&event);

    const ImGuiIO& io = ImGui::GetIO();
    switch (event.type) {
        case SDL_MOUSEMOTION:
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
        case SDL_MOUSEWHEEL:
            return io.WantCaptureMouse && !gameHovered;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
        case SDL_TEXTINPUT:
            return io.WantCaptureKeyboard;
        default:
            return false;
    }
}

bool DebugUi::WantsKeyboard() const {
    return active && ImGui::GetIO().WantCaptureKeyboard;
}

bool DebugUi::WantsMouse() const {
    return active && ImGui::GetIO().WantCaptureMouse && !gameHovered;
}

void DebugUi::Draw(const std::vector<DebugPanel>& panels) {
    if (!active) {
        return;
    }

    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("##DebugHost", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
                 ImGuiWindowFlags_NoDocking);
    ImGui::PopStyleVar(3);

    const ImGuiID dockspaceId = ImGui::GetID("RodLegacyDockspace");
    ImGui::DockSpace(dockspaceId);

    // Default layout only when nothing is restored. Docking by name works for panels that do not exist yet.
    if (ImGui::DockBuilderGetNode(dockspaceId) == nullptr) {
        ImGui::DockBuilderRemoveNode(dockspaceId);
        ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceId, viewport->WorkSize);

        ImGuiID centreId = dockspaceId;
        const ImGuiID leftId =
            ImGui::DockBuilderSplitNode(centreId, ImGuiDir_Left, 0.22f, nullptr, &centreId);
        const ImGuiID rightId =
            ImGui::DockBuilderSplitNode(centreId, ImGuiDir_Right, 0.28f, nullptr, &centreId);

        ImGui::DockBuilderDockWindow("Game", centreId);

        // Separate nodes per panel; docking all to one would collapse them into tabs.
        auto stackColumn = [](ImGuiID column, const std::vector<const char*>& titles) {
            ImGuiID remaining = column;
            for (size_t i = 0; i < titles.size(); ++i) {
                if (i + 1 == titles.size()) {
                    ImGui::DockBuilderDockWindow(titles[i], remaining);
                    break;
                }
                const float ratio = 1.0f / static_cast<float>(titles.size() - i);
                const ImGuiID slot =
                    ImGui::DockBuilderSplitNode(remaining, ImGuiDir_Up, ratio, nullptr, &remaining);
                ImGui::DockBuilderDockWindow(titles[i], slot);
            }
        };

        stackColumn(leftId, {"Scenes", "Session"});
        stackColumn(rightId, {"Scene", "Racing", "Opponent"});
        ImGui::DockBuilderFinish(dockspaceId);
    }
    ImGui::End();

    // Largest whole multiple that fits, so the pixel art stays sharp.
    gameHovered = false;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    if (ImGui::Begin("Game")) {
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        int scale = static_cast<int>(std::min(avail.x / static_cast<float>(gameW),
                                              avail.y / static_cast<float>(gameH)));
        if (scale < 1) {
            scale = 1;
        }
        const float drawW = static_cast<float>(gameW * scale);
        const float drawH = static_cast<float>(gameH * scale);
        const ImVec2 cursor(ImGui::GetCursorPosX() + std::max(0.0f, (avail.x - drawW) * 0.5f),
                            ImGui::GetCursorPosY() + std::max(0.0f, (avail.y - drawH) * 0.5f));
        ImGui::SetCursorPos(cursor);

        const ImVec2 screenPos = ImGui::GetCursorScreenPos();
        ImGui::Image(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(gameTexture)),
                     ImVec2(drawW, drawH));
        gameHovered = ImGui::IsItemHovered();

        gameViewport.x = static_cast<int>(screenPos.x);
        gameViewport.y = static_cast<int>(screenPos.y);
        gameViewport.w = static_cast<int>(drawW);
        gameViewport.h = static_cast<int>(drawH);
    }
    ImGui::End();
    ImGui::PopStyleVar();

    for (const DebugPanel& panel : panels) {
        if (ImGui::Begin(panel.title.c_str())) {
            DrawPanelBody(panel);
        }
        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
}

#endif
