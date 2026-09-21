#include "FEngHudDetermineHudFeatures.hpp"

#include "../dllmain.hpp"

namespace FEngHudDetermineHudFeatures {

    namespace {

        constexpr Memory::Pattern kDetermineHudFeatures =
            Memory::ParsePattern("53 55 56 8B D9 8B 4C 24 10 8B 01 57 33 FF 33 F6 FF 50 2C 6B C0 70 "
                                 "05 ?? ?? ?? ?? 0F 84");

        constexpr std::array<std::uint8_t, 7> kNoHud{ 0x33, 0xC0, 0x33, 0xD2, 0xC2, 0x04, 0x00 };

        std::optional<ScopedPatch> g_patch;

    }

    bool Install() noexcept {
        return Hook::Patch(g_patch, kDetermineHudFeatures, 0, kNoHud);
    }

}
