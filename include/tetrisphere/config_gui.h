#pragma once

#include "tetrisphere/settings_store.h"

#include <functional>
#include <string>
#include <vector>

namespace tetrisphere {

// Return the selected button index, or -1 when the dialog is dismissed.
using ConfigGuiDialog = std::function<int(
    const std::string& title, const std::string& message,
    const std::vector<std::string>& buttons)>;

int run_graphics_config_gui(const SettingsStore& store, const ConfigGuiDialog& dialog);

} // namespace tetrisphere
