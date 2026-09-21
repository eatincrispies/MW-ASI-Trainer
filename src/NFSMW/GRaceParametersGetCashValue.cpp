#include "GRaceParametersGetCashValue.hpp"

#include "../dllmain.hpp"

namespace GRaceParametersGetCashValue {

    namespace {

        constexpr Memory::Pattern kGetCashValue =
            Memory::ParsePattern("56 8B F1 8B 46 04 85 C0 74 0A 8D 48 22 E8 ?? ?? ?? ?? 5E C3 "
                                 "8B 46 0C 85 C0 74 11");

        constexpr std::size_t kStolenBytes = 6;

        using GetCashValueFn = float(__thiscall*)(void*);

        std::uintptr_t g_original   = 0;
        float          g_multiplier = 1.0f;
        ScopedHook     g_detour;

        float __fastcall GetCashValue(void* parameters, void*) {
            return reinterpret_cast<GetCashValueFn>(g_original)(parameters) * g_multiplier;
        }

    }

    bool Install(float multiplier) noexcept {
        g_multiplier = multiplier;
        return Hook::Detour(g_detour, kGetCashValue, kStolenBytes, reinterpret_cast<const void*>(&GetCashValue),
                            g_original);
    }

}
