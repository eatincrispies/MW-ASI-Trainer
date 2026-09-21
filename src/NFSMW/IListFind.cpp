#include "IListFind.hpp"

#include "../dllmain.hpp"

namespace IListFind {

    namespace {

        constexpr Memory::Pattern kVehicleLookup =
            Memory::ParsePattern("8B 46 34 C7 06 ?? ?? ?? ?? C7 46 08 ?? ?? ?? ?? 8B 48 04 68 ?? ?? ?? ?? E8 ?? ?? ?? ??");

        constexpr std::ptrdiff_t kFindCall      = 25;
        constexpr std::ptrdiff_t kInterfaceList = 0x04;

        using FindFn = void*(__thiscall*)(void*, void*);

        std::uintptr_t g_find = 0;

    }

    bool Resolve() noexcept {
        if (g_find != 0) return true;

        const auto find = Hook::Operand(kVehicleLookup, kFindCall, true);
        if (!find || !Memory::IsExecutable(reinterpret_cast<const void*>(*find))) return false;

        g_find = *find;
        return true;
    }

    void* Find(void* object, std::uintptr_t handle) {
        if (object == nullptr || g_find == 0 || handle == 0) return nullptr;

        void* const list = Game::Field<void*>(object, kInterfaceList);
        if (list == nullptr) return nullptr;
        return reinterpret_cast<FindFn>(g_find)(list, reinterpret_cast<void*>(handle));
    }

}
