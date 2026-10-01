#pragma once

#include <cctype>
#include <cstdlib>
#include <string>

namespace tetrisphere {

inline const char* run_id() {
    static const std::string value = [] {
        const char* environment = std::getenv("TETRISPHERE_RUN_ID");
        if (environment == nullptr) return std::string{"untracked"};
        std::string candidate{environment};
        if (candidate.empty() || candidate.size() > 128) return std::string{"invalid-run-id"};
        for (const unsigned char character : candidate) {
            if (!std::isalnum(character) && character != '-' && character != '_' &&
                character != '.') {
                return std::string{"invalid-run-id"};
            }
        }
        return candidate;
    }();
    return value.c_str();
}

} // namespace tetrisphere
