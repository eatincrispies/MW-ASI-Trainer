#pragma once

namespace IVehicleHandle {

    enum class DriverClass : int {
        Human   = 0,
        Traffic = 1,
        Cop     = 2,
        Racer   = 3,
    };

    [[nodiscard]] bool Resolve() noexcept;
    [[nodiscard]] void* Get(void* simable);
    [[nodiscard]] DriverClass GetDriverClass(void* vehicle);
    [[nodiscard]] bool IsHuman(void* vehicle);
    [[nodiscard]] bool IsCop(void* simable);
    [[nodiscard]] bool IsRaceStopped(void* vehicle);
    void RaceStop(void* vehicle);

}
