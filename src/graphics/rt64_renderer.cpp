#include "tetrisphere/microcode.h"

#include <algorithm>
#include <utility>

namespace tetrisphere {

GraphicsTaskEvidence inspect_rt64_commands(
        std::string identity, std::span<const MicrocodeCommand> commands,
        std::span<const std::uint8_t> supported_opcodes) {
    GraphicsTaskEvidence result;
    result.identity = std::move(identity);
    result.command_count = commands.size();
    for (const auto& command : commands) {
        if (std::find(supported_opcodes.begin(), supported_opcodes.end(),
                      command.opcode) == supported_opcodes.end()) {
            result.unsupported.push_back(command);
        }
    }
    result.accepted = result.identity == "F3D_SM64_FINAL" &&
                      !commands.empty() && result.unsupported.empty();
    return result;
}

} // namespace tetrisphere
