#pragma once

#include <Windows.h>

namespace eDisplayFrame {

    [[nodiscard]] bool InstallPopup(HMODULE resources) noexcept;
    void ShowPopup() noexcept;

}
