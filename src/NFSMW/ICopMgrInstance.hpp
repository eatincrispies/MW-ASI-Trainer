#pragma once

#include <optional>

namespace ICopMgrInstance {

    [[nodiscard]] bool Resolve() noexcept;
    [[nodiscard]] std::optional<int> ActiveHelicopters() noexcept;

}
