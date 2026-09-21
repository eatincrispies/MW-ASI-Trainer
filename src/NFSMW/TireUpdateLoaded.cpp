#include "TireUpdateLoaded.hpp"

#include "../dllmain.hpp"
#include "SuspensionRacerDoWheelForces.hpp"

#include <algorithm>

namespace TireUpdateLoaded {

    namespace {

        constexpr Memory::Pattern kUpdateLoaded =
            Memory::ParsePattern("83 EC 24 56 8B F1 8B 86 0C 01 00 00 8B 8E 04 01 00 00 8B 49 08 "
                                 "D9 44 81 08 8B 96 00 01 00 00");

        constexpr std::size_t    kStolenBytes  = 6;
        constexpr std::ptrdiff_t kLateralGrip  = 0x128;
        constexpr std::ptrdiff_t kGripLimit    = 0x12C;
        constexpr float          kFullGrip     = 1000.0f;
        constexpr float          kLateralBonus = 3.0f;

        using UpdateLoadedFn = float(__thiscall*)(void*, float, float, float, float, float);

        std::uintptr_t g_original = 0;
        ScopedHook     g_detour;

        float __fastcall UpdateLoaded(void* tire, void*, float lateralVelocity, float forwardVelocity, float carSpeed,
                                      float load, float deltaTime) {
            const auto original = reinterpret_cast<UpdateLoadedFn>(g_original);
            if (!SuspensionRacerDoWheelForces::IsPlayerCar()) {
                return original(tire, lateralVelocity, forwardVelocity, carSpeed, load, deltaTime);
            }

            float& lateralGrip = Game::Field<float>(tire, kLateralGrip);
            float& gripLimit   = Game::Field<float>(tire, kGripLimit);
            const float savedLateralGrip = lateralGrip;
            const float savedGripLimit   = gripLimit;

            lateralGrip = std::max(savedLateralGrip, kLateralBonus);
            gripLimit   = kFullGrip;
            const float result = original(tire, lateralVelocity, forwardVelocity, carSpeed, load, deltaTime);
            lateralGrip = savedLateralGrip;
            gripLimit   = savedGripLimit;
            return result;
        }

    }

    bool Install() noexcept {
        if (!SuspensionRacerDoWheelForces::Install()) return false;
        if (Hook::Detour(g_detour, kUpdateLoaded, kStolenBytes, reinterpret_cast<const void*>(&UpdateLoaded), g_original)) {
            return true;
        }

        SuspensionRacerDoWheelForces::Remove();
        return false;
    }

}
