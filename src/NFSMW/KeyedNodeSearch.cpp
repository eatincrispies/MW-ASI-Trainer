#include "KeyedNodeSearch.hpp"

#include "../dllmain.hpp"

namespace KeyedNodeSearch {

    namespace {

        constexpr Memory::Pattern kRacerVehicleLookup =
            Memory::ParsePattern("8B 01 85 C0 74 3E 8B 0D ?? ?? ?? ?? 50 A1 ?? ?? ?? ?? 8D 14 C8 52 50 E8 ?? ?? ?? ?? "
                                 "83 C4 0C 85 C0 74 21 8B 40 04 85 C0 74 1A 8B 48 04 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? "
                                 "85 C0 74 09 8B 10 6A 01");

        constexpr std::ptrdiff_t kCountRead   = 8;
        constexpr std::ptrdiff_t kTableRead   = 14;
        constexpr std::ptrdiff_t kSearchCall  = 24;
        constexpr std::ptrdiff_t kNodeSimable = 0x04;
        constexpr std::size_t    kEntrySize   = 8;

        using SearchFn = void*(__cdecl*)(void*, void*, std::uint32_t);

        std::uintptr_t g_count  = 0;
        std::uintptr_t g_table  = 0;
        std::uintptr_t g_search = 0;

    }

    bool Resolve() noexcept {
        if (g_search != 0) return true;

        const auto count  = Hook::Operand(kRacerVehicleLookup, kCountRead);
        const auto table  = Hook::Operand(kRacerVehicleLookup, kTableRead);
        const auto search = Hook::Operand(kRacerVehicleLookup, kSearchCall, true);
        if (!count || !table || !search || !Memory::IsExecutable(reinterpret_cast<const void*>(*search))) return false;

        g_count  = *count;
        g_table  = *table;
        g_search = *search;
        return true;
    }

    void* FindSimable(std::uint32_t handle) {
        if (handle == 0 || g_search == 0) return nullptr;

        const auto table = Memory::Read<std::uintptr_t>(g_table);
        const auto count = Memory::Read<std::uint32_t>(g_count);
        if (!table || !count || *table == 0) return nullptr;

        auto* const begin = reinterpret_cast<std::byte*>(*table);
        auto* const end   = begin + static_cast<std::size_t>(*count) * kEntrySize;
        void* const node  = reinterpret_cast<SearchFn>(g_search)(begin, end, handle);
        return node != nullptr ? Game::Field<void*>(node, kNodeSimable) : nullptr;
    }

}
