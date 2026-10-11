#pragma once
#include "tetrisphere/settings_store.h"
#include <array>
namespace tetrisphere {
// Called under the bridge mutex. Captures and Apply always operate on a staged copy.
class ControlsEditor {
public:
 enum Row { Layout, Drop, Slide, RestartPuzzle, Magic, Defaults, Discard, Apply, RowCount };
 ControlsEditor(Settings settings, ControllerFamily device);
 void focus(int row);
 void move(int direction);
 void select();
 void back();
 void cycle_family(int direction);
 void set_device(ControllerFamily device);
 bool capture(MagicControl control);
 std::optional<Settings> take_apply();
 void complete_apply(bool success);
 bool dirty() const;
 bool capturing() const {return capturing_;}
 bool saving() const {return saving_;}
 bool closed() const {return closed_;}
 bool exit_pending() const {return exit_pending_;}
 int focused() const {return focused_;}
 ControllerFamily display_family() const;
 ControllerFamily navigation_family() const;
 std::array<std::string,4> labels(ControllerFamily family) const;
 const Settings& pending() const {return pending_;}
 const Settings& applied() const {return applied_;}
 const std::string& status() const {return status_;}
private:
 Settings applied_,pending_;
 ControllerFamily device_,navigation_device_;
 int focused_=1;
 bool capturing_=false,saving_=false,closed_=false,exit_pending_=false;
 std::optional<Settings> request_;
 std::string status_;
};
}
