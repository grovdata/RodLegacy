#include "debug_ui.h"

#ifdef RODLEGACY_DEBUG_UI

#include "log.h"

#include "imgui.h"
// DockBuilder* is not in the public header.
#include "imgui_internal.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_sdlrenderer2.h"

#include <algorithm>
#include <cstdint>

namespace {
// The one live instance, so DebugUiWantsKeyboard() can be called from code that
// has no handle on it.
DebugUi* gDebugUi = nullptr;
}  // namespace

bool DebugUiWantsKeyboard() {
    return gDebugUi != nullptr && gDebugUi->WantsKeyboard();
}

namespace {
// Panel body: the key/value pairs as a two column table, then any loose lines.
void DrawPanelBody(const DebugPanel& panel) {
    for (const DebugToggle& toggle : panel.toggles) {
        if (toggle.value != nullptr) {
            ImGui::Checkbox(toggle.label.c_str(), toggle.value);
        }
    }
    if (!panel.toggles.empty() && !panel.rows.empty()) {
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
    for (const std::string& line : panel.lines) {
        ImGui::TextUnformatted(line.c_str());
    }
}
}  // namespace

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

    // The game draws into this at its native resolution; the dockspace scales it
    // for display, so it stays pixel-exact regardless of window size.
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
    // Panel positions and dock layout persist between runs.
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
            // Clicks inside the game image belong to the game, not the UI.
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

    // A full-window host holding the dockspace, so panels can be dragged
    // anywhere in the window and the game sits in a dock node like any panel.
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

    // Build the default layout only when there is nothing to restore, so a saved
    // debug_ui.ini always wins. Windows are docked by name, which works even for
    // panels that do not exist yet (Racing only appears in the driving scene).
    if (ImGui::DockBuilderGetNode(dockspaceId) == nullptr) {
        // Three columns: global info left, the game in the middle, scene info
        // right. Each split takes from the shrinking centre, so the game keeps
        // whatever is left over.
        ImGui::DockBuilderRemoveNode(dockspaceId);
        ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceId, viewport->WorkSize);

        ImGuiID centreId = dockspaceId;
        const ImGuiID leftId =
            ImGui::DockBuilderSplitNode(centreId, ImGuiDir_Left, 0.22f, nullptr, &centreId);
        const ImGuiID rightId =
            ImGui::DockBuilderSplitNode(centreId, ImGuiDir_Right, 0.28f, nullptr, &centreId);

        ImGui::DockBuilderDockWindow("Game", centreId);

        // Stack each column's panels into their own nodes rather than docking
        // them all to one, which would collapse them into tabs.
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

        stackColumn(leftId, {"Session"});
        stackColumn(rightId, {"Scene", "Racing"});
        ImGui::DockBuilderFinish(dockspaceId);
    }
    ImGui::End();

    // The game viewport. Scaled to the largest whole multiple that fits so the
    // pixel art stays sharp, and centred in whatever space is left.
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

#endif  // RODLEGACY_DEBUG_UI
