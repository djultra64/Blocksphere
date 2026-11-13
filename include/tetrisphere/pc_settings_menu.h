#pragma once
#include "tetrisphere/settings_store.h"
#include <optional>
#include <string>
#include <vector>
namespace tetrisphere {
inline constexpr int pc_graphics_apply_field = 12;
Settings merge_pc_settings_change(const Settings& persisted,const Settings& requested,int field);
// Bridge-lock protected model; persistence and drawing receive value copies.
class PCSettingsMenu {
public:
    enum GraphicsRow { Resolution, Aspect, FrameRate, Window, Msaa, PresentFilter,
                       TextureFilter, Apply, BackOrDiscard, KeepEditing };
    explicit PCSettingsMenu(Settings settings, bool graphics_only = false)
      : page_(graphics_only ? Page::Graphics : Page::Root),
        applied_(settings), settings_(std::move(settings)), graphics_only_(graphics_only) {}
    std::vector<std::string> rows(ControllerFamily family) const;
    void select(int row);
    void back();
    void move(int direction);
    void adjust(int direction);
    int focused() const {return focused_;}
    const Settings& settings() const {return settings_;}
    std::string value(int field) const;
    bool restart_required(int field) const;
    bool dirty() const;
    bool restart_changes() const;
    bool exit_pending() const { return exit_pending_; }
    bool saving() const {return saving_;}
    const std::string& status() const {return status_;}
    bool closed() const { return page_ == Page::Closed; }
    std::optional<Settings> take_change();
    void update_settings(const Settings& settings);
    int changed_field() const { return pc_graphics_apply_field; }
    bool take_controls_request() {bool value=controls_request_;controls_request_=false;return value;}
private:
    enum class Page { Root, Graphics, Closed };
    void apply();
    void leave_graphics();
    void keep_editing();
    int focused_=0;
    bool saving_=false;
    std::string status_;
    Page page_ = Page::Root;
    Settings applied_, settings_;
    int previous_focus_ = 0;
    bool exit_pending_ = false;
    std::optional<Settings> change_;
    bool controls_request_=false;
    bool graphics_only_=false;
};
}
