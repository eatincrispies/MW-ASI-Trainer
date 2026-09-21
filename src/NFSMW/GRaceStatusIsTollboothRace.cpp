#include "GRaceStatusIsTollboothRace.hpp"

#include "../dllmain.hpp"

namespace GRaceStatusIsTollboothRace {

    namespace {

        constexpr Memory::Pattern kIsTollboothRace =
            Memory::ParsePattern("A1 ?? ?? ?? ?? 85 C0 74 1A 8B 88 68 19 00 00 85 C9 74 10 E8 ?? ?? ?? ?? "
                                 "83 F8 04 75 06 B8 01 00 00 00 C3 33 C0 C3");

        using IsTollboothRaceFn = int(__cdecl*)();

        std::uintptr_t g_function = 0;

    }

    bool Resolve() noexcept {
        if (g_function != 0) return true;

        const auto function = Scan::Find(kIsTollboothRace);
        if (!function) return false;

        g_function = *function;
        return true;
    }

    bool Call() {
        return reinterpret_cast<IsTollboothRaceFn>(g_function)() != 0;
    }

}
