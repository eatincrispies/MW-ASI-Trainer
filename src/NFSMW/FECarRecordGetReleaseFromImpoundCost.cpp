#include "FECarRecordGetReleaseFromImpoundCost.hpp"

#include "../dllmain.hpp"

namespace FECarRecordGetReleaseFromImpoundCost {

    namespace {

        constexpr Memory::Pattern kGetReleaseFromImpoundCost =
            Memory::ParsePattern("51 E8 ?? ?? ?? ?? 85 C0 89 04 24 DB 04 24 7D 06 D8 05 ?? ?? ?? ?? "
                                 "D9 05 ?? ?? ?? ?? D8 C9 E8");

        constexpr std::array<std::uint8_t, 3> kFree{ 0x33, 0xC0, 0xC3 };

        std::optional<ScopedPatch> g_patch;

    }

    bool Install() noexcept {
        return Hook::Patch(g_patch, kGetReleaseFromImpoundCost, 0, kFree);
    }

}
