#include "UnlockSystemIsCarUnlocked.hpp"

#include "../dllmain.hpp"

namespace UnlockSystemIsCarUnlocked {

    namespace {

        constexpr Memory::Pattern kIsCarUnlocked =
            Memory::ParsePattern("51 A0 ?? ?? ?? ?? 84 C0 74 04 B0 01 59 C3 53 8B 5C 24 0C F6 C3 01 "
                                 "56 8B 74 24 14 C6 44 24 0B 00 74 13 8B 44 24 18 50 56 53 E8");

        constexpr std::array<std::uint8_t, 3> kUnlocked{ 0xB0, 0x01, 0xC3 };

        std::optional<ScopedPatch> g_patch;

    }

    bool Install() noexcept {
        return Hook::Patch(g_patch, kIsCarUnlocked, 0, kUnlocked);
    }

}
