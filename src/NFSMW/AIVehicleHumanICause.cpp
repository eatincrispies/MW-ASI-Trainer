#include "AIVehicleHumanICause.hpp"

#include "../dllmain.hpp"
#include "IDamageableHandle.hpp"
#include "ISimable.hpp"
#include "IVehicleHandle.hpp"

namespace AIVehicleHumanICause {

    namespace {

        constexpr Memory::Pattern kConstructorVTables =
            Memory::ParsePattern("C7 46 54 ?? ?? ?? ?? C7 86 58 07 00 00 ?? ?? ?? ?? "
                                 "C7 86 60 07 00 00 ?? ?? ?? ?? C7 86 6C 07 00 00 ?? ?? ?? ?? "
                                 "C7 86 C4 07 00 00 ?? ?? ?? ?? 8B C6 5E");

        constexpr std::ptrdiff_t kVTableStore       = 23;
        constexpr std::ptrdiff_t kOnCausedCollision = 0x04;

        using OnCausedCollisionFn = void(__thiscall*)(void*, const void*, void*, void*);

        std::uintptr_t             g_original = 0;
        std::optional<ScopedPatch> g_hook;

        void __fastcall OnCausedCollision(void* cause, void*, const void* info, void* collider, void* other) {
            reinterpret_cast<OnCausedCollisionFn>(g_original)(cause, info, collider, other);

            Memory::Guarded([&] {
                if (ISimable::IsPlayer(collider) && IVehicleHandle::IsCop(other)) IDamageableHandle::Destroy(other);
            });
        }

    }

    bool InstallTouchOfDeath() noexcept {
        if (!IVehicleHandle::Resolve() || !IDamageableHandle::Resolve()) return false;

        const auto vtable = Hook::Operand(kConstructorVTables, kVTableStore);
        if (!vtable) return false;

        return Hook::VTable(g_hook, *vtable, kOnCausedCollision, reinterpret_cast<const void*>(&OnCausedCollision),
                            g_original);
    }

}
