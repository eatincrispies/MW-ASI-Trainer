#include "AIPursuitRequestGroundSupport.hpp"

#include "../dllmain.hpp"

namespace AIPursuitRequestGroundSupport {

    namespace {

        constexpr Memory::Pattern kRequestGroundSupport =
            Memory::ParsePattern("56 8B F1 8A 86 E8 00 00 00 84 C0 0F 85 ?? ?? ?? ?? 8A 86 14 01 00 00 "
                                 "84 C0 0F 84 ?? ?? ?? ?? 8A 86 E9 00 00 00");

        constexpr std::array<std::uint8_t, 3> kNoSupport{ 0x33, 0xC0, 0xC3 };

        std::optional<ScopedPatch> g_patch;

    }

    bool Install() noexcept {
        return Hook::Patch(g_patch, kRequestGroundSupport, 0, kNoSupport);
    }

}
