#pragma once

#include <cstdint>

namespace AICopManagerSpawnPursuitHelicopter {

    [[nodiscard]] bool InstallDisable() noexcept;
    [[nodiscard]] bool InstallLimit(int maxHelicopters) noexcept;
    void RemoveLimit() noexcept;
    [[nodiscard]] std::uintptr_t HeliVehicle() noexcept;
    [[nodiscard]] std::uintptr_t Spawner() noexcept;
    [[nodiscard]] const void* Gate() noexcept;

}
