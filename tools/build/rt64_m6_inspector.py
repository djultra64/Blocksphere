"""Fail-closed RT64 inspector copy patch for the pinned renderer revision."""

from __future__ import annotations

import hashlib


APPLICATION_SHA256 = "c46696042709053654b502c76132cdeb81f52f8393a79f66f9b2b2384134a826"
STATE_SHA256 = "98464b469c2bdf7bb13e17bb204d41476250a23d82bba332a020e7962279ebc8"


def _checked(raw: bytes, expected: str, name: str) -> str:
    actual = hashlib.sha256(raw).hexdigest()
    if actual != expected:
        raise ValueError(f"RT64 {name} SHA256 mismatch: {actual}")
    return raw.decode("utf-8")


def _replace_once(source: str, before: str, after: str) -> str:
    count = source.count(before)
    if count != 1:
        raise ValueError(f"RT64 inspector anchor count {count}, expected 1: {before[:70]!r}")
    return source.replace(before, after, 1)


def patch_application(raw: bytes) -> bytes:
    source = _checked(raw, APPLICATION_SHA256, "rt64_application.cpp")
    source = _replace_once(
        source, "namespace RT64 {\n",
        'extern "C" void tetrisphere_rt64_clear_keyboard_input();\n\nnamespace RT64 {\n',
    )
    start = source.index("    bool Application::sdlEventFilter(SDL_Event *event) {")
    end = source.index("    bool Application::usesWindowMessageFilter()", start)
    original = source[start:end]
    if original.count("processDeveloperShortcut(DeveloperShortcut::Inspector)") != 1:
        raise ValueError("RT64 SDL inspector shortcut changed")
    replacement = '''    bool Application::sdlEventFilter(SDL_Event *event) {
        static bool f1Down = false;
        static bool altEnterDown = false;
        if ((event->type == SDL_WINDOWEVENT) &&
            (event->window.event == SDL_WINDOWEVENT_FOCUS_LOST)) {
            f1Down = false;
            altEnterDown = false;
            tetrisphere_rt64_clear_keyboard_input();
        }
        if ((event->type == SDL_KEYDOWN) || (event->type == SDL_KEYUP)) {
            const bool down = (event->type == SDL_KEYDOWN);
            const SDL_Scancode key = event->key.keysym.scancode;
            if (event->key.keysym.scancode == SDL_SCANCODE_F1) {
                if (down && (event->key.repeat == 0) && !f1Down) {
                    f1Down = true;
                    if (userConfig.developerMode && (presentQueue != nullptr) && (state != nullptr)) {
                        processDeveloperShortcut(DeveloperShortcut::Inspector);
                        tetrisphere_rt64_clear_keyboard_input();
                    }
                }
                if (!down) f1Down = false;
                return true;
            }
            // Host fullscreen shortcuts must reach SDL_PollEvent even when ImGui has focus.
            if (key == SDL_SCANCODE_F11) return false;
            if ((key == SDL_SCANCODE_RETURN) || (key == SDL_SCANCODE_KP_ENTER)) {
                if ((down && ((event->key.keysym.mod & KMOD_ALT) != 0)) || altEnterDown) {
                    altEnterDown = down;
                    return false;
                }
            }
        }
        if (userConfig.developerMode && (presentQueue != nullptr) &&
            (state != nullptr) && !FileDialog::isOpen) {
            const std::lock_guard lock(presentQueue->inspectorMutex);
            if ((presentQueue->inspector != nullptr) &&
                presentQueue->inspector->handleSdlEvent(event)) {
                if ((event->type == SDL_KEYDOWN) || (event->type == SDL_KEYUP))
                    tetrisphere_rt64_clear_keyboard_input();
                return true;
            }
        }
        return false;
    }

'''
    source = source[:start] + replacement + source[end:]
    return source.encode("utf-8")


def patch_state(raw: bytes) -> bytes:
    source = _checked(raw, STATE_SHA256, "rt64_state.cpp")
    source = _replace_once(source, "#include <cinttypes>\n",
                           "#include <cinttypes>\n#include <cstdio>\n")
    source = _replace_once(source, '        if (ImGui::Begin("Game editor")) {',
        '        ImGui::SetNextWindowSize(ImVec2(430, 330), ImGuiCond_Always);\n'
        '        if (ImGui::Begin("Game editor")) {')
    start = source.index('                    ImGui::Text("User Configuration (persistent)");')
    end = source.index("                    ImGui::EndTabItem();", start)
    removed = source[start:end]
    for required in ('"Resolution Mode"', '"Filtering"', '"Three-Point Filtering"',
                     '"Upscale 2D Mode"', '"Render to RAM"'):
        if required not in removed:
            raise ValueError(f"RT64 inspector setting anchor missing: {required}")
    replacement = '''                    ImGui::TextWrapped("Tetrisphere graphics: session only; changes are not saved.");
                    ImGui::TextWrapped("Presentation filtering affects the final image. Texture filtering affects N64 textures.");
                    ImGui::TextUnformatted("Presentation Filtering");
                    ImGui::SetNextItemWidth(-1);
                    genConfigChanged = ImGui::Combo("##presentation_filter",
                        reinterpret_cast<int *>(&userConfig.filtering),
                        "Nearest\\0Linear\\0Pixel Scaling\\0") || genConfigChanged;
                    genConfigChanged = ImGui::Checkbox("Three-Point Texture Filtering",
                        &userConfig.threePointFiltering) || genConfigChanged;
                    ImGui::Text("Effective MSAA: %ux", userConfig.msaaSampleCount());
                    ImGui::Separator();
                    ImGui::TextWrapped("Resolution, aspect, MSAA, 2D upscaling, refresh rate and framebuffer controls use startup settings. Restart to change them.");

'''
    source = source[:start] + replacement + source[end:]
    hidden_tabs = ("Textures", "Script", "Scenes", "Lights", "Materials",
                   "Draw calls", "Game", "Render")
    for tab in hidden_tabs:
        before = f'if (ImGui::BeginTabItem("{tab}"'
        after = f'if (false && ImGui::BeginTabItem("{tab}"'
        source = _replace_once(source, before, after)
    source = _replace_once(source, "            configurationSaveQueued = true;",
                           "            configurationSaveQueued = false; // Session only in Tetrisphere.")
    source = _replace_once(
        source,
        "            ext.sharedQueueResources->setUserConfig(userConfig, resConfigChanged);",
        '''            ext.sharedQueueResources->setUserConfig(userConfig, resConfigChanged);
            std::fprintf(stdout,
                "{\\"event\\":\\"rt64_inspector_graphics_changed\\",\\"presentation_filter\\":%d,"
                "\\"three_point_texture_filter\\":%s}\\n",
                static_cast<int>(userConfig.filtering),
                userConfig.threePointFiltering ? "true" : "false");
            std::fflush(stdout);''',
    )
    return source.encode("utf-8")
