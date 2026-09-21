#include "FEMarkerSelectionGetSelectedButtonIndex.hpp"

#include "../dllmain.hpp"

namespace FEMarkerSelectionGetSelectedButtonIndex {

    namespace {

        constexpr Memory::Pattern kPickHandler =
            Memory::ParsePattern("8B CE E8 ?? ?? ?? ?? 8B F8 8D 04 7F 8D 1C 86 8A 43 7C 84 C0");

        constexpr std::ptrdiff_t kCall = 3;

        using GetSelectedButtonIndexFn = int(__thiscall*)(void*);

        std::uintptr_t g_function = 0;

    }

    bool Resolve() noexcept {
        if (g_function != 0) return true;

        const auto function = Hook::Operand(kPickHandler, kCall, true);
        if (!function || !Memory::IsExecutable(reinterpret_cast<const void*>(*function))) return false;

        g_function = *function;
        return true;
    }

    int Call(void* screen) {
        return reinterpret_cast<GetSelectedButtonIndexFn>(g_function)(screen);
    }

}
