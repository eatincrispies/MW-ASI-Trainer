#pragma once

#include <cstdint>
#include <optional>

namespace CubicCameraMoverUpdate {

    [[nodiscard]] bool InstallFieldOfView(float degrees) noexcept;
    [[nodiscard]] std::optional<std::uint16_t> UnwidenedFieldOfView(const void* camera, std::uint16_t shown) noexcept;

}
