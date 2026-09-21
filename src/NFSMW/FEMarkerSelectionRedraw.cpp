#include "FEMarkerSelectionRedraw.hpp"

#include "../dllmain.hpp"

namespace FEMarkerSelectionRedraw {

    namespace {

        constexpr Memory::Pattern kFocusHandler =
            Memory::ParsePattern("8B CE E8 ?? ?? ?? ?? 5E C2 10 00 3D 3D 31 3E BB");

        constexpr std::ptrdiff_t kCall = 3;

        using RedrawFn = void(__thiscall*)(void*);

        std::uintptr_t g_function = 0;

    }

    bool Resolve() noexcept {
        if (g_function != 0) return true;

        const auto function = Hook::Operand(kFocusHandler, kCall, true);
        if (!function || !Memory::IsExecutable(reinterpret_cast<const void*>(*function))) return false;

        g_function = *function;
        return true;
    }

    void Call(void* screen) {
        reinterpret_cast<RedrawFn>(g_function)(screen);
    }

}
