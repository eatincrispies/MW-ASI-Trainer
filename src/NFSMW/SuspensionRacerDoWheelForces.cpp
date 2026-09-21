#include "SuspensionRacerDoWheelForces.hpp"

#include "../dllmain.hpp"
#include "ISimable.hpp"

namespace SuspensionRacerDoWheelForces {

    namespace {

        constexpr Memory::Pattern kDoWheelForces =
            Memory::ParsePattern("81 EC A4 01 00 00 53 56 8B B4 24 B0 01 00 00 8B 86 C0 00 00 00 "
                                 "57 8B D9 8D 4C 24 78 51 33 FF");

        constexpr std::size_t    kStolenBytes = 6;
        constexpr std::ptrdiff_t kOwner       = 0x34;

        using DoWheelForcesFn = void(__thiscall*)(void*, void*);

        std::uintptr_t g_original = 0;
        bool           g_player   = false;
        ScopedHook     g_detour;

        void __fastcall DoWheelForces(void* suspension, void*, void* state) {
            bool player = false;
            Memory::Guarded([&] { player = ISimable::IsPlayer(Game::Field<void*>(suspension, kOwner)); });

            const bool outer = g_player;
            g_player = player;
            reinterpret_cast<DoWheelForcesFn>(g_original)(suspension, state);
            g_player = outer;
        }

    }

    bool Install() noexcept {
        return Hook::Detour(g_detour, kDoWheelForces, kStolenBytes,
                            reinterpret_cast<const void*>(&DoWheelForces), g_original);
    }

    void Remove() noexcept {
        g_detour.Reset();
    }

    bool IsPlayerCar() noexcept {
        return g_player;
    }

}
