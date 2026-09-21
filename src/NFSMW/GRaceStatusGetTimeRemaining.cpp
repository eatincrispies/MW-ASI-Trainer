#include "GRaceStatusGetTimeRemaining.hpp"

#include "../dllmain.hpp"
#include "GRaceParametersGetTimeLimit.hpp"
#include "GRaceStatusIsTollboothRace.hpp"

namespace GRaceStatusGetTimeRemaining {

    namespace {

        constexpr Memory::Pattern kGetTimeRemaining =
            Memory::ParsePattern("51 56 8B F1 8B 8E 68 19 00 00 85 C9 74 5B E8 ?? ?? ?? ?? D8 86 80 19 00 00 D9 54 24 04");

        constexpr std::size_t    kStolenBytes    = 10;
        constexpr std::ptrdiff_t kRaceParameters = 0x1968;
        constexpr std::ptrdiff_t kBonusTime      = 0x1980;

        using GetTimeRemainingFn = float(__thiscall*)(void*);

        std::uintptr_t g_original = 0;
        ScopedHook     g_detour;

        float __fastcall GetTimeRemaining(void* status, void*) {
            float remaining = reinterpret_cast<GetTimeRemainingFn>(g_original)(status);

            Memory::Guarded([&] {
                void* const parameters = Game::Field<void*>(status, kRaceParameters);
                if (parameters == nullptr || !GRaceStatusIsTollboothRace::Call()) return;
                remaining = GRaceParametersGetTimeLimit::Call(parameters) + Game::Field<float>(status, kBonusTime);
            });
            return remaining;
        }

    }

    bool Install() noexcept {
        if (!GRaceParametersGetTimeLimit::Resolve() || !GRaceStatusIsTollboothRace::Resolve()) return false;
        return Hook::Detour(g_detour, kGetTimeRemaining, kStolenBytes, reinterpret_cast<const void*>(&GetTimeRemaining),
                            g_original);
    }

}
