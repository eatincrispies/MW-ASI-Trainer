#include "AIPursuitCopRequest.hpp"

#include "../dllmain.hpp"

namespace AIPursuitCopRequest {

    namespace {

        constexpr Memory::Pattern kHeliRequest =
            Memory::ParsePattern("C6 86 D4 00 00 00 00 75 55 A1 ?? ?? ?? ?? 85 C0 75 4C 8B CD E8");

        constexpr std::ptrdiff_t kHeliCheck       = 9;
        constexpr std::ptrdiff_t kHeliVehicleRead = 10;
        constexpr std::size_t    kHeliCheckLength = 5;

        ScopedHook g_limit;

    }

    bool InstallLimit(std::uintptr_t heliVehicle, const void* gate) noexcept {
        const auto match = Scan::Find(kHeliRequest);
        if (!match) return false;

        const auto read = Scan::Absolute(*match + kHeliVehicleRead);
        if (!read || *read != heliVehicle) return false;

        return Hook::Thunk(g_limit, *match + kHeliCheck, kHeliCheckLength, gate);
    }

    void RemoveLimit() noexcept {
        g_limit.Reset();
    }

}
