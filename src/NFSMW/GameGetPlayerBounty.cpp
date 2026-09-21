#include "GameGetPlayerBounty.hpp"

#include "../dllmain.hpp"

namespace GameGetPlayerBounty {

    namespace {

        constexpr Memory::Pattern kGetPlayerBounty =
            Memory::ParsePattern("8B 0D ?? ?? ?? ?? 85 C9 56 74 5A A1 ?? ?? ?? ?? 8B 10 85 D2 74 4F "
                                 "85 C9 74 04 8B CA EB 02 33 C9 8B 01 FF 50 04 8B F0 85 F6 74 3A "
                                 "8B 16 8B CE FF 52 24 84 C0 74 2F");

        constexpr std::array<std::uint8_t, 6> kEnoughBounty{ 0xB8, 0x00, 0xE1, 0xF5, 0x05, 0xC3 };

        std::optional<ScopedPatch> g_patch;

    }

    bool Install() noexcept {
        return Hook::Patch(g_patch, kGetPlayerBounty, 0, kEnoughBounty);
    }

    void Remove() noexcept {
        g_patch.reset();
    }

}
