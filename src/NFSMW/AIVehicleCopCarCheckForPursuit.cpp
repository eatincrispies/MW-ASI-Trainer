#include "AIVehicleCopCarCheckForPursuit.hpp"

#include "../dllmain.hpp"
#include "IVehicleHandle.hpp"

namespace AIVehicleCopCarCheckForPursuit {

    namespace {

        constexpr Memory::Pattern kCheckForPursuit =
            Memory::ParsePattern("6A FF 68 ?? ?? ?? ?? 64 A1 00 00 00 00 50 64 89 25 00 00 00 00 "
                                 "83 EC 4C 53 8B 5C 24 60 8B 03 56 57 8B F9 8B CB FF 50 04");

        constexpr std::size_t kStolenBytes = 7;

        using CheckForPursuitFn = bool(__thiscall*)(void*, void*);

        std::uintptr_t g_original = 0;
        ScopedHook     g_detour;

        bool __fastcall CheckForPursuit(void* cop, void*, void* candidate) {
            bool ignored = false;
            Memory::Guarded([&] { ignored = IVehicleHandle::IsHuman(candidate); });

            return !ignored && reinterpret_cast<CheckForPursuitFn>(g_original)(cop, candidate);
        }

    }

    bool Install() noexcept {
        return Hook::Detour(g_detour, kCheckForPursuit, kStolenBytes, reinterpret_cast<const void*>(&CheckForPursuit),
                            g_original);
    }

}
