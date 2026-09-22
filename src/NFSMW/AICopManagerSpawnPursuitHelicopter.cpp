#include "AICopManagerSpawnPursuitHelicopter.hpp"

#include "../dllmain.hpp"
#include "ICopMgrInstance.hpp"

namespace AICopManagerSpawnPursuitHelicopter {

    namespace {

        constexpr Memory::Pattern kSpawnPursuitHelicopter =
            Memory::ParsePattern("6A FF 68 ?? ?? ?? ?? 64 A1 00 00 00 00 50 64 89 25 00 00 00 00 81 EC A0 03 00 00 "
                                 "A1 ?? ?? ?? ?? 85 C0 53 55 56 57 89 4C 24 34 0F 85");

        constexpr std::array<std::uint8_t, 5> kNoHelicopter{ 0x32, 0xC0, 0xC2, 0x04, 0x00 };
        constexpr std::ptrdiff_t              kHeliCheck       = 27;
        constexpr std::ptrdiff_t              kHeliVehicleRead = 28;
        constexpr std::size_t                 kHeliCheckLength = 5;

        std::uintptr_t             g_spawner        = 0;
        std::uintptr_t             g_heliVehicle    = 0;
        int                        g_maxHelicopters = 1;
        std::optional<ScopedPatch> g_disable;
        ScopedHook                 g_limit;

        std::uint32_t __cdecl HelicopterGate() {
            if (const auto active = ICopMgrInstance::ActiveHelicopters()) {
                return *active >= g_maxHelicopters ? 1u : 0u;
            }
            return Memory::Read<std::uint32_t>(g_heliVehicle).value_or(0);
        }

    }

    bool InstallDisable() noexcept {
        return Hook::Patch(g_disable, kSpawnPursuitHelicopter, 0, kNoHelicopter);
    }

    bool InstallLimit(int maxHelicopters) noexcept {
        if (!ICopMgrInstance::Resolve()) return false;

        const auto match = Scan::Find(kSpawnPursuitHelicopter);
        if (!match) return false;

        const auto heliVehicle = Scan::Absolute(*match + kHeliVehicleRead);
        if (!heliVehicle || *heliVehicle == 0) return false;

        g_spawner        = *match;
        g_heliVehicle    = *heliVehicle;
        g_maxHelicopters = maxHelicopters;
        return Hook::Thunk(g_limit, *match + kHeliCheck, kHeliCheckLength, Gate());
    }

    void RemoveLimit() noexcept {
        g_limit.Reset();
    }

    std::uintptr_t HeliVehicle() noexcept {
        return g_heliVehicle;
    }

    std::uintptr_t Spawner() noexcept {
        return g_spawner;
    }

    const void* Gate() noexcept {
        return reinterpret_cast<const void*>(&HelicopterGate);
    }

}
