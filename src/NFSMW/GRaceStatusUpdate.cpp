#include "GRaceStatusUpdate.hpp"

#include "../dllmain.hpp"
#include "IVehicleHandle.hpp"
#include "KeyedNodeSearch.hpp"

#include <algorithm>

namespace GRaceStatusUpdate {

    namespace {

        constexpr Memory::Pattern kUpdate =
            Memory::ParsePattern("83 EC 28 53 55 56 8B F1 8B 86 60 19 00 00 83 F8 01 57 75 17 "
                                 "8A 8E A5 19 00 00 84 C9 74 0D");

        constexpr std::size_t    kStolenBytes = 6;
        constexpr std::ptrdiff_t kRacers      = 0x18;
        constexpr std::ptrdiff_t kRacerStride = 0x194;
        constexpr std::ptrdiff_t kRacerCount  = 0x1958;
        constexpr std::ptrdiff_t kRacerHandle = 0x00;
        constexpr int            kMaxRacers   = 16;

        using UpdateFn = void(__thiscall*)(void*, float);

        std::uintptr_t g_original = 0;
        ScopedHook     g_detour;

        void FreezeOpponents(std::byte* status) {
            const int count = std::clamp(Game::Field<int>(status, kRacerCount), 0, kMaxRacers);

            for (int index = 0; index < count; ++index) {
                std::byte* const racer = status + kRacers + index * kRacerStride;

                void* const simable = KeyedNodeSearch::FindSimable(Game::Field<std::uint32_t>(racer, kRacerHandle));
                void* const vehicle = IVehicleHandle::Get(simable);
                if (vehicle == nullptr) continue;

                if (IVehicleHandle::GetDriverClass(vehicle) != IVehicleHandle::DriverClass::Racer) continue;
                if (!IVehicleHandle::IsRaceStopped(vehicle)) IVehicleHandle::RaceStop(vehicle);
            }
        }

        void __fastcall Update(void* status, void*, float deltaTime) {
            reinterpret_cast<UpdateFn>(g_original)(status, deltaTime);
            Memory::Guarded([&] { FreezeOpponents(static_cast<std::byte*>(status)); });
        }

    }

    bool Install() noexcept {
        if (!KeyedNodeSearch::Resolve() || !IVehicleHandle::Resolve()) return false;
        return Hook::Detour(g_detour, kUpdate, kStolenBytes, reinterpret_cast<const void*>(&Update), g_original);
    }

}
