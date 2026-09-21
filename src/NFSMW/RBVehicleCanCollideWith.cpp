#include "RBVehicleCanCollideWith.hpp"

#include "../dllmain.hpp"
#include "ISimable.hpp"
#include "IVehicleHandle.hpp"

namespace RBVehicleCanCollideWith {

    namespace {

        constexpr Memory::Pattern kCanCollideWith =
            Memory::ParsePattern("8A 81 70 01 00 00 84 C0 74 1C 8B 41 78 8B 08 8A 51 1D");

        constexpr std::size_t    kStolenBytes = 6;
        constexpr std::ptrdiff_t kOwner       = 0x34;

        using CanCollideWithFn = bool(__thiscall*)(void*, void*);

        std::uintptr_t g_original = 0;
        ScopedHook     g_detour;

        bool IsPlayerAgainstCop(void* body, void* other) {
            void* const first  = Game::Field<void*>(body, kOwner);
            void* const second = Game::Field<void*>(other, kOwner);
            if (first == nullptr || second == nullptr) return false;

            if (ISimable::IsPlayer(first)) return IVehicleHandle::IsCop(second);
            if (ISimable::IsPlayer(second)) return IVehicleHandle::IsCop(first);
            return false;
        }

        bool __fastcall CanCollideWith(void* body, void*, void* other) {
            if (!reinterpret_cast<CanCollideWithFn>(g_original)(body, other)) return false;

            bool ghosted = false;
            Memory::Guarded([&] { ghosted = IsPlayerAgainstCop(body, other); });
            return !ghosted;
        }

    }

    bool Install() noexcept {
        if (!IVehicleHandle::Resolve()) return false;
        return Hook::Detour(g_detour, kCanCollideWith, kStolenBytes, reinterpret_cast<const void*>(&CanCollideWith),
                            g_original);
    }

}
