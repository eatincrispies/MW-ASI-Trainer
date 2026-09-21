#include "AIVehicleHelicopterUpdateFuel.hpp"

#include "../dllmain.hpp"

namespace AIVehicleHelicopterUpdateFuel {

    namespace {

        constexpr Memory::Pattern kUpdateFuel =
            Memory::ParsePattern("56 8B F1 D9 86 D8 07 00 00 D8 64 24 08 D9 96 D8 07 00 00");

        constexpr std::array<std::uint8_t, 3> kKeepFuel{ 0xC2, 0x04, 0x00 };

        std::optional<ScopedPatch> g_patch;

    }

    bool Install() noexcept {
        return Hook::Patch(g_patch, kUpdateFuel, 0, kKeepFuel);
    }

}
