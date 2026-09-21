#pragma once

namespace AIPursuitOnTask {

    struct Options {
        bool bustProof       = false;
        bool instantCooldown = false;
        bool tankMode        = false;
    };

    [[nodiscard]] Options Install(Options wanted) noexcept;

}
