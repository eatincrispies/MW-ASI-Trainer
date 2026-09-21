#pragma once

#include <cstdint>

namespace KeyedNodeSearch {

    [[nodiscard]] bool Resolve() noexcept;
    [[nodiscard]] void* FindSimable(std::uint32_t handle);

}
