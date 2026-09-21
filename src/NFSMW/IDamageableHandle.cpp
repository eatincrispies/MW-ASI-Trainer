#include "IDamageableHandle.hpp"

#include "../dllmain.hpp"
#include "IListFind.hpp"

namespace IDamageableHandle {

    namespace {

        constexpr Memory::Pattern kBreakerZoneLookup =
            Memory::ParsePattern("8B 4B 04 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 85 C0 74 07 8B 10 8B C8 FF 52 1C "
                                 "8B 87 D0 00 00 00");

        constexpr std::ptrdiff_t kHandlePush  = 4;
        constexpr std::ptrdiff_t kIsDestroyed = 0x18;
        constexpr std::ptrdiff_t kDestroy     = 0x1C;

        std::uintptr_t g_handle = 0;

    }

    bool Resolve() noexcept {
        if (g_handle != 0) return true;
        if (!IListFind::Resolve()) return false;

        const auto handle = Hook::Operand(kBreakerZoneLookup, kHandlePush);
        if (!handle || *handle == 0) return false;

        g_handle = *handle;
        return true;
    }

    void Destroy(void* simable) {
        void* const damageable = IListFind::Find(simable, g_handle);
        if (damageable == nullptr || Game::CallVirtual<bool>(damageable, kIsDestroyed)) return;
        Game::CallVirtual<void>(damageable, kDestroy);
    }

}
