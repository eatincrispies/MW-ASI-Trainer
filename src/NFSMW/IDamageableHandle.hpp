#pragma once

namespace IDamageableHandle {

    [[nodiscard]] bool Resolve() noexcept;
    void Destroy(void* simable);

}
