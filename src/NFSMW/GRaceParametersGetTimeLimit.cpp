#include "GRaceParametersGetTimeLimit.hpp"

#include "../dllmain.hpp"

namespace GRaceParametersGetTimeLimit {

    namespace {

        constexpr Memory::Pattern kTimeRemaining =
            Memory::ParsePattern("51 56 8B F1 8B 8E 68 19 00 00 85 C9 74 5B E8 ?? ?? ?? ?? D8 86 80 19 00 00 D9 54 24 04");

        constexpr std::ptrdiff_t kCall = 15;

        using GetTimeLimitFn = float(__thiscall*)(void*);

        std::uintptr_t g_function = 0;

    }

    bool Resolve() noexcept {
        if (g_function != 0) return true;

        const auto function = Hook::Operand(kTimeRemaining, kCall, true);
        if (!function || !Memory::IsExecutable(reinterpret_cast<const void*>(*function))) return false;

        g_function = *function;
        return true;
    }

    float Call(void* parameters) {
        return reinterpret_cast<GetTimeLimitFn>(g_function)(parameters);
    }

}
