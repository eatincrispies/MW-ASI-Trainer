#include "ICopMgrInstance.hpp"

#include "../dllmain.hpp"

namespace ICopMgrInstance {

    namespace {

        constexpr Memory::Pattern kCopManagerConstructor =
            Memory::ParsePattern("8D 6E ?? 55 68 ?? ?? ?? ?? C7 45 00 ?? ?? ?? ?? 89 4D 04 E8 ?? ?? ?? ?? "
                                 "8D 45 08 3B C3 74 0A 83 C0 F8 A3 ?? ?? ?? ?? EB 06 89 1D ?? ?? ?? ?? "
                                 "C7 45 00 ?? ?? ?? ?? 3B F3 C6 44 24 20 02");

        constexpr Memory::Pattern kHelicopterSpawned =
            Memory::ParsePattern("8B 44 24 34 FF 80 ?? ?? ?? ?? 8D 8C 24 88 00 00 00");

        constexpr std::ptrdiff_t kConstructorBase         = 2;
        constexpr std::ptrdiff_t kConstructorInstanceSave = 35;
        constexpr std::ptrdiff_t kActiveHelicoptersField  = 6;

        std::uintptr_t g_instance          = 0;
        std::uintptr_t g_base              = 0;
        std::uintptr_t g_activeHelicopters = 0;

    }

    bool Resolve() noexcept {
        if (g_instance != 0) return true;

        const auto constructor = Scan::Find(kCopManagerConstructor);
        const auto helicopters = Hook::Operand(kHelicopterSpawned, kActiveHelicoptersField);
        if (!constructor || !helicopters) return false;

        const auto base     = Memory::Read<std::uint8_t>(*constructor + kConstructorBase);
        const auto instance = Scan::Absolute(*constructor + kConstructorInstanceSave);
        if (!base || !instance || *instance == 0) return false;

        g_base              = *base;
        g_activeHelicopters = *helicopters;
        g_instance          = *instance;
        return true;
    }

    std::optional<int> ActiveHelicopters() noexcept {
        const auto instance = Memory::Read<std::uintptr_t>(g_instance);
        if (!instance || *instance == 0) return std::nullopt;
        return Memory::Read<int>(*instance - g_base + g_activeHelicopters);
    }

}
