#include "EngineRacerGetEngineTorque.hpp"

#include "../dllmain.hpp"
#include "IVehicleHandle.hpp"

namespace EngineRacerGetEngineTorque {

    namespace {

        constexpr Memory::Pattern kGetEngineTorque =
            Memory::ParsePattern("8B 44 24 04 56 8B F1 50 8D 8E 44 01 00 00 51 E8 ?? ?? ?? ?? "
                                 "8B 4E 48 8B 11 D8 0D ?? ?? ?? ?? D9 86 98 00 00 00");

        constexpr std::size_t    kStolenBytes = 5;
        constexpr std::ptrdiff_t kVehicle     = 0x48;

        using GetEngineTorqueFn = float(__thiscall*)(void*, float);

        std::uintptr_t g_original   = 0;
        float          g_multiplier = 1.0f;
        ScopedHook     g_detour;

        float __fastcall GetEngineTorque(void* engine, void*, float rpm) {
            const float torque = reinterpret_cast<GetEngineTorqueFn>(g_original)(engine, rpm);

            bool player = false;
            Memory::Guarded([&] { player = IVehicleHandle::IsHuman(Game::Field<void*>(engine, kVehicle)); });
            return player ? torque * g_multiplier : torque;
        }

    }

    bool Install(float multiplier) noexcept {
        g_multiplier = multiplier;
        return Hook::Detour(g_detour, kGetEngineTorque, kStolenBytes,
                            reinterpret_cast<const void*>(&GetEngineTorque), g_original);
    }

}
