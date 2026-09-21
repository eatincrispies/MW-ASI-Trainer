#include "AIPursuitOnTask.hpp"

#include "../dllmain.hpp"
#include "IRBVehicleHandle.hpp"
#include "ISimable.hpp"

namespace AIPursuitOnTask {

    namespace {

        constexpr Memory::Pattern kOnTask =
            Memory::ParsePattern("8B 44 24 04 83 EC 44 53 55 56 8B F1 3B 46 50 57 75 ?? "
                                 "D9 86 20 01 00 00 D8 86 1C 01 00 00");

        constexpr std::size_t    kStolenBytes      = 7;
        constexpr std::ptrdiff_t kTaskable         = 0x008;
        constexpr std::ptrdiff_t kIPursuit         = 0x048;
        constexpr std::ptrdiff_t kTarget           = 0x0BC;
        constexpr std::ptrdiff_t kBustTimer        = 0x124;
        constexpr std::ptrdiff_t kCoolDownRequired = 0x14C;
        constexpr std::ptrdiff_t kCoolDownHidden   = 0x16C;
        constexpr std::ptrdiff_t kInCoolDown       = 0x174;
        constexpr std::ptrdiff_t kIsPlayerPursuit  = 0x8C;
        constexpr std::ptrdiff_t kTargetSimable    = 0x1C;
        constexpr std::ptrdiff_t kGetMass          = 0x18;
        constexpr float          kTankMassScale    = 10.0f;

        using OnTaskFn = bool(__thiscall*)(void*, void*, float);

        std::uintptr_t g_original = 0;
        Options                     g_options{};
        ScopedHook g_detour;

        void ApplyTankMode(std::byte* pursuit) {
            void* const target = Game::Field<void*>(pursuit, kTarget);
            if (target == nullptr) return;

            void* const simable   = Game::Field<void*>(target, kTargetSimable);
            void* const rbVehicle = IRBVehicleHandle::Get(simable);
            void* const rigidBody = ISimable::GetRigidBody(simable);
            if (rbVehicle == nullptr || rigidBody == nullptr) return;

            const float mass = Game::CallVirtual<float>(rigidBody, kGetMass);
            IRBVehicleHandle::SetCollisionMass(rbVehicle, mass * kTankMassScale);
        }

        void AfterTask(std::byte* pursuit) {
            if (!Game::CallVirtual<bool>(pursuit + kIPursuit, kIsPlayerPursuit)) return;

            if (g_options.bustProof) Game::Field<float>(pursuit, kBustTimer) = 0.0f;

            if (g_options.instantCooldown && Game::Field<std::uint8_t>(pursuit, kInCoolDown) != 0) {
                const float required = Game::Field<float>(pursuit, kCoolDownRequired);
                float& hidden        = Game::Field<float>(pursuit, kCoolDownHidden);
                if (hidden < required) hidden = required;
            }

            if (g_options.tankMode) ApplyTankMode(pursuit);
        }

        bool __fastcall OnTask(void* taskable, void*, void* task, float deltaTime) {
            const bool result = reinterpret_cast<OnTaskFn>(g_original)(taskable, task, deltaTime);

            std::byte* const pursuit = static_cast<std::byte*>(taskable) - kTaskable;
            Memory::Guarded([&] { AfterTask(pursuit); });
            return result;
        }

    }

    Options Install(Options wanted) noexcept {
        if (wanted.tankMode && !IRBVehicleHandle::Resolve()) wanted.tankMode = false;
        if (!wanted.bustProof && !wanted.instantCooldown && !wanted.tankMode) return {};

        g_options = wanted;
        if (!Hook::Detour(g_detour, kOnTask, kStolenBytes, reinterpret_cast<const void*>(&OnTask), g_original)) return {};
        return wanted;
    }

}
