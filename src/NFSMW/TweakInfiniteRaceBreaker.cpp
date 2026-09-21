#include "TweakInfiniteRaceBreaker.hpp"

#include "../dllmain.hpp"

namespace TweakInfiniteRaceBreaker {

    namespace {

        constexpr Memory::Pattern kBreakerDrain =
            Memory::ParsePattern("8A 86 80 00 00 00 84 C0 D8 0D ?? ?? ?? ?? 74 ?? A0 ?? ?? ?? ?? DD D8 84 C0 75 ??");

        constexpr std::ptrdiff_t              kFlagRead = 17;
        constexpr std::array<std::uint8_t, 1> kEnabled{ 0x01 };

        std::optional<ScopedPatch> g_patch;

    }

    bool Install() noexcept {
        if (g_patch) return true;

        const auto flag = Hook::Operand(kBreakerDrain, kFlagRead);
        if (!flag) return false;

        const auto current = Memory::Read<std::uint8_t>(*flag);
        if (!current || *current > 1) return false;

        g_patch.emplace(*flag, kEnabled);
        if (*g_patch) return true;

        g_patch.reset();
        return false;
    }

}
