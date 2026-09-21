#pragma once

namespace FEMarkerManagerGetNumMarkers {

    [[nodiscard]] bool Resolve() noexcept;
    [[nodiscard]] int Call(void* manager, int type, int param);

}
