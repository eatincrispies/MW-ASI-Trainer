#include "AIVehicleHumanIPerpetrator.hpp"

#include "../dllmain.hpp"

#include <algorithm>
#include <climits>
#include <cmath>

namespace AIVehicleHumanIPerpetrator {

    namespace {

        constexpr Memory::Pattern kConstructorVTables =
            Memory::ParsePattern("C7 46 54 ?? ?? ?? ?? C7 86 58 07 00 00 ?? ?? ?? ?? "
                                 "C7 86 60 07 00 00 ?? ?? ?? ?? C7 86 6C 07 00 00 ?? ?? ?? ?? "
                                 "C7 86 C4 07 00 00 ?? ?? ?? ?? 8B C6 5E");

        constexpr std::ptrdiff_t kVTableStore       = 13;
        constexpr std::ptrdiff_t kGetHeat           = 0x04;
        constexpr std::ptrdiff_t kSetHeat           = 0x08;
        constexpr std::ptrdiff_t kBountyFromCops    = 0x38;
        constexpr std::ptrdiff_t kBountyFromPursuit = 0x3C;
        constexpr std::ptrdiff_t kHeat              = 0x1C;

        using AddBountyFn = void(__thiscall*)(void*, int);
        using SetHeatFn   = void(__thiscall*)(void*, float);

        float          g_bountyMultiplier  = 1.0f;
        float          g_heat              = 1.0f;
        bool           g_syncingHeat       = false;
        std::uintptr_t g_bountyFromCops    = 0;
        std::uintptr_t g_bountyFromPursuit = 0;
        std::uintptr_t g_getHeat           = 0;
        std::uintptr_t g_setHeat           = 0;

        std::optional<ScopedPatch> g_bountyFromCopsHook;
        std::optional<ScopedPatch> g_bountyFromPursuitHook;
        std::optional<ScopedPatch> g_getHeatHook;
        std::optional<ScopedPatch> g_setHeatHook;

        int ScaleBounty(int amount) noexcept {
            const double scaled = std::round(static_cast<double>(amount) * g_bountyMultiplier);
            return static_cast<int>(std::clamp(scaled, static_cast<double>(INT_MIN), static_cast<double>(INT_MAX)));
        }

        void __fastcall AddBountyFromCops(void* perpetrator, void*, int amount) {
            reinterpret_cast<AddBountyFn>(g_bountyFromCops)(perpetrator, ScaleBounty(amount));
        }

        void __fastcall AddBountyFromPursuit(void* perpetrator, void*, int amount) {
            reinterpret_cast<AddBountyFn>(g_bountyFromPursuit)(perpetrator, ScaleBounty(amount));
        }

        float __fastcall GetHeat(void* perpetrator, void*) {
            Memory::Guarded([&] {
                if (g_syncingHeat || Game::Field<float>(perpetrator, kHeat) == g_heat) return;
                g_syncingHeat = true;
                reinterpret_cast<SetHeatFn>(g_setHeat)(perpetrator, g_heat);
                g_syncingHeat = false;
            });
            return g_heat;
        }

        void __fastcall SetHeat(void* perpetrator, void*, float) {
            reinterpret_cast<SetHeatFn>(g_setHeat)(perpetrator, g_heat);
        }

        std::optional<std::uintptr_t> VTable() noexcept {
            return Hook::Operand(kConstructorVTables, kVTableStore);
        }

    }

    bool InstallBountyMultiplier(float multiplier) noexcept {
        const auto vtable = VTable();
        if (!vtable) return false;

        g_bountyMultiplier = multiplier;
        if (Hook::VTable(g_bountyFromCopsHook, *vtable, kBountyFromCops, reinterpret_cast<const void*>(&AddBountyFromCops),
                         g_bountyFromCops) &&
            Hook::VTable(g_bountyFromPursuitHook, *vtable, kBountyFromPursuit,
                         reinterpret_cast<const void*>(&AddBountyFromPursuit), g_bountyFromPursuit)) {
            return true;
        }

        g_bountyFromPursuitHook.reset();
        g_bountyFromCopsHook.reset();
        return false;
    }

    bool InstallHeatLock(float heat) noexcept {
        const auto vtable = VTable();
        if (!vtable) return false;

        g_heat = heat;
        if (Hook::VTable(g_setHeatHook, *vtable, kSetHeat, reinterpret_cast<const void*>(&SetHeat), g_setHeat) &&
            Hook::VTable(g_getHeatHook, *vtable, kGetHeat, reinterpret_cast<const void*>(&GetHeat), g_getHeat)) {
            return true;
        }

        g_getHeatHook.reset();
        g_setHeatHook.reset();
        return false;
    }

}
