#include "tetrisphere/menu_language.h"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>

int main() {
    using namespace tetrisphere;
    const auto directory = std::filesystem::temp_directory_path() /
        ("blocksphere-language-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(directory);
    struct RemoveDirectory {
        std::filesystem::path path;
        ~RemoveDirectory() { std::filesystem::remove_all(path); }
    } cleanup{directory};

    assert(read_menu_language(directory) == MenuLanguage::English);
    const auto preference = directory / "language.txt";
    for (const auto& [code, language] : menu_languages) {
        std::ofstream(preference, std::ios::binary) << code << "\r\n";
        assert(read_menu_language(directory) == language);
    }
    assert(read_menu_language(directory, "de") == MenuLanguage::German);
    assert(read_menu_language(directory, "ru") == MenuLanguage::Portuguese);
    std::ofstream(preference, std::ios::binary) << "\xEF\xBB\xBF" "es\n";
    assert(read_menu_language(directory) == MenuLanguage::Spanish);
    std::ofstream(preference, std::ios::binary) << "es\nfr\n";
    assert(read_menu_language(directory) == MenuLanguage::English);
    std::ofstream(preference, std::ios::binary) << "pt" << std::string(80, ' ');
    assert(read_menu_language(directory) == MenuLanguage::English);

    assert(menu_translation("GRAPHICS", MenuLanguage::Spanish) == "GRAFICOS");
    assert(menu_translation("CLOSE GAME", MenuLanguage::German) == "BEENDEN");
    assert(native_menu_translation(0x800ECBA4u, MenuLanguage::Italian) == "PUZZLE");
    assert(native_menu_translation(0x800EC9A0u, MenuLanguage::Portuguese) == "RESGATE");
    assert(native_menu_translation(0x800EC9A4u, MenuLanguage::Portuguese).empty());
    assert(native_menu_translation(0x800EC9A0u, MenuLanguage::English).empty());

    for (const auto& [code, language] : menu_languages) {
        (void)code;
        set_menu_language(language);
        assert(menu_translation("120 FPS") == "120 FPS");
        assert(menu_translation("XBOX") == "XBOX");
        for (const auto key : {"GRAPHICS", "APPLY", "KEEP EDITING", "CLOSE GAME",
                               "FRAME RATE", "UNLIMITED", "PRESS BUTTON"}) {
            const auto text = menu_translation(key);
            assert(!text.empty());
            for (const unsigned char character : text) {
                assert(character >= 32 && character <= 90);
            }
        }
    }
}
