#include "EngineRacerDoNos.hpp"

#include "../dllmain.hpp"

namespace EngineRacerDoNos {

    namespace {

        constexpr Memory::Pattern kDriverClassCheck =
            Memory::ParsePattern("A0 ?? ?? ?? ?? 84 C0 D8 74 24 28 D9 5C 24 28 75 0D "
                                 "8B 4E 48 8B 11 FF 52 58 83 F8 06 75 08");

        constexpr std::ptrdiff_t               kDriverClass = 0x1B;
        constexpr std::array<std::uint8_t, 1> kHuman{ 0x00 };

        std::optional<ScopedPatch> g_patch;

    }

    bool Install() noexcept {
        return Hook::Patch(g_patch, kDriverClassCheck, kDriverClass, kHuman);
    }

}
