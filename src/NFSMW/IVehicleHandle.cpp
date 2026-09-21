#include "IVehicleHandle.hpp"

#include "../dllmain.hpp"
#include "IListFind.hpp"

namespace IVehicleHandle {

    namespace {

        constexpr Memory::Pattern kVehicleLookup =
            Memory::ParsePattern("8B 46 34 C7 06 ?? ?? ?? ?? C7 46 08 ?? ?? ?? ?? 8B 48 04 68 ?? ?? ?? ?? E8 ?? ?? ?? ??");

        constexpr std::ptrdiff_t kHandlePush     = 20;
        constexpr std::ptrdiff_t kGetDriverClass = 0x58;
        constexpr std::ptrdiff_t kForceStopOn    = 0x6C;
        constexpr std::ptrdiff_t kGetForceStop   = 0x74;
        constexpr char           kRaceStop       = 0x01;

        std::uintptr_t g_handle = 0;

    }

    bool Resolve() noexcept {
        if (g_handle != 0) return true;
        if (!IListFind::Resolve()) return false;

        const auto handle = Hook::Operand(kVehicleLookup, kHandlePush);
        if (!handle || *handle == 0) return false;

        g_handle = *handle;
        return true;
    }

    void* Get(void* simable) {
        return IListFind::Find(simable, g_handle);
    }

    DriverClass GetDriverClass(void* vehicle) {
        return static_cast<DriverClass>(Game::CallVirtual<int>(vehicle, kGetDriverClass));
    }

    bool IsHuman(void* vehicle) {
        return vehicle != nullptr && GetDriverClass(vehicle) == DriverClass::Human;
    }

    bool IsCop(void* simable) {
        void* const vehicle = Get(simable);
        return vehicle != nullptr && GetDriverClass(vehicle) == DriverClass::Cop;
    }

    bool IsRaceStopped(void* vehicle) {
        return (Game::CallVirtual<char>(vehicle, kGetForceStop) & kRaceStop) != 0;
    }

    void RaceStop(void* vehicle) {
        Game::CallVirtual<void>(vehicle, kForceStopOn, kRaceStop);
    }

}
