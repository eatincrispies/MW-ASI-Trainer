#pragma once

#include <cstdint>

namespace AIPursuitCopRequest {

    [[nodiscard]] bool InstallLimit(std::uintptr_t heliVehicle, const void* gate) noexcept;

}
