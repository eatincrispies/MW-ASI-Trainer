#include "AITrafficManagerComputeDensity.hpp"

#include "../dllmain.hpp"

namespace AITrafficManagerComputeDensity {

    namespace {

        constexpr Memory::Pattern kComputeDensity =
            Memory::ParsePattern("A1 ?? ?? ?? ?? 85 C0 75 13 8B 0D ?? ?? ?? ?? 85 C9 74 10 8B 01 FF 50 14 84 C0 74 07 "
                                 "D9 05 ?? ?? ?? ?? C3");

        constexpr std::array<std::uint8_t, 3> kNoTraffic{ 0xD9, 0xEE, 0xC3 };

        std::optional<ScopedPatch> g_patch;

    }

    bool Install() noexcept {
        return Hook::Patch(g_patch, kComputeDensity, 0, kNoTraffic);
    }

}
