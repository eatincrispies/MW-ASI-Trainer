#include "FEDatabase.hpp"

#include "../dllmain.hpp"

namespace FEDatabase {

    namespace {

        constexpr Memory::Pattern kCashMarkerAward =
            Memory::ParsePattern("8B 0D ?? ?? ?? ?? 8B 41 10 8B 0C 96 8B 90 B4 00 00 00 05 A8 00 00 00 "
                                 "03 D1 89 50 0C");

        constexpr std::ptrdiff_t kDatabaseRead = 2;
        constexpr std::ptrdiff_t kUserProfile  = 0x10;
        constexpr std::ptrdiff_t kCash         = 0xB4;
        constexpr std::uint32_t  kFullWallet   = 9'999'999;

        std::uintptr_t g_database     = 0;
        bool           g_infiniteCash = false;

    }

    bool EnableInfiniteCash() noexcept {
        if (g_database == 0) {
            const auto database = Hook::Operand(kCashMarkerAward, kDatabaseRead);
            if (!database || *database == 0) return false;
            g_database = *database;
        }

        g_infiniteCash = true;
        return true;
    }

    void Tick() noexcept {
        if (!g_infiniteCash) return;

        const auto database = Memory::Read<std::uintptr_t>(g_database);
        if (!database || *database == 0) return;

        const auto profile = Memory::Read<std::uintptr_t>(*database + kUserProfile);
        if (!profile || *profile == 0) return;

        const auto cash = Memory::Read<std::uint32_t>(*profile + kCash);
        if (!cash || *cash >= kFullWallet) return;

        Memory::Guarded([&] { *reinterpret_cast<std::uint32_t*>(*profile + kCash) = kFullWallet; });
    }

}
