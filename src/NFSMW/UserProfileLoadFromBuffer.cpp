#include "UserProfileLoadFromBuffer.hpp"

#include "../dllmain.hpp"
#include "FEPlayerCarDBDefault.hpp"

namespace UserProfileLoadFromBuffer {

    namespace {

        constexpr Memory::Pattern kLoadFromBuffer =
            Memory::ParsePattern("83 EC 24 8B 44 24 28 53 55 8B E9 8B 4C 24 34 8D 1C 08 83 C0 10 56");

        constexpr Memory::Pattern kCarDatabaseCopy = Memory::ParsePattern("B9 32 23 00 00 8D BD 14 04 00 00 F3 A5");

        constexpr std::size_t    kStolenBytes      = 7;
        constexpr std::ptrdiff_t kCarDatabaseField = 7;

        using LoadFromBufferFn = bool(__thiscall*)(void*, void*, int, int, int);

        std::uintptr_t g_original    = 0;
        std::ptrdiff_t g_carDatabase = 0;
        ScopedHook     g_detour;

        bool __fastcall LoadFromBuffer(std::byte* profile, void*, void* buffer, int size, int first, int second) {
            const bool loaded = reinterpret_cast<LoadFromBufferFn>(g_original)(profile, buffer, size, first, second);
            if (loaded) Memory::Guarded([&] { FEPlayerCarDBDefault::AddEveryVehicle(profile + g_carDatabase); });
            return loaded;
        }

    }

    bool Install() noexcept {
        const auto field = Hook::Operand(kCarDatabaseCopy, kCarDatabaseField);
        if (!field || *field == 0) return false;

        g_carDatabase = static_cast<std::ptrdiff_t>(*field);
        return Hook::Detour(g_detour, kLoadFromBuffer, kStolenBytes, reinterpret_cast<const void*>(&LoadFromBuffer),
                            g_original);
    }

}
