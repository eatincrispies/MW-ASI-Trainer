#include "IRBVehicleHandle.hpp"

#include "../dllmain.hpp"
#include "IListFind.hpp"

namespace IRBVehicleHandle {

    namespace {

        constexpr Memory::Pattern kBreakerLookup =
            Memory::ParsePattern("8B 4D 04 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B F0 85 F6 74 ?? 84 DB 74 ?? "
                                 "8B 45 00 8B CD FF 50 54 8B 10 8B 1E 8B C8 FF 52 18");

        constexpr std::ptrdiff_t kHandlePush       = 4;
        constexpr std::ptrdiff_t kSetCollisionMass = 0x04;

        std::uintptr_t g_handle = 0;

    }

    bool Resolve() noexcept {
        if (g_handle != 0) return true;
        if (!IListFind::Resolve()) return false;

        const auto handle = Hook::Operand(kBreakerLookup, kHandlePush);
        if (!handle || *handle == 0) return false;

        g_handle = *handle;
        return true;
    }

    void* Get(void* simable) {
        return IListFind::Find(simable, g_handle);
    }

    void SetCollisionMass(void* rbVehicle, float mass) {
        Game::CallVirtual<void>(rbVehicle, kSetCollisionMass, mass);
    }

}
