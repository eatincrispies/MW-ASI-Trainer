#include "FEMarkerManagerGetNumMarkers.hpp"

#include "../dllmain.hpp"

namespace FEMarkerManagerGetNumMarkers {

    namespace {

        constexpr Memory::Pattern kGetNumMarkers =
            Memory::ParsePattern("8B 54 24 08 53 56 8B 74 24 0C 57 33 C0 83 C1 08 BF 15 00 00 00 BB 01 00 00 00");

        using GetNumMarkersFn = int(__thiscall*)(void*, int, int);

        std::uintptr_t g_function = 0;

    }

    bool Resolve() noexcept {
        if (g_function != 0) return true;

        const auto function = Scan::Find(kGetNumMarkers);
        if (!function) return false;

        g_function = *function;
        return true;
    }

    int Call(void* manager, int type, int param) {
        return reinterpret_cast<GetNumMarkersFn>(g_function)(manager, type, param);
    }

}
