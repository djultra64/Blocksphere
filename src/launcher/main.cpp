#include "tetrisphere/launcher_backend.h"
#include "tetrisphere/controller_mappings.h"
#include "tetrisphere/linux_platform.h"
#include "launcher_text.h"
#include "steam_artwork.h"

#include "gui/rt64_file_dialog.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include <SDL.h>
#include <imgui.h>

#include <stb_image.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace tetrisphere::launcher {
namespace {

constexpr auto background = ImVec4(22.f/255, 22.f/255, 28.f/255, 1);
constexpr auto text = ImVec4(243.f/255, 241.f/255, 250.f/255, 1);
constexpr auto muted = ImVec4(170.f/255, 167.f/255, 182.f/255, 1);
constexpr auto line = ImVec4(55.f/255, 53.f/255, 63.f/255, 1);
constexpr auto accent = ImVec4(219.f/255, 184.f/255, 1, 1);
constexpr auto accent_ink = ImVec4(34.f/255, 21.f/255, 46.f/255, 1);
constexpr auto success = ImVec4(178.f/255, 216.f/255, 190.f/255, 1);
constexpr auto failure = ImVec4(1, 181.f/255, 167.f/255, 1);

using Surface = std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)>;
using Texture = std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)>;

std::string utf8(const fs::path& path) {
    const auto bytes = path.u8string();
    return {bytes.begin(), bytes.end()};
}

Surface decode_image(std::string_view bytes) {
    Surface image(nullptr, SDL_FreeSurface);
    if (bytes.empty() || bytes.size() > 16 * 1024 * 1024) return image;
    const auto* data = reinterpret_cast<const unsigned char*>(bytes.data());
    const auto size = static_cast<int>(bytes.size());
    int w = 0, h = 0, channels = 0;
    if (!stbi_info_from_memory(data, size, &w, &h, &channels) ||
        w <= 0 || h <= 0 || w > 4096 || h > 4096) return image;
    std::unique_ptr<unsigned char, decltype(&stbi_image_free)> pixels(
        stbi_load_from_memory(data, size, &w, &h, &channels, 4),
        stbi_image_free);
    if (!pixels) return image;
    image.reset(SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32));
    if (image) {
        for (int y = 0; y < h; ++y) {
            std::copy_n(pixels.get() + y * w * 4, w * 4,
                        static_cast<unsigned char*>(image->pixels) + y * image->pitch);
        }
    }
    return image;
}

Surface load_image(const fs::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    const auto size = file.tellg();
    if (size <= 0 || size > 16 * 1024 * 1024) return {nullptr, SDL_FreeSurface};
    std::string bytes(static_cast<std::size_t>(size), '\0');
    file.seekg(0);
    if (!file.read(bytes.data(), size)) return {nullptr, SDL_FreeSurface};
    return decode_image(bytes);
}

Text error_text(Error error) {
    switch (error) {
        case Error::RomUnreadable: return Text::RomRead;
        case Error::RomInvalid: return Text::RomInvalid;
        case Error::RomUnsupported: return Text::RomUnsupported;
        case Error::InvalidDestination: case Error::DestinationNotEmpty: return Text::DestinationError;
        case Error::PreparationFailed: return Text::PrepareError;
        case Error::PreferencesFailed: return Text::PreferenceError;
        case Error::NotInstalled: case Error::LaunchFailed: return Text::PlayError;
        case Error::SteamUnavailable: return Text::NoSteam;
        case Error::SteamGameRunning: return Text::RunningGame;
        case Error::SteamShutdownFailed: return Text::SteamClose;
        case Error::SteamShortcutInvalid: case Error::SteamWriteFailed: return Text::SteamWrite;
        case Error::SteamReopenFailed: return Text::SteamReopen;
        case Error::ArtworkInvalid: return Text::ArtError;
        case Error::ArtworkDownloadFailed: return Text::ArtDownloadFailed;
        default: return Text::InstallError;
    }
}

enum class Screen { Rom, Destination, Install, Ready, Account, Artwork, Adding, Added };

