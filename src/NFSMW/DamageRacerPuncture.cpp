#include "DamageRacerPuncture.hpp"

#include "../dllmain.hpp"
#include "IVehicleHandle.hpp"

namespace DamageRacerPuncture {

    namespace {

        constexpr Memory::Pattern kPuncture =
            Memory::ParsePattern("64 A1 00 00 00 00 6A FF 68 ?? ?? ?? ?? 50 64 89 25 00 00 00 00 "
                                 "56 57 8B 7C 24 18 83 FF 04 8B F1 73 ?? 8A 44 37 1C");

        constexpr std::size_t    kStolenBytes = 6;
        constexpr std::ptrdiff_t kVehicle     = -0x70;

        using PunctureFn = void(__thiscall*)(void*, unsigned);

        std::uintptr_t g_original = 0;
        ScopedHook     g_detour;

        void __fastcall Puncture(void* damage, void*, unsigned wheel) {
            bool shielded = false;
            Memory::Guarded([&] { shielded = IVehicleHandle::IsHuman(Game::Field<void*>(damage, kVehicle)); });

            if (!shielded) reinterpret_cast<PunctureFn>(g_original)(damage, wheel);
        }

    }

    bool Install() noexcept {
        return Hook::Detour(g_detour, kPuncture, kStolenBytes, reinterpret_cast<const void*>(&Puncture), g_original);
    }

}
