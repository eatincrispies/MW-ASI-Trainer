#include "AIPursuitRequestRoadBlock.hpp"

#include "../dllmain.hpp"

namespace AIPursuitRequestRoadBlock {

    namespace {

        constexpr Memory::Pattern kRequestRoadBlock =
            Memory::ParsePattern("51 56 8B F1 8A 86 E8 00 00 00 84 C0 0F 85 ?? ?? ?? ?? 8A 86 E9 00 00 00 "
                                 "84 C0 0F 85 ?? ?? ?? ?? 8B 86 84 00 00 00 85 C0");

        constexpr std::array<std::uint8_t, 3> kNoRoadBlock{ 0x33, 0xC0, 0xC3 };

        std::optional<ScopedPatch> g_patch;

    }

    bool Install() noexcept {
        return Hook::Patch(g_patch, kRequestRoadBlock, 0, kNoRoadBlock);
    }

}
