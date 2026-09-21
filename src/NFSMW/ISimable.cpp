#include "ISimable.hpp"

#include "../dllmain.hpp"

namespace ISimable {

    namespace {

        constexpr std::ptrdiff_t kGetPlayer    = 0x20;
        constexpr std::ptrdiff_t kGetRigidBody = 0x54;

    }

    bool IsPlayer(void* simable) {
        return simable != nullptr && Game::CallVirtual<void*>(simable, kGetPlayer) != nullptr;
    }

    void* GetRigidBody(void* simable) {
        return simable != nullptr ? Game::CallVirtual<void*>(simable, kGetRigidBody) : nullptr;
    }

}
