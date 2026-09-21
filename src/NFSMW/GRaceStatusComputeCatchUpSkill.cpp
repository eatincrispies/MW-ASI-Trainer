#include "GRaceStatusComputeCatchUpSkill.hpp"

#include "../dllmain.hpp"

namespace GRaceStatusComputeCatchUpSkill {

    namespace {

        constexpr Memory::Pattern kComputeCatchUpSkill =
            Memory::ParsePattern("83 EC 20 8A 44 24 34 53 32 DB 84 C0 56 8B F1 C6 44 24 0B 00 74 0D "
                                 "C7 44 24 3C 00 00 80 3F E9");

        constexpr std::array<std::uint8_t, 5> kNoCatchUp{ 0x32, 0xC0, 0xC2, 0x14, 0x00 };

        std::optional<ScopedPatch> g_patch;

    }

    bool Install() noexcept {
        return Hook::Patch(g_patch, kComputeCatchUpSkill, 0, kNoCatchUp);
    }

}
