#pragma once

namespace ISimable {

    [[nodiscard]] bool IsPlayer(void* simable);
    [[nodiscard]] void* GetRigidBody(void* simable);

}
