#include "AICopManagerUpdatePursuits.hpp"

#include "../dllmain.hpp"
#include "ICopMgrInstance.hpp"

#include <algorithm>

namespace AICopManagerUpdatePursuits {

    namespace {

        constexpr Memory::Pattern kUpdatePursuits =
            Memory::ParsePattern("6A FF 68 ?? ?? ?? ?? 64 A1 00 00 00 00 50 64 89 25 00 00 00 00 83 EC 50 "
                                 "53 55 56 57 8B F9 8B AF C8 00 00 00 8B 87 D0 00 00 00 8B CD 33 F6");

        constexpr Memory::Pattern kPursuitList =
            Memory::ParsePattern("8B 87 28 01 00 00 8B 08 3B C8 C6 44 24 13 00 89 4C 24 18");

        constexpr std::size_t    kStolenBytes      = 7;
        constexpr std::ptrdiff_t kPursuitListField = 2;
        constexpr std::ptrdiff_t kNodeNext         = 0x0;
        constexpr std::ptrdiff_t kNodeValue        = 0x8;
        constexpr std::ptrdiff_t kIsPlayerPursuit  = 0x8C;
        constexpr float          kTopUpDelay       = 5.0f;
        constexpr float          kMaxStep          = 0.25f;

        using UpdatePursuitsFn = void(__thiscall*)(void*);
        using SpawnHelicopterFn = bool(__thiscall*)(void*, void*);

        std::uintptr_t g_original        = 0;
        std::uintptr_t g_spawnHelicopter = 0;
        std::ptrdiff_t g_pursuits        = 0;
        int            g_maxHelicopters  = 1;
        float          g_waited          = 0.0f;
        std::int64_t   g_lastTick        = 0;
        double         g_frequency       = 1.0;
        ScopedHook     g_detour;

        float Step() noexcept {
            LARGE_INTEGER now{};
            QueryPerformanceCounter(&now);
            const float step = g_lastTick == 0
                                   ? 0.0f
                                   : static_cast<float>(static_cast<double>(now.QuadPart - g_lastTick) / g_frequency);
            g_lastTick = now.QuadPart;
            return std::clamp(step, 0.0f, kMaxStep);
        }

        void* PlayerPursuit(void* manager) {
            auto* const head = Game::Field<std::byte*>(manager, g_pursuits);
            if (head == nullptr) return nullptr;

            for (auto* node = Game::Field<std::byte*>(head, kNodeNext); node != nullptr && node != head;
                 node = Game::Field<std::byte*>(node, kNodeNext)) {
                void* const pursuit = Game::Field<void*>(node, kNodeValue);
                if (pursuit != nullptr && Game::CallVirtual<bool>(pursuit, kIsPlayerPursuit)) return pursuit;
            }
            return nullptr;
        }

        void TopUp(void* manager) {
            const float step   = Step();
            const int   active = ICopMgrInstance::ActiveHelicopters().value_or(0);
            if (active < 1 || active >= g_maxHelicopters) {
                g_waited = 0.0f;
                return;
            }

            g_waited += step;
            if (g_waited < kTopUpDelay) return;
            g_waited = 0.0f;

            if (void* const pursuit = PlayerPursuit(manager)) {
                reinterpret_cast<SpawnHelicopterFn>(g_spawnHelicopter)(manager, pursuit);
            }
        }

        void __fastcall UpdatePursuits(void* manager, void*) {
            reinterpret_cast<UpdatePursuitsFn>(g_original)(manager);
            Memory::Guarded([&] { TopUp(manager); });
        }

    }

    bool InstallHelicopterTopUp(std::uintptr_t spawnHelicopter, int maxHelicopters) noexcept {
        if (spawnHelicopter == 0) return false;

        const auto field = Hook::Operand(kPursuitList, kPursuitListField);
        if (!field || *field == 0) return false;

        LARGE_INTEGER frequency{};
        if (QueryPerformanceFrequency(&frequency) && frequency.QuadPart > 0) {
            g_frequency = static_cast<double>(frequency.QuadPart);
        }

        g_pursuits        = static_cast<std::ptrdiff_t>(*field);
        g_spawnHelicopter = spawnHelicopter;
        g_maxHelicopters  = maxHelicopters;
        return Hook::Detour(g_detour, kUpdatePursuits, kStolenBytes, reinterpret_cast<const void*>(&UpdatePursuits),
                            g_original);
    }

    void RemoveHelicopterTopUp() noexcept {
        g_detour.Reset();
    }

}
