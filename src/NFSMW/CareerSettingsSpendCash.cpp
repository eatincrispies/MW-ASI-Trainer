#include "CareerSettingsSpendCash.hpp"

#include "../dllmain.hpp"

namespace CareerSettingsSpendCash {

    namespace {

        constexpr Memory::Pattern kSpendCash =
            Memory::ParsePattern("8B 41 0C 8B 54 24 04 3B D0 76 0A C7 41 0C 00 00 00 00 C2 04 00 "
                                 "2B C2 89 41 0C C2 04 00");

        constexpr std::array<std::uint8_t, 3> kKeepCash{ 0xC2, 0x04, 0x00 };

        std::optional<ScopedPatch> g_patch;

    }

    bool Install() noexcept {
        return Hook::Patch(g_patch, kSpendCash, 0, kKeepCash);
    }

    void Remove() noexcept {
        g_patch.reset();
    }

}
