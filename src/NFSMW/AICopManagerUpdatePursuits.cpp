#include "AICopManagerUpdatePursuits.hpp"

#include "../dllmain.hpp"
#include "ICopMgrInstance.hpp"
#include "IRBVehicleHandle.hpp"
#include "ISimable.hpp"

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
        constexpr std::ptrdiff_t kVehicles         = 0xC8;
        constexpr std::ptrdiff_t kVehicleCount     = 0xD0;
        constexpr std::ptrdiff_t kIsUnavailable    = 0x7C;
        constexpr std::ptrdiff_t kIsActive         = 0x88;
        constexpr std::ptrdiff_t kGetSimable       = 0x04;
        constexpr std::ptrdiff_t kGetMass          = 0x18;
        constexpr float          kPaperWeightScale = 0.1f;
        constexpr float          kTopUpDelay       = 5.0f;
        constexpr float          kMaxStep          = 0.25f;

        using UpdatePursuitsFn  = void(__thiscall*)(void*);
        using SpawnHelicopterFn = bool(__thiscall*)(void*, void*);

        std::uintptr_t g_original        = 0;
        std::uintptr_t g_spawnHelicopter = 0;
        std::ptrdiff_t g_pursuits        = 0;
        int            g_maxHelicopters  = 1;
        bool           g_paperWeight     = false;
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

        void LightenCops(void* manager) {
            for (int index = 0; index < Game::Field<int>(manager, kVehicleCount); ++index) {
                void** const vehicles = Game::Field<void**>(manager, kVehicles);
                void* const  vehicle  = vehicles != nullptr ? vehicles[index] : nullptr;
                if (vehicle == nullptr || Game::CallVirtual<bool>(vehicle, kIsUnavailable) ||
                    !Game::CallVirtual<bool>(vehicle, kIsActive)) {
                    continue;
                }

                void* const simable   = Game::CallVirtual<void*>(vehicle, kGetSimable);
                void* const rbVehicle = IRBVehicleHandle::Get(simable);
                void* const rigidBody = ISimable::GetRigidBody(simable);
                if (rbVehicle == nullptr || rigidBody == nullptr) continue;

                const float mass = Game::CallVirtual<float>(rigidBody, kGetMass);
                IRBVehicleHandle::SetCollisionMass(rbVehicle, mass * kPaperWeightScale);
            }
        }

        void __fastcall UpdatePursuits(void* manager, void*) {
            reinterpret_cast<UpdatePursuitsFn>(g_original)(manager);
            Memory::Guarded([&] {
                if (g_maxHelicopters > 1) TopUp(manager);
                if (g_paperWeight) LightenCops(manager);
            });
        }

        bool Engage() noexcept {
            if (g_detour) return true;

            LARGE_INTEGER frequency{};
            if (QueryPerformanceFrequency(&frequency) && frequency.QuadPart > 0) {
                g_frequency = static_cast<double>(frequency.QuadPart);
            }
            return Hook::Detour(g_detour, kUpdatePursuits, kStolenBytes,
                                reinterpret_cast<const void*>(&UpdatePursuits), g_original);
        }

    }

    bool InstallHelicopterTopUp(std::uintptr_t spawnHelicopter, int maxHelicopters) noexcept {
        if (spawnHelicopter == 0) return false;

        const auto field = Hook::Operand(kPursuitList, kPursuitListField);
        if (!field || *field == 0 || !Engage()) return false;

        g_pursuits        = static_cast<std::ptrdiff_t>(*field);
        g_spawnHelicopter = spawnHelicopter;
        g_maxHelicopters  = maxHelicopters;
        return true;
    }

    bool InstallPaperWeightCops() noexcept {
        if (!IRBVehicleHandle::Resolve() || !Engage()) return false;

        g_paperWeight = true;
        return true;
    }

    void RemoveHelicopterTopUp() noexcept {
        g_maxHelicopters = 1;
        if (!g_paperWeight) g_detour.Reset();
    }

}