struct ArtworkPreview {
    std::future<std::string> request;
    Texture texture{nullptr, SDL_DestroyTexture};
    ImVec2 size{1, 1};
};

class Launcher {
public:
    Launcher(SDL_Renderer* renderer, fs::path bundle, ImFont* heading, ImFont* identity)
        : renderer_(renderer), bundle_(std::move(bundle)), heading_(heading), identity_(identity),
          preferences_(load_preferences(default_preferences_path())), preview_(nullptr, SDL_DestroyTexture) {
        const auto selected = std::find(language_codes.begin(), language_codes.end(), preferences_.language);
        if (selected != language_codes.end()) language_ = static_cast<unsigned>(selected - language_codes.begin());
        if (installation_ready(bundle_)) preferences_.destination = bundle_;
        if (preferences_.destination.empty()) preferences_.destination = default_installation_path();
        if (installation_ready(preferences_.destination)) screen_ = Screen::Ready;
    }

    ~Launcher() {
        if (install_job_.valid()) install_job_.wait();
        if (rom_job_.valid()) rom_job_.wait();
        if (steam_job_.valid()) steam_job_.wait();
    }

    void handle(const SDL_Event& event) {
        if (event.type == SDL_QUIT) close_requested_ = true;
        if (event.type == SDL_KEYDOWN || event.type == SDL_MOUSEBUTTONDOWN) {
            family_ = ControllerFamily::Keyboard;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE && !event.key.repeat &&
                !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId)) back();
        }
        if (event.type == SDL_CONTROLLERBUTTONDOWN ||
            (event.type == SDL_CONTROLLERAXISMOTION && std::abs(event.caxis.value) > 18000)) {
            const auto id = event.type == SDL_CONTROLLERBUTTONDOWN ? event.cbutton.which : event.caxis.which;
            auto* pad = SDL_GameControllerFromInstanceID(id);
            if (pad) family_ = select_controller_family(SDL_GameControllerName(pad), "auto");
            active_pad_ = id;
            if (event.type == SDL_CONTROLLERBUTTONDOWN && event.cbutton.button == SDL_CONTROLLER_BUTTON_B &&
                !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId)) back();
        }
        if (event.type == SDL_CONTROLLERDEVICEREMOVED && event.cdevice.which == active_pad_) {
            family_ = ControllerFamily::Keyboard;
            active_pad_ = -1;
        }
    }

    bool finished() const { return close_requested_ && !busy(); }

    void draw() {
        poll_jobs();
        const auto size = ImGui::GetIO().DisplaySize;
        const float margin = std::min(42.0f, size.x * 0.045f);
        const float top = 74;
        const float bottom = size.y - 76;
        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize(size);
        ImGui::Begin("Blocksphere##launcher", nullptr, ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);
        ImGui::SetCursorPos({size.x - margin - 132, 24});
        ImGui::SetNextItemWidth(132);
        if (ImGui::BeginCombo("##language", language_names[language_])) {
            for (unsigned i = 0; i < language_names.size(); ++i) {
                if (ImGui::Selectable(language_names[i], i == language_)) set_language(i);
                if (i == language_) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        rule(margin, size.x - margin, top);
        rule(margin, size.x - margin, bottom);

        const bool gallery = screen_ == Screen::Artwork || screen_ == Screen::Adding;
        const float work_x = gallery ? margin : size.x * .51f;
        if (!gallery) {
            const float x = margin + 16;
            const float y = std::max(top + 38, size.y * .40f);
            ImGui::GetWindowDrawList()->AddRectFilled({x, y}, {x + 42, y + 4}, ImColor(accent));
            ImGui::SetCursorPos({x, y + 24});
            ImGui::PushFont(identity_);
            ImGui::TextUnformatted("BLOCKSPHERE");
            ImGui::PopFont();
            ImGui::SetCursorPos({x, y + 88});
            ImGui::TextColored(muted, "%s", t(Text::Subtitle));
        }

        const float work_y = gallery ? top + 24 : std::max(top + 30, size.y * .30f);
        ImGui::SetCursorPos({work_x, work_y});
        ImGui::BeginChild("task", {size.x - margin - work_x, std::max(110.f, bottom - work_y - 10)},
                          false, ImGuiWindowFlags_NavFlattened);
        draw_task();
        if (!result_) {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, failure);
            ImGui::TextWrapped("%s", t(error_text(result_.error)));
            ImGui::PopStyleColor();
            if (!result_.detail.empty()) ImGui::TextWrapped("%s", result_.detail.c_str());
        }
        ImGui::EndChild();
        draw_footer(size.x, margin, bottom + 18);
        ImGui::End();
    }

private:
    const char* t(Text key) const { return translate(key, language_); }
    bool busy() const { return rom_job_.valid() || install_job_.valid() || steam_job_.valid(); }

    void transition(Screen next) {
        screen_ = next;
        result_ = {};
    }

    void set_language(unsigned language) {
        language_ = language;
        preferences_.language = language_codes[language];
        result_ = save_preferences(default_preferences_path(), preferences_);
        if (result_ && screen_ != Screen::Install && installation_ready(preferences_.destination))
            result_ = apply_language(preferences_.destination, preferences_.language);
    }

    void choose_rom() {
        const auto path = RT64::FileDialog::getOpenFilename({{"Tetrisphere ROM / ZIP", "z64,v64,n64,zip"}});
        if (path.empty()) return;
        result_ = {};
        rom_job_ = std::async(std::launch::async, [path] { return validate_rom(path); });
    }

    void begin_install() {
        transition(Screen::Install);
        const auto language = preferences_.language;
        const auto destination = preferences_.destination;
        progress_ = {InstallStage::Copying};
        install_job_ = std::async(std::launch::async, [this, language, destination] {
            return install(bundle_, destination, rom_, language, [this](const InstallProgress& progress) {
                const std::lock_guard lock(progress_lock_);
                progress_ = progress;
            });
        });
    }

    void accounts() {
        accounts_ = discover_steam_accounts();
        selected_account_ = 0;
        transition(Screen::Account);
        if (accounts_.empty()) result_ = {Error::SteamUnavailable, {}};
    }

    fs::path& artwork_path() {
        static constexpr std::array fields{&ArtworkChoice::portrait, &ArtworkChoice::landscape,
            &ArtworkChoice::hero, &ArtworkChoice::logo, &ArtworkChoice::icon};
        return artwork_.*fields[art_category_];
    }

    void reload_preview() {
        auto path = artwork_path();
        if (path.empty() && art_category_ == 4) path = bundle_ / project_profile().icon_relative;
        const auto image = load_image(path);
        preview_size_ = image ? ImVec2(float(image->w), float(image->h)) : ImVec2(1, 1);
        preview_.reset(image ? SDL_CreateTextureFromSurface(renderer_, image.get()) : nullptr);
    }

    void choose_art() {
        const auto selected = RT64::FileDialog::getOpenFilename({{"PNG / JPEG", "png,jpg,jpeg"}});
        if (selected.empty()) return;
        if (!load_image(selected)) {
            result_ = {Error::ArtworkInvalid, {}};
            return;
        }
        artwork_path() = selected;
        online_selection_[art_category_] = -1;
        artwork_override_[art_category_] = true;
        result_ = {};
        reload_preview();
    }

    void open_artwork() {
        transition(Screen::Artwork);
        reload_preview();
        if (!online_previews_.empty()) return;
        for (const auto& asset : steam_artwork()) {
            online_previews_.emplace_back();
            online_previews_.back().request = std::async(std::launch::async, [asset] {
                return download_steam_artwork(asset.thumbnail, true);
            });
        }
    }

    bool artwork_loading() const {
        return std::any_of(online_previews_.begin(), online_previews_.end(),
                           [](const ArtworkPreview& preview) { return preview.request.valid(); });
    }

    void poll_artwork() {
        using namespace std::chrono_literals;
        const auto assets = steam_artwork();
        for (auto& preview : online_previews_) {
            if (!preview.request.valid() || preview.request.wait_for(0s) != std::future_status::ready) continue;
            const auto image = decode_image(preview.request.get());
            if (!image) continue;
            preview.texture.reset(SDL_CreateTextureFromSurface(renderer_, image.get()));
            preview.size = {float(image->w), float(image->h)};
        }
        for (unsigned category = 0; category < online_selection_.size(); ++category) {
            if (artwork_override_[category] || online_selection_[category] >= 0) continue;
            for (std::size_t i = 0; i < online_previews_.size(); ++i) {
                if (assets[i].category != category) continue;
                if (online_previews_[i].request.valid()) break;
                if (online_previews_[i].texture) {
                    online_selection_[category] = static_cast<int>(i);
                    break;
                }
            }
        }
    }

    void begin_steam() {
        transition(Screen::Adding);
        const auto account = accounts_[selected_account_];
        const auto destination = preferences_.destination;
        steam_job_ = std::async(std::launch::async,
            [account, destination, artwork = artwork_, choices = online_selection_]() mutable -> Result {
            std::array<std::future<std::string>, 5> downloads;
            const auto assets = steam_artwork();
            for (std::size_t i = 0; i < choices.size(); ++i) {
                if (choices[i] < 0) continue;
                downloads[i] = std::async(std::launch::async, [path = assets[choices[i]].image] {
                    return download_steam_artwork(path, false);
                });
            }
            for (std::size_t i = 0; i < downloads.size(); ++i) {
                if (!downloads[i].valid()) continue;
                artwork.downloaded[i] = downloads[i].get();
                if (artwork.downloaded[i].empty())
                    return {Error::ArtworkDownloadFailed, std::string(assets[choices[i]].artist)};
            }
            return add_to_steam(account, destination, artwork);
        });
    }

    void poll_jobs() {
        using namespace std::chrono_literals;
        poll_artwork();
        if (rom_job_.valid() && rom_job_.wait_for(0s) == std::future_status::ready) {
            rom_ = rom_job_.get();
            result_ = rom_.result;
        }
        if (install_job_.valid() && install_job_.wait_for(0s) == std::future_status::ready) {
            auto result = install_job_.get();
            transition(result ? Screen::Ready : Screen::Destination);
            result_ = result ? apply_language(preferences_.destination, preferences_.language) : std::move(result);
            if (result_) result_ = save_preferences(default_preferences_path(), preferences_);
        }
        if (steam_job_.valid() && steam_job_.wait_for(0s) == std::future_status::ready) {
            auto result = steam_job_.get();
            transition(result || result.error == Error::SteamReopenFailed ? Screen::Added : Screen::Artwork);
            result_ = std::move(result);
        }
    }

    void back() {
        if (busy()) return;
        if (screen_ == Screen::Destination) transition(Screen::Rom);
        else if (screen_ == Screen::Artwork) transition(Screen::Account);
        else if (screen_ == Screen::Account || screen_ == Screen::Added) transition(Screen::Ready);
        else if (screen_ == Screen::Ready || screen_ == Screen::Rom) close_requested_ = true;
    }

    static void rule(float left, float right, float y) {
        ImGui::GetWindowDrawList()->AddLine({left, y}, {right, y}, ImColor(line));
    }

    void heading(Text title) {
        ImGui::PushFont(heading_);
        ImGui::TextWrapped("%s", t(title));
        ImGui::PopFont();
        ImGui::Spacing();
    }

    void note(Text label) {
        ImGui::PushStyleColor(ImGuiCol_Text, muted);
        ImGui::TextWrapped("%s", t(label));
        ImGui::PopStyleColor();
    }

    static bool button(const char* label, ImVec2 size, bool filled = false, bool enabled = true) {
        if (!enabled) ImGui::BeginDisabled();
        ImGui::PushStyleColor(ImGuiCol_Button, filled ? accent : background);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, filled ? ImVec4(.94f,.83f,1,1) : line);
        if (filled) ImGui::PushStyleColor(ImGuiCol_Text, accent_ink);
        const bool pressed = ImGui::Button(label, size);
        ImGui::PopStyleColor(filled ? 3 : 2);
        if (!enabled) ImGui::EndDisabled();
        return pressed;
    }

    bool draw_artwork_gallery() {
        const auto assets = steam_artwork();
        const bool tall = art_category_ == 0;
        const int columns = tall ? 7 : 4;
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float width = std::min(tall ? 115.f : 216.f,
            (ImGui::GetContentRegionAvail().x - (columns - 1) * spacing) / columns);
        const float height = tall ? width * 1.4f : 100.f;
        int shown = 0;
        int focused = -1;
        for (std::size_t i = 0; i < online_previews_.size(); ++i) {
            const auto& preview = online_previews_[i];
            if (assets[i].category != art_category_ || !preview.texture) continue;
            if (shown++ % columns != 0) ImGui::SameLine();
            ImGui::PushID(static_cast<int>(i));
            if (button("##artwork", {width, height})) {
                artwork_path().clear();
                online_selection_[art_category_] = static_cast<int>(i);
                artwork_override_[art_category_] = true;
                reload_preview();
            }
            if (ImGui::IsItemFocused() || ImGui::IsItemHovered()) focused = static_cast<int>(i);
            const auto start = ImGui::GetItemRectMin();
            const float scale = std::min((width - 8) / preview.size.x, (height - 8) / preview.size.y);
            const ImVec2 top{start.x + (width - preview.size.x * scale) / 2,
                             start.y + (height - preview.size.y * scale) / 2};
            const ImVec2 bottom{top.x + preview.size.x * scale, top.y + preview.size.y * scale};
            auto* draw = ImGui::GetWindowDrawList();
            draw->AddImage(preview.texture.get(), top, bottom);
            if (online_selection_[art_category_] == static_cast<int>(i)) {
                const ImVec2 mark{bottom.x - 20, top.y + 3};
                draw->AddRectFilled(mark, {mark.x + 17, mark.y + 17}, ImColor(accent), 3.f);
                draw->AddLine({mark.x + 4, mark.y + 9}, {mark.x + 7, mark.y + 12}, ImColor(accent_ink), 2.f);
                draw->AddLine({mark.x + 7, mark.y + 12}, {mark.x + 13, mark.y + 5}, ImColor(accent_ink), 2.f);
            }
            ImGui::PopID();
        }
        const auto credit = focused >= 0 ? focused : online_selection_[art_category_];
        if (credit >= 0) {
            const auto& asset = assets[credit];
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(muted, "%s", asset.artist.data());
            ImGui::SameLine();
            if (button("SteamGridDB", {0, 28})) SDL_OpenURL(asset.page.data());
        }
        return shown != 0;
    }

    void draw_task() {
        const float width = ImGui::GetContentRegionAvail().x;
        switch (screen_) {
        case Screen::Rom: {
            heading(Text::RomTitle);
            note(Text::RomHelp);
            ImGui::Dummy({0, 10});
            if (button(t(rom_.bytes.empty() ? Text::SelectRom : Text::Change), {width, 48}, false, !busy())) choose_rom();
            const auto first = ImGui::GetItemRectMin();
            const auto last = ImGui::GetItemRectMax();
            ImGui::GetWindowDrawList()->AddLine({first.x, last.y}, last, ImColor(line));
            ImGui::SetItemDefaultFocus();
            if (rom_job_.valid()) ImGui::ProgressBar(-static_cast<float>(ImGui::GetTime()), {width, 6}, "");
            else if (!rom_.bytes.empty()) {
                ImGui::TextColored(success, "%s", t(Text::Compatible));
                ImGui::TextWrapped("%s", utf8(rom_.path.filename()).c_str());
            }
            break;
        }
        case Screen::Destination:
            heading(Text::Destination);
            ImGui::TextWrapped("%s", utf8(preferences_.destination).c_str());
            if (button(t(Text::Browse), {width, 44})) {
                const auto selected = RT64::FileDialog::getDirectoryPath();
                if (!selected.empty()) preferences_.destination = selected;
            }
            ImGui::SetItemDefaultFocus();
            break;
        case Screen::Install: {
            heading(Text::Installing);
            InstallProgress progress;
            { const std::lock_guard lock(progress_lock_); progress = progress_; }
            const auto stage = progress.stage == InstallStage::Copying ? Text::Copying :
                progress.stage == InstallStage::Preparing ? Text::Preparing : Text::Verifying;
            note(stage);
            const bool measured = progress.stage == InstallStage::Copying && progress.total_bytes > 0;
            ImGui::ProgressBar(measured ? float(progress.completed_bytes) / float(progress.total_bytes)
                                       : -static_cast<float>(ImGui::GetTime()), {width, 9}, measured ? nullptr : "");
            break;
        }
        case Screen::Ready:
            ImGui::TextColored(success, "%s", t(Text::Installed));
            ImGui::PushFont(heading_);
            ImGui::PushStyleColor(ImGuiCol_Text, accent);
            if (button(t(Text::Play), {width, 58})) result_ = play(preferences_.destination, preferences_.language);
            ImGui::PopStyleColor();
            ImGui::PopFont();
            ImGui::SetItemDefaultFocus();
            if (button(t(Text::AddSteam), {width, 45})) accounts();
            if (button(t(Text::Exit), {width, 45})) close_requested_ = true;
            break;
        case Screen::Account:
            heading(Text::Account);
            note(Text::AccountHelp);
            for (std::size_t i = 0; i < accounts_.size(); ++i) {
                ImGui::PushID(static_cast<int>(i));
                const auto label = accounts_[i].name + "  ·  " + accounts_[i].account_id;
                if (ImGui::Selectable(label.c_str(), i == selected_account_, 0, {0, 40})) selected_account_ = i;
                if (i == selected_account_) ImGui::SetItemDefaultFocus();
                ImGui::PopID();
            }
            break;
        case Screen::Artwork: {
            heading(Text::Artwork);
            constexpr std::array labels{Text::Cover, Text::Wide, Text::Hero, Text::Logo, Text::Icon};
            for (unsigned i = 0; i < labels.size(); ++i) {
                if (i) ImGui::SameLine();
                if (button(t(labels[i]), {0, 35}, i == art_category_)) {
                    art_category_ = i;
                    reload_preview();
                }
            }
            ImGui::Spacing();
            if (!draw_artwork_gallery()) note(artwork_loading() ? Text::ArtLoading : Text::ArtUnavailable);
            if (preview_ && online_selection_[art_category_] < 0) {
                const float scale = std::min(210.f / preview_size_.x, 126.f / preview_size_.y);
                ImGui::Image(preview_.get(), {preview_size_.x * scale, preview_size_.y * scale},
                             {0, 0}, {1, 1}, {1, 1, 1, 1}, accent);
                ImGui::SameLine();
            }
            if (button(t(Text::LocalArt), {0, 42})) choose_art();
            ImGui::SameLine();
            if (button(t(Text::KeepArt), {0, 42}, artwork_path().empty() && online_selection_[art_category_] < 0)) {
                artwork_path().clear();
                online_selection_[art_category_] = -1;
                artwork_override_[art_category_] = true;
                reload_preview();
            }
            if (!artwork_path().empty()) ImGui::TextWrapped("%s", utf8(artwork_path().filename()).c_str());
            ImGui::Spacing();
            note(Text::SteamNotice);
            break;
        }
        case Screen::Adding:
            heading(Text::AddSteam);
            note(Text::SteamNotice);
            ImGui::ProgressBar(-static_cast<float>(ImGui::GetTime()), {width, 9}, "");
            break;
        case Screen::Added:
            heading(Text::Added);
            if (selected_account_ < accounts_.size()) ImGui::TextUnformatted(accounts_[selected_account_].name.c_str());
            break;
        }
    }

    void draw_footer(float width, float margin, float y) {
        ImGui::SetCursorPos({margin, y});
        if (button("by djultra64", {0, 28})) SDL_OpenURL("https://github.com/djultra64");
        ImGui::SameLine();
        ImGui::TextColored(muted, "v%s", BLOCKSPHERE_VERSION);
        ImGui::SameLine(0, 22);
        const char* confirm = family_ == ControllerFamily::Keyboard ? "Enter" : family_ == ControllerFamily::PlayStation ? "×" : family_ == ControllerFamily::NintendoSwitch ? "B" : "A";
        const char* cancel = family_ == ControllerFamily::Keyboard ? "Esc" : family_ == ControllerFamily::PlayStation ? "○" : family_ == ControllerFamily::NintendoSwitch ? "A" : "B";
        ImGui::TextColored(muted, "%s %s   %s %s", confirm, t(Text::Select), cancel, t(Text::Back));

        std::optional<Text> next;
        if (screen_ == Screen::Rom || screen_ == Screen::Account) next = Text::Continue;
        else if (screen_ == Screen::Destination) next = result_ ? Text::Install : Text::Retry;
        else if (screen_ == Screen::Artwork) next = Text::Add;
        else if (screen_ == Screen::Added) next = Text::Done;
        if (!next) return;
        const bool show_back = screen_ == Screen::Destination || screen_ == Screen::Account || screen_ == Screen::Artwork;
        const float next_width = std::max(130.f, ImGui::CalcTextSize(t(*next)).x + 28);
        const float back_width = ImGui::CalcTextSize(t(Text::Back)).x + 20;
        ImGui::SetCursorPos({width - margin - next_width - (show_back ? back_width + 12 : 0), y});
        if (show_back) {
            if (button(t(Text::Back), {back_width, 36})) { back(); return; }
            ImGui::SameLine(0, 12);
        }
        const bool enabled = !busy() && (screen_ != Screen::Rom || !rom_.bytes.empty()) &&
                             (screen_ != Screen::Account || !accounts_.empty()) &&
                             (screen_ != Screen::Artwork || !artwork_loading());
        if (!button(t(*next), {next_width, 36}, true, enabled)) return;
        switch (screen_) {
            case Screen::Rom: transition(Screen::Destination); break;
            case Screen::Destination: begin_install(); break;
            case Screen::Account: open_artwork(); break;
            case Screen::Artwork: begin_steam(); break;
            case Screen::Added: transition(Screen::Ready); break;
            default: break;
        }
    }

    SDL_Renderer* renderer_;
    fs::path bundle_;
    ImFont* heading_;
    ImFont* identity_;
    Preferences preferences_;
    Screen screen_ = Screen::Rom;
    unsigned language_ = 0;
    Result result_;
    RomSelection rom_;
    std::future<RomSelection> rom_job_;
    std::future<Result> install_job_;
    std::future<Result> steam_job_;
    std::mutex progress_lock_;
    InstallProgress progress_{InstallStage::Copying};
    std::vector<SteamAccount> accounts_;
    std::size_t selected_account_ = 0;
    ArtworkChoice artwork_;
    std::vector<ArtworkPreview> online_previews_;
    std::array<int, 5> online_selection_{-1, -1, -1, -1, -1};
    std::array<bool, 5> artwork_override_{};
    unsigned art_category_ = 0;
    Texture preview_;
    ImVec2 preview_size_{1, 1};
    ControllerFamily family_ = ControllerFamily::Keyboard;
    SDL_JoystickID active_pad_ = -1;
    bool close_requested_ = false;
};

