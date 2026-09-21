#pragma once

#include <cstdint>

namespace IListFind {

    [[nodiscard]] bool Resolve() noexcept;
    [[nodiscard]] void* Find(void* object, std::uintptr_t handle);

}
