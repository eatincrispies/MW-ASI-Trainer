#include "FEngineUpdate.hpp"

#include "../dllmain.hpp"
#include "FEDatabase.hpp"
#include "FEMarkerManagerAddMarkerToInventory.hpp"

namespace FEngineUpdate {

    namespace {

        constexpr Memory::Pattern kUpdate =
            Memory::ParsePattern("83 EC 0C 56 8B F1 8A 86 4E 52 00 00 84 C0 74 0B 8B 8E 08 01 00 00 "
                                 "8B 01 FF 50 5C 80 3E 00");

        constexpr std::size_t kStolenBytes = 6;

        using UpdateFn = void(__thiscall*)(void*, std::uint32_t, std::uint32_t);

        std::uintptr_t g_original = 0;
        ScopedHook     g_detour;

        void __fastcall Update(void* engine, void*, std::uint32_t first, std::uint32_t second) {
            reinterpret_cast<UpdateFn>(g_original)(engine, first, second);
            FEDatabase::Tick();
            FEMarkerManagerAddMarkerToInventory::Tick();
        }

    }

    bool Install() noexcept {
        return Hook::Detour(g_detour, kUpdate, kStolenBytes, reinterpret_cast<const void*>(&Update), g_original);
    }

}