int run() {
    SDL_SetHint("SDL_APP_NAME", "Blocksphere");
    SDL_SetHint("SDL_GAMECONTROLLER_USE_BUTTON_LABELS", "0");
    initialize_controller_mappings();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) throw std::runtime_error(SDL_GetError());
    std::unique_ptr<char, decltype(&SDL_free)> base(SDL_GetBasePath(), SDL_free);
    if (!base) throw std::runtime_error(SDL_GetError());
    const fs::path bundle = fs::u8path(base.get());
    std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> window(
        SDL_CreateWindow("Blocksphere", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1000, 650,
                         SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI), SDL_DestroyWindow);
    if (!window) throw std::runtime_error(SDL_GetError());
    SDL_SetWindowMinimumSize(window.get(), 820, 560);
    const auto icon = load_image(bundle / project_profile().icon_relative);
    if (icon) SDL_SetWindowIcon(window.get(), icon.get());
    std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)> renderer(
        SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC), SDL_DestroyRenderer);
    if (!renderer) renderer.reset(SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_SOFTWARE));
    if (!renderer) throw std::runtime_error(SDL_GetError());

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    const auto font = utf8(bundle / "launcher-assets/Inter.ttf");
    ImFontGlyphRangesBuilder glyphs;
    glyphs.AddRanges(io.Fonts->GetGlyphRangesDefault());
    glyphs.AddText("…’–·×○");
    ImVector<ImWchar> font_ranges;
    glyphs.BuildRanges(&font_ranges);
    io.FontDefault = io.Fonts->AddFontFromFileTTF(font.c_str(), 16, nullptr, font_ranges.Data);
    ImFont* heading = io.Fonts->AddFontFromFileTTF(font.c_str(), 29, nullptr, font_ranges.Data);
    ImFont* identity = io.Fonts->AddFontFromFileTTF(font.c_str(), 46, nullptr, font_ranges.Data);
    auto& style = ImGui::GetStyle();
    style.WindowPadding = {0, 0};
    style.FramePadding = {12, 8};
    style.FrameRounding = 5;
    style.ItemSpacing = {10, 10};
    style.ButtonTextAlign = {0.0f, .5f};
    style.Colors[ImGuiCol_WindowBg] = background;
    style.Colors[ImGuiCol_PopupBg] = background;
    style.Colors[ImGuiCol_Text] = text;
    style.Colors[ImGuiCol_TextDisabled] = muted;
    style.Colors[ImGuiCol_Border] = line;
    style.Colors[ImGuiCol_FrameBg] = line;
    style.Colors[ImGuiCol_Button] = line;
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(.30f, .26f, .35f, 1);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(.37f, .30f, .44f, 1);
    style.Colors[ImGuiCol_Header] = line;
    style.Colors[ImGuiCol_HeaderHovered] = line;
    style.Colors[ImGuiCol_NavHighlight] = accent;
    style.Colors[ImGuiCol_PlotHistogram] = accent;
    ImGui_ImplSDL2_InitForSDLRenderer(window.get(), renderer.get());
    ImGui_ImplSDL2_SetGamepadMode(ImGui_ImplSDL2_GamepadMode_AutoAll);
    ImGui_ImplSDLRenderer2_Init(renderer.get());
    RT64::FileDialog::initialize();
    {
        Launcher launcher(renderer.get(), bundle, heading, identity);
        while (!launcher.finished()) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                ImGui_ImplSDL2_ProcessEvent(&event);
                launcher.handle(event);
            }
            ImGui_ImplSDLRenderer2_NewFrame();
            ImGui_ImplSDL2_NewFrame();
            ImGui::NewFrame();
            launcher.draw();
            ImGui::Render();
            SDL_SetRenderDrawColor(renderer.get(), 22, 22, 28, 255);
            SDL_RenderClear(renderer.get());
            ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
            SDL_RenderPresent(renderer.get());
            SDL_Delay(8);
        }
    }
    RT64::FileDialog::finish();
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    renderer.reset();
    window.reset();
    SDL_Quit();
    return 0;
}

} // namespace
} // namespace tetrisphere::launcher

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--version") {
        std::cout << "Blocksphere " << BLOCKSPHERE_VERSION << '\n';
        return 0;
    }
    try {
        return tetrisphere::launcher::run();
    } catch (const std::exception& error) {
        std::cerr << "Blocksphere launcher: " << error.what() << '\n';
        SDL_Quit();
        return 1;
    }
}
