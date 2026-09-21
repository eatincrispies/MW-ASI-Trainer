#include "BinGetNumChallengesPassed.hpp"

#include "../dllmain.hpp"

namespace BinGetNumChallengesPassed {

    namespace {

        constexpr Memory::Pattern kGetNumChallengesPassed =
            Memory::ParsePattern("8B 44 24 04 8B 0D ?? ?? ?? ?? 50 E8 ?? ?? ?? ?? 85 C0 74 07 8B C8 E9 95 87 FD FF "
                                 "33 C0 C3");

        constexpr std::array<std::uint8_t, 6> kEveryChallenge{ 0xB8, 0xE8, 0x03, 0x00, 0x00, 0xC3 };

        std::optional<ScopedPatch> g_patch;

    }

    bool Install() noexcept {
        return Hook::Patch(g_patch, kGetNumChallengesPassed, 0, kEveryChallenge);
    }

    void Remove() noexcept {
        g_patch.reset();
    }

}
