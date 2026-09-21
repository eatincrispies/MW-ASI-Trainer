#pragma once

namespace IRBVehicleHandle {

    [[nodiscard]] bool Resolve() noexcept;
    [[nodiscard]] void* Get(void* simable);
    void SetCollisionMass(void* rbVehicle, float mass);

}
