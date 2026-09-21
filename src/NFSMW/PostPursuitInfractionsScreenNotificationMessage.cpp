#include "PostPursuitInfractionsScreenNotificationMessage.hpp"

#include "../dllmain.hpp"

namespace PostPursuitInfractionsScreenNotificationMessage {

    namespace {

        constexpr Memory::Pattern kStrikeCheck =
            Memory::ParsePattern("8B 4E 2C 3B C2 0F 9D C0 88 46 30 E8");

        constexpr std::ptrdiff_t              kImpoundDecision = 5;
        constexpr std::array<std::uint8_t, 3> kNeverImpounded{ 0x32, 0xC0, 0x90 };

        std::optional<ScopedPatch> g_patch;

    }

    bool Install() noexcept {
        return Hook::Patch(g_patch, kStrikeCheck, kImpoundDecision, kNeverImpounded);
    }

}
