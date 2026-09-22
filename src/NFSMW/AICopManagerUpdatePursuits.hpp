#pragma once

#include <cstdint>

namespace AICopManagerUpdatePursuits {

    [[nodiscard]] bool InstallHelicopterTopUp(std::uintptr_t spawnHelicopter, int maxHelicopters) noexcept;
    [[nodiscard]] bool InstallPaperWeightCops() noexcept;
    void RemoveHelicopterTopUp() noexcept;

}
