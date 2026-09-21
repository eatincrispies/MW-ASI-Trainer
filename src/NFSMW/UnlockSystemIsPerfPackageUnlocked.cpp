#include "UnlockSystemIsPerfPackageUnlocked.hpp"

#include "../dllmain.hpp"

namespace UnlockSystemIsPerfPackageUnlocked {

    namespace {

        constexpr Memory::Pattern kIsPerfPackageUnlocked =
            Memory::ParsePattern("51 A0 ?? ?? ?? ?? 84 C0 74 04 B0 01 59 C3 53 8B 5C 24 0C F6 C3 01 "
                                 "55 8B 6C 24 20 56 8B 74 24 1C 57 8B 7C 24 1C C6 44 24 13 00 74 15 "
                                 "8B 44 24 24 55 50 56 57 53 E8 E6 F6 FF FF");

        constexpr std::array<std::uint8_t, 3> kUnlocked{ 0xB0, 0x01, 0xC3 };

        std::optional<ScopedPatch> g_patch;

    }

    bool Install() noexcept {
        return Hook::Patch(g_patch, kIsPerfPackageUnlocked, 0, kUnlocked);
    }

}
