#include "../dllmain.hpp"

#include <algorithm>
#include <bit>
#include <climits>
#include <cmath>

namespace {

    class IDamageable : public UTL::COM::IUnknown {
    public:
        [[nodiscard]] static bool Resolve() noexcept;
        [[nodiscard]] static std::uintptr_t _IHandle() noexcept { return sHandle; }

        [[nodiscard]] bool IsDestroyed() const {
            return Game::CallVirtual<bool>(const_cast<IDamageable*>(this), kIsDestroyed);
        }

        void Destroy() { Game::CallVirtual<void>(this, kDestroy); }

        static void Destroy(const UTL::COM::IUnknown* object) {
            IDamageable* const damageable = object != nullptr ? object->QueryInterface<IDamageable>() : nullptr;
            if (damageable != nullptr && !damageable->IsDestroyed()) damageable->Destroy();
        }

    private:
        static constexpr std::ptrdiff_t kIsDestroyed = 0x18;
        static constexpr std::ptrdiff_t kDestroy     = 0x1C;

        static inline std::uintptr_t sHandle = 0;
    };

    class IRBVehicle : public UTL::COM::IUnknown {
    public:
        [[nodiscard]] static bool Resolve() noexcept;
        [[nodiscard]] static std::uintptr_t _IHandle() noexcept { return sHandle; }

        void SetCollisionMass(float mass) { Game::CallVirtual<void>(this, kSetCollisionMass, mass); }

        static void ScaleCollisionMass(const ISimable* simable, float scale) {
            if (simable == nullptr) return;
            IRBVehicle* const vehicle   = simable->QueryInterface<IRBVehicle>();
            IRigidBody* const rigidBody = simable->GetRigidBody();
            if (vehicle != nullptr && rigidBody != nullptr) vehicle->SetCollisionMass(rigidBody->GetMass() * scale);
        }

    private:
        static constexpr std::ptrdiff_t kSetCollisionMass = 0x04;

        static inline std::uintptr_t sHandle = 0;
    };

    class IAIHelicopter : public UTL::COM::IUnknown {
    public:
        [[nodiscard]] static bool Resolve() noexcept;
        [[nodiscard]] static std::uintptr_t _IHandle() noexcept { return sHandle; }

    private:
        static inline std::uintptr_t sHandle = 0;
    };

    constexpr Memory::Pattern kApplyBreakerZonesIDamageable =
        Memory::ParsePattern("8B 4B 04 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 85 C0 74 07 8B 10 8B C8 FF 52 1C "
                             "8B 87 D0 00 00 00");

    constexpr Memory::Pattern kEPursuitBreakerIRBVehicle =
        Memory::ParsePattern("8B 4D 04 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B F0 85 F6 74 ?? 84 DB 74 ?? "
                             "8B 45 00 8B CD FF 50 54 8B 10 8B 1E 8B C8 FF 52 18");

    constexpr Memory::Pattern kUnSpawnIAIHelicopter =
        Memory::ParsePattern("56 8B F1 8B 46 E8 8B 48 04 57 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 85 C0 74 0A C7 05");

    constexpr std::ptrdiff_t kIDamageableHandlePush   = 4;
    constexpr std::ptrdiff_t kIRBVehicleHandlePush    = 4;
    constexpr std::ptrdiff_t kIAIHelicopterHandlePush = 11;

    bool ResolveHandle(std::uintptr_t& handle, const Memory::Pattern& pattern, std::ptrdiff_t push) noexcept {
        if (handle != 0) return true;
        if (!UTL::COM::IUnknown::Resolve()) return false;

        const auto found = Hook::Operand(pattern, push);
        if (!found || *found == 0) return false;

        handle = *found;
        return true;
    }

    bool IDamageable::Resolve() noexcept {
        return ResolveHandle(sHandle, kApplyBreakerZonesIDamageable, kIDamageableHandlePush);
    }

    bool IRBVehicle::Resolve() noexcept {
        return ResolveHandle(sHandle, kEPursuitBreakerIRBVehicle, kIRBVehicleHandlePush);
    }

    bool IAIHelicopter::Resolve() noexcept {
        return ResolveHandle(sHandle, kUnSpawnIAIHelicopter, kIAIHelicopterHandlePush);
    }

    constexpr std::ptrdiff_t kIPursuitIsPlayerPursuit = 0x8C;

    bool IsPlayerPursuit(void* pursuit) {
        return Game::CallVirtual<bool>(pursuit, kIPursuitIsPlayerPursuit);
    }

    namespace AIPursuit {

        constexpr Memory::Pattern kOnTask =
            Memory::ParsePattern("8B 44 24 04 83 EC 44 53 55 56 8B F1 3B 46 50 57 75 ?? "
                                 "D9 86 20 01 00 00 D8 86 1C 01 00 00");

        constexpr Memory::Pattern kRequestRoadBlock =
            Memory::ParsePattern("51 56 8B F1 8A 86 E8 00 00 00 84 C0 0F 85 ?? ?? ?? ?? 8A 86 E9 00 00 00 "
                                 "84 C0 0F 85 ?? ?? ?? ?? 8B 86 84 00 00 00 85 C0");

        constexpr Memory::Pattern kRequestGroundSupport =
            Memory::ParsePattern("56 8B F1 8A 86 E8 00 00 00 84 C0 0F 85 ?? ?? ?? ?? 8A 86 14 01 00 00 "
                                 "84 C0 0F 84 ?? ?? ?? ?? 8A 86 E9 00 00 00");

        constexpr Memory::Pattern kCopRequestHeliCheck =
            Memory::ParsePattern("C6 86 D4 00 00 00 00 75 55 A1 ?? ?? ?? ?? 85 C0 75 4C 8B CD E8");

        constexpr std::size_t    kOnTaskStolen         = 7;
        constexpr std::ptrdiff_t kTaskable             = 0x008;
        constexpr std::ptrdiff_t kIPursuit             = 0x048;
        constexpr std::ptrdiff_t kTarget               = 0x0BC;
        constexpr std::ptrdiff_t kBustedTimer          = 0x124;
        constexpr std::ptrdiff_t kCoolDownTimeRequired = 0x14C;
        constexpr std::ptrdiff_t kCoolDownTime         = 0x16C;
        constexpr std::ptrdiff_t kInCoolDown           = 0x174;
        constexpr std::ptrdiff_t kTargetSimable        = 0x1C;
        constexpr float          kTankMassScale        = 10.0f;
        constexpr std::ptrdiff_t kHeliCheck            = 9;
        constexpr std::ptrdiff_t kgHeliVehicleOperand  = 10;
        constexpr std::size_t    kHeliCheckLength      = 5;

        constexpr std::array<std::uint8_t, 3> kReturnZero{ 0x33, 0xC0, 0xC3 };
        constexpr std::array<std::uint8_t, 3> kReturnTrue{ 0xB0, 0x01, 0xC3 };

        constexpr Memory::Pattern kGetIsAJerk =
            Memory::ParsePattern("8A 81 38 02 00 00 C3 CC CC CC CC CC CC CC CC CC D9 81 20 02 00 00");

        constexpr Memory::Pattern kOnTaskBustedDistance =
            Memory::ParsePattern("84 DB D9 05 ?? ?? ?? ?? 74 0A DD D8 D9 05 ?? ?? ?? ?? EB 16");

        constexpr std::ptrdiff_t kBustedCopDistanceOperand        = 4;
        constexpr std::ptrdiff_t kBustedCopDistanceFlashingOperand = 14;
        constexpr float          kFlashingDistanceScale           = 6.0f;

        struct Cheats {
            bool bustProof       = false;
            bool instantCooldown = false;
            bool tankMode        = false;
        };

        using OnTaskFn = bool(__thiscall*)(void*, void*, float);

        std::uintptr_t             g_OnTask = 0;
        Cheats                     g_cheats{};
        ScopedHook                 g_OnTaskHook;
        std::optional<ScopedPatch> g_RequestRoadBlock;
        std::optional<ScopedPatch> g_RequestGroundSupport;
        ScopedHook                 g_CopRequestHeliCheck;
        std::optional<ScopedPatch> g_GetIsAJerk;
        std::optional<ScopedPatch> g_BustedCopDistance;
        std::optional<ScopedPatch> g_BustedCopDistanceFlashing;
        float                      g_bustedCopDistance         = 15.0f;
        float                      g_bustedCopDistanceFlashing = 90.0f;

        void ApplyPlayerPursuitCheats(std::byte* pursuit) {
            if (!IsPlayerPursuit(pursuit + kIPursuit)) return;

            if (g_cheats.bustProof) Game::Field<float>(pursuit, kBustedTimer) = 0.0f;

            if (g_cheats.instantCooldown && Game::Field<std::uint8_t>(pursuit, kInCoolDown) != 0) {
                const float required = Game::Field<float>(pursuit, kCoolDownTimeRequired);
                float& elapsed       = Game::Field<float>(pursuit, kCoolDownTime);
                if (elapsed < required) elapsed = required;
            }

            if (g_cheats.tankMode) {
                void* const target = Game::Field<void*>(pursuit, kTarget);
                if (target != nullptr) IRBVehicle::ScaleCollisionMass(Game::Field<ISimable*>(target, kTargetSimable), kTankMassScale);
            }
        }

        bool __fastcall OnTaskHook(void* taskable, void*, void* task, float deltaTime) {
            const bool result = reinterpret_cast<OnTaskFn>(g_OnTask)(taskable, task, deltaTime);

            std::byte* const pursuit = static_cast<std::byte*>(taskable) - kTaskable;
            Memory::Guarded([&] { ApplyPlayerPursuitCheats(pursuit); });
            return result;
        }

        Cheats InstallOnTask(Cheats wanted) noexcept {
            if (wanted.tankMode && !IRBVehicle::Resolve()) wanted.tankMode = false;
            if (!wanted.bustProof && !wanted.instantCooldown && !wanted.tankMode) return {};

            g_cheats = wanted;
            if (!Hook::Detour(g_OnTaskHook, kOnTask, kOnTaskStolen, reinterpret_cast<const void*>(&OnTaskHook), g_OnTask)) {
                return {};
            }
            return wanted;
        }

        bool InstallAggressiveCops() noexcept {
            return Hook::Patch(g_GetIsAJerk, kGetIsAJerk, 0, kReturnTrue);
        }

        bool InstallBustDistance(float distance) noexcept {
            const auto site = Scan::Find(kOnTaskBustedDistance);
            if (!site) return false;

            g_bustedCopDistance         = distance;
            g_bustedCopDistanceFlashing = distance * kFlashingDistanceScale;
            const auto nearOperand = std::bit_cast<std::array<std::uint8_t, 4>>(reinterpret_cast<std::uintptr_t>(&g_bustedCopDistance));
            const auto farOperand  = std::bit_cast<std::array<std::uint8_t, 4>>(reinterpret_cast<std::uintptr_t>(&g_bustedCopDistanceFlashing));
            g_BustedCopDistance.emplace(*site + kBustedCopDistanceOperand, nearOperand);
            g_BustedCopDistanceFlashing.emplace(*site + kBustedCopDistanceFlashingOperand, farOperand);
            if (*g_BustedCopDistance && *g_BustedCopDistanceFlashing) return true;

            g_BustedCopDistanceFlashing.reset();
            g_BustedCopDistance.reset();
            return false;
        }

        bool InstallDisableRoadblocks() noexcept {
            return Hook::Patch(g_RequestRoadBlock, kRequestRoadBlock, 0, kReturnZero);
        }

        bool InstallDisableReinforcements() noexcept {
            return Hook::Patch(g_RequestGroundSupport, kRequestGroundSupport, 0, kReturnZero);
        }

        bool InstallCopRequestHeliLimit(std::uintptr_t gHeliVehicle, const void* gate) noexcept {
            const auto match = Scan::Find(kCopRequestHeliCheck);
            if (!match) return false;

            const auto read = Scan::Absolute(*match + kgHeliVehicleOperand);
            if (!read || *read != gHeliVehicle) return false;

            return Hook::Thunk(g_CopRequestHeliCheck, *match + kHeliCheck, kHeliCheckLength, gate);
        }

    }

    namespace AIVehiclePursuit {

        constexpr Memory::Pattern kGetChicken =
            Memory::ParsePattern("8A 41 0A C3 CC CC CC CC CC CC CC CC CC CC CC CC 8A 44 24 04");

        constexpr std::array<std::uint8_t, 3> kReturnFalse{ 0x32, 0xC0, 0xC3 };

        std::optional<ScopedPatch> g_GetChicken;

        bool InstallNeverChicken() noexcept {
            return Hook::Patch(g_GetChicken, kGetChicken, 0, kReturnFalse);
        }

    }

    namespace GManager {

        constexpr Memory::Pattern kUpdateTriggerAvailabilityInPursuit =
            Memory::ParsePattern("8A 4C 24 0A 74 0B 84 C9 75 07 B8 01 00 00 00 EB 02 33 C0 84 DB");

        constexpr std::array<std::uint8_t, 4> kNotInPursuit{ 0x32, 0xC9, 0x90, 0x90 };

        std::optional<ScopedPatch> g_InPursuit;

        bool InstallDisableSafehouseBust() noexcept {
            return Hook::Patch(g_InPursuit, kUpdateTriggerAvailabilityInPursuit, 0, kNotInPursuit);
        }

    }

    bool InstallAggressiveCopAI() noexcept {
        if (AIPursuit::InstallAggressiveCops() && AIVehiclePursuit::InstallNeverChicken()) return true;

        AIVehiclePursuit::g_GetChicken.reset();
        AIPursuit::g_GetIsAJerk.reset();
        return false;
    }

    namespace DamageRacer {

        constexpr Memory::Pattern kPuncture =
            Memory::ParsePattern("64 A1 00 00 00 00 6A FF 68 ?? ?? ?? ?? 50 64 89 25 00 00 00 00 "
                                 "56 57 8B 7C 24 18 83 FF 04 8B F1 73 ?? 8A 44 37 1C");

        constexpr std::size_t    kPunctureStolen = 6;
        constexpr std::ptrdiff_t kVehicle        = -0x70;

        using PunctureFn = void(__thiscall*)(void*, unsigned);

        std::uintptr_t g_Puncture = 0;
        ScopedHook     g_PunctureHook;

        void __fastcall PunctureHook(void* damage, void*, unsigned wheel) {
            bool player = false;
            Memory::Guarded([&] { player = IVehicle::IsDriver(Game::Field<IVehicle*>(damage, kVehicle), DRIVER_HUMAN); });

            if (!player) reinterpret_cast<PunctureFn>(g_Puncture)(damage, wheel);
        }

        bool InstallSpikeProof() noexcept {
            return Hook::Detour(g_PunctureHook, kPuncture, kPunctureStolen, reinterpret_cast<const void*>(&PunctureHook),
                                g_Puncture);
        }

    }

    namespace RBVehicle {

        constexpr Memory::Pattern kCanCollideWith =
            Memory::ParsePattern("8A 81 70 01 00 00 84 C0 74 1C 8B 41 78 8B 08 8A 51 1D");

        constexpr std::size_t    kCanCollideWithStolen = 6;
        constexpr std::ptrdiff_t kOwner                = 0x34;

        using CanCollideWithFn = bool(__thiscall*)(void*, void*);

        std::uintptr_t g_CanCollideWith = 0;
        ScopedHook     g_CanCollideWithHook;

        bool IsPlayerAgainstCop(void* body, void* other) {
            const ISimable* const first  = Game::Field<ISimable*>(body, kOwner);
            const ISimable* const second = Game::Field<ISimable*>(other, kOwner);
            if (first == nullptr || second == nullptr) return false;

            if (first->IsPlayer()) return IVehicle::IsDriver(second, DRIVER_COP);
            if (second->IsPlayer()) return IVehicle::IsDriver(first, DRIVER_COP);
            return false;
        }

        bool __fastcall CanCollideWithHook(void* body, void*, void* other) {
            if (!reinterpret_cast<CanCollideWithFn>(g_CanCollideWith)(body, other)) return false;

            bool ghosted = false;
            Memory::Guarded([&] { ghosted = IsPlayerAgainstCop(body, other); });
            return !ghosted;
        }

        bool InstallGhostCops() noexcept {
            if (!IVehicle::Resolve()) return false;
            return Hook::Detour(g_CanCollideWithHook, kCanCollideWith, kCanCollideWithStolen,
                                reinterpret_cast<const void*>(&CanCollideWithHook), g_CanCollideWith);
        }

    }

    namespace ICopMgr {

        constexpr Memory::Pattern kAICopManagerConstructor =
            Memory::ParsePattern("8D 6E ?? 55 68 ?? ?? ?? ?? C7 45 00 ?? ?? ?? ?? 89 4D 04 E8 ?? ?? ?? ?? "
                                 "8D 45 08 3B C3 74 0A 83 C0 F8 A3 ?? ?? ?? ?? EB 06 89 1D ?? ?? ?? ?? "
                                 "C7 45 00 ?? ?? ?? ?? 3B F3 C6 44 24 20 02");

        constexpr Memory::Pattern kSpawnPursuitHelicopterCount =
            Memory::ParsePattern("8B 44 24 34 FF 80 ?? ?? ?? ?? 8D 8C 24 88 00 00 00");

        constexpr std::ptrdiff_t kICopMgrOffsetOperand = 2;
        constexpr std::ptrdiff_t kmInstanceOperand     = 35;
        constexpr std::ptrdiff_t kNumHelicoptersOperand = 6;

        std::uintptr_t g_mInstance      = 0;
        std::uintptr_t g_ICopMgrOffset  = 0;
        std::uintptr_t g_NumHelicopters = 0;

        bool Resolve() noexcept {
            if (g_mInstance != 0) return true;

            const auto constructor = Scan::Find(kAICopManagerConstructor);
            const auto helicopters = Hook::Operand(kSpawnPursuitHelicopterCount, kNumHelicoptersOperand);
            if (!constructor || !helicopters) return false;

            const auto offset   = Memory::Read<std::uint8_t>(*constructor + kICopMgrOffsetOperand);
            const auto instance = Scan::Absolute(*constructor + kmInstanceOperand);
            if (!offset || !instance || *instance == 0) return false;

            g_ICopMgrOffset  = *offset;
            g_NumHelicopters = *helicopters;
            g_mInstance      = *instance;
            return true;
        }

        std::optional<int> GetNumHelicopters() noexcept {
            const auto instance = Memory::Read<std::uintptr_t>(g_mInstance);
            if (!instance || *instance == 0) return std::nullopt;
            return Memory::Read<int>(*instance - g_ICopMgrOffset + g_NumHelicopters);
        }

    }

    namespace AICopManager {

        constexpr Memory::Pattern kSpawnPursuitHelicopter =
            Memory::ParsePattern("6A FF 68 ?? ?? ?? ?? 64 A1 00 00 00 00 50 64 89 25 00 00 00 00 81 EC A0 03 00 00 "
                                 "A1 ?? ?? ?? ?? 85 C0 53 55 56 57 89 4C 24 34 0F 85");

        constexpr Memory::Pattern kUpdatePursuits =
            Memory::ParsePattern("6A FF 68 ?? ?? ?? ?? 64 A1 00 00 00 00 50 64 89 25 00 00 00 00 83 EC 50 "
                                 "53 55 56 57 8B F9 8B AF C8 00 00 00 8B 87 D0 00 00 00 8B CD 33 F6");

        constexpr Memory::Pattern kUpdatePursuitsPursuitList =
            Memory::ParsePattern("8B 87 28 01 00 00 8B 08 3B C8 C6 44 24 13 00 89 4C 24 18");

        constexpr Memory::Pattern kApplyBreakerZones =
            Memory::ParsePattern("83 EC 10 57 8B F9 8B 47 7C 85 C0 0F 84 ?? ?? ?? ?? 8B 87 D0 00 00 00 "
                                 "53 55 8B AF C8 00 00 00");

        constexpr Memory::Pattern kUpdateFuelHeliExit =
            Memory::ParsePattern("68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 89 44 24 0C 83 C4 04 8D 44 24 08 50 8B CE "
                                 "E8 ?? ?? ?? ?? 5E C2 04 00");

        constexpr std::array<std::uint8_t, 5> kReturnFalse{ 0x32, 0xC0, 0xC2, 0x04, 0x00 };
        constexpr std::ptrdiff_t              kHeliCheck             = 27;
        constexpr std::ptrdiff_t              kgHeliVehicleOperand   = 28;
        constexpr std::size_t                 kHeliCheckLength       = 5;
        constexpr std::size_t                 kUpdatePursuitsStolen  = 7;
        constexpr std::size_t                 kApplyBreakerZonesStolen = 6;
        constexpr std::ptrdiff_t              kPursuitListOperand    = 2;
        constexpr std::ptrdiff_t              kAIGoalHeliExitOperand = 1;
        constexpr std::ptrdiff_t              kstringhash32Call      = 6;
        constexpr std::ptrdiff_t              kSetGoalCall           = 25;
        constexpr std::ptrdiff_t              kListNext              = 0x0;
        constexpr std::ptrdiff_t              kListValue             = 0x8;
        constexpr std::ptrdiff_t              kNumBreakerZones       = 0x7C;
        constexpr std::ptrdiff_t              kVehicles              = 0xC8;
        constexpr std::ptrdiff_t              kNumVehicles           = 0xD0;
        constexpr std::ptrdiff_t              kIAIHelicopter         = 0x7A4;
        constexpr std::ptrdiff_t              kHeliFuelTimeRemaining = 0x7D8;
        constexpr float                       kPaperWeightScale      = 0.1f;
        constexpr float                       kHelicopterTopUpDelay  = 5.0f;
        constexpr float                       kMaxTopUpStep          = 0.25f;

        using SpawnPursuitHelicopterFn = bool(__thiscall*)(void*, void*);
        using UpdatePursuitsFn         = void(__thiscall*)(void*);
        using ApplyBreakerZonesFn      = void(__thiscall*)(void*);
        using stringhash32Fn           = std::uint32_t(__cdecl*)(const char*);
        using SetGoalFn                = void(__thiscall*)(void*, const std::uint32_t*);

        std::uintptr_t             g_SpawnPursuitHelicopter = 0;
        std::uintptr_t             g_gHeliVehicle           = 0;
        std::uintptr_t             g_UpdatePursuits         = 0;
        std::uintptr_t             g_ApplyBreakerZones      = 0;
        std::uintptr_t             g_SetGoal                = 0;
        std::ptrdiff_t             g_PursuitList            = 0;
        std::uint32_t              g_AIGoalHeliExit         = 0;
        int                        g_maxHelicopters         = 1;
        bool                       g_paperWeight            = false;
        float                      g_topUpWaited            = 0.0f;
        std::int64_t               g_lastTick               = 0;
        double                     g_tickFrequency          = 1.0;
        int                        g_knownBreakerZones      = 0;
        std::optional<ScopedPatch> g_DisableHelicopter;
        ScopedHook                 g_SpawnHeliCheck;
        ScopedHook                 g_UpdatePursuitsHook;
        ScopedHook                 g_ApplyBreakerZonesHook;

        std::uint32_t __cdecl HelicopterGate() {
            if (const auto active = ICopMgr::GetNumHelicopters()) return *active >= g_maxHelicopters ? 1u : 0u;
            return Memory::Read<std::uint32_t>(g_gHeliVehicle).value_or(0);
        }

        template <class F>
        void ForEachActiveVehicle(void* manager, F&& visit) {
            for (int index = 0; index < Game::Field<int>(manager, kNumVehicles); ++index) {
                IVehicle** const vehicles = Game::Field<IVehicle**>(manager, kVehicles);
                IVehicle* const  vehicle  = vehicles != nullptr ? vehicles[index] : nullptr;
                if (vehicle == nullptr || vehicle->IsDestroyed() || !vehicle->IsActive()) continue;
                visit(vehicle);
            }
        }

        float TopUpStep() noexcept {
            LARGE_INTEGER now{};
            QueryPerformanceCounter(&now);
            const float step = g_lastTick == 0
                                   ? 0.0f
                                   : static_cast<float>(static_cast<double>(now.QuadPart - g_lastTick) / g_tickFrequency);
            g_lastTick = now.QuadPart;
            return std::clamp(step, 0.0f, kMaxTopUpStep);
        }

        void* GetPlayerPursuit(void* manager) {
            auto* const head = Game::Field<std::byte*>(manager, g_PursuitList);
            if (head == nullptr) return nullptr;

            for (auto* node = Game::Field<std::byte*>(head, kListNext); node != nullptr && node != head;
                 node = Game::Field<std::byte*>(node, kListNext)) {
                void* const pursuit = Game::Field<void*>(node, kListValue);
                if (pursuit != nullptr && IsPlayerPursuit(pursuit)) return pursuit;
            }
            return nullptr;
        }

        void TopUpHelicopters(void* manager) {
            const float step   = TopUpStep();
            const int   active = ICopMgr::GetNumHelicopters().value_or(0);
            if (active < 1 || active >= g_maxHelicopters) {
                g_topUpWaited = 0.0f;
                return;
            }

            g_topUpWaited += step;
            if (g_topUpWaited < kHelicopterTopUpDelay) return;
            g_topUpWaited = 0.0f;

            if (void* const pursuit = GetPlayerPursuit(manager)) {
                reinterpret_cast<SpawnPursuitHelicopterFn>(g_SpawnPursuitHelicopter)(manager, pursuit);
            }
        }

        void __fastcall UpdatePursuitsHook(void* manager, void*) {
            reinterpret_cast<UpdatePursuitsFn>(g_UpdatePursuits)(manager);
            Memory::Guarded([&] {
                if (g_maxHelicopters > 1) TopUpHelicopters(manager);
                if (g_paperWeight) {
                    ForEachActiveVehicle(manager, [](IVehicle* vehicle) {
                        IRBVehicle::ScaleCollisionMass(vehicle->GetSimable(), kPaperWeightScale);
                    });
                }
            });
        }

        bool HookUpdatePursuits() noexcept {
            if (g_UpdatePursuitsHook) return true;

            LARGE_INTEGER frequency{};
            if (QueryPerformanceFrequency(&frequency) && frequency.QuadPart > 0) {
                g_tickFrequency = static_cast<double>(frequency.QuadPart);
            }
            return Hook::Detour(g_UpdatePursuitsHook, kUpdatePursuits, kUpdatePursuitsStolen,
                                reinterpret_cast<const void*>(&UpdatePursuitsHook), g_UpdatePursuits);
        }

        void SendHelicopterHome(std::byte* helicopter) {
            Game::Field<float>(helicopter, kHeliFuelTimeRemaining) = 0.0f;
            reinterpret_cast<SetGoalFn>(g_SetGoal)(helicopter, &g_AIGoalHeliExit);
        }

        void __fastcall ApplyBreakerZonesHook(void* manager, void*) {
            Memory::Guarded([&] {
                if (Game::Field<int>(manager, kNumBreakerZones) <= g_knownBreakerZones) return;
                ForEachActiveVehicle(manager, [](IVehicle* vehicle) {
                    if (void* const helicopter = vehicle->QueryInterface<IAIHelicopter>()) {
                        SendHelicopterHome(static_cast<std::byte*>(helicopter) - kIAIHelicopter);
                    } else {
                        IDamageable::Destroy(vehicle);
                    }
                });
            });
            reinterpret_cast<ApplyBreakerZonesFn>(g_ApplyBreakerZones)(manager);
            g_knownBreakerZones = Game::Field<int>(manager, kNumBreakerZones);
        }

        bool InstallDisableHelicopter() noexcept {
            return Hook::Patch(g_DisableHelicopter, kSpawnPursuitHelicopter, 0, kReturnFalse);
        }

        bool InstallMaxHelicopters(int maxHelicopters) noexcept {
            if (!ICopMgr::Resolve()) return false;

            const auto spawner = Scan::Find(kSpawnPursuitHelicopter);
            if (!spawner) return false;

            const auto gHeliVehicle = Scan::Absolute(*spawner + kgHeliVehicleOperand);
            const auto pursuits     = Hook::Operand(kUpdatePursuitsPursuitList, kPursuitListOperand);
            if (!gHeliVehicle || *gHeliVehicle == 0 || !pursuits || *pursuits == 0) return false;

            g_SpawnPursuitHelicopter = *spawner;
            g_gHeliVehicle           = *gHeliVehicle;
            g_PursuitList            = static_cast<std::ptrdiff_t>(*pursuits);
            g_maxHelicopters         = maxHelicopters;
            const void* const gate   = reinterpret_cast<const void*>(&HelicopterGate);

            if (Hook::Thunk(g_SpawnHeliCheck, *spawner + kHeliCheck, kHeliCheckLength, gate) &&
                AIPursuit::InstallCopRequestHeliLimit(g_gHeliVehicle, gate) && HookUpdatePursuits()) {
                return true;
            }

            AIPursuit::g_CopRequestHeliCheck.Reset();
            g_SpawnHeliCheck.Reset();
            g_maxHelicopters = 1;
            return false;
        }

        bool InstallPaperWeightCops() noexcept {
            if (!IRBVehicle::Resolve() || !HookUpdatePursuits()) return false;

            g_paperWeight = true;
            return true;
        }

        bool InstallPursuitBreakerNuke() noexcept {
            if (!IAIHelicopter::Resolve() || !IDamageable::Resolve()) return false;

            const auto exit = Scan::Find(kUpdateFuelHeliExit);
            if (!exit) return false;

            const auto name       = Scan::Absolute(*exit + kAIGoalHeliExitOperand);
            const auto stringHash = Scan::Relative(*exit + kstringhash32Call);
            const auto setGoal    = Scan::Relative(*exit + kSetGoalCall);
            if (!name || !stringHash || !setGoal || !Memory::IsExecutable(reinterpret_cast<const void*>(*stringHash)) ||
                !Memory::IsExecutable(reinterpret_cast<const void*>(*setGoal))) {
                return false;
            }

            g_AIGoalHeliExit = reinterpret_cast<stringhash32Fn>(*stringHash)(reinterpret_cast<const char*>(*name));
            g_SetGoal        = *setGoal;
            return Hook::Detour(g_ApplyBreakerZonesHook, kApplyBreakerZones, kApplyBreakerZonesStolen,
                                reinterpret_cast<const void*>(&ApplyBreakerZonesHook), g_ApplyBreakerZones);
        }

    }

    namespace Minimap {

        constexpr Memory::Pattern kUpdateCopElements =
            Memory::ParsePattern("55 8B EC 83 E4 F8 83 EC 24 53 8B D9 8B 83 C0 00 00 00 56 57 33 FF 83 F8 08");

        constexpr Memory::Pattern kUpdateCopElementsVehicleList =
            Memory::ParsePattern("8B 35 ?? ?? ?? ?? A1 ?? ?? ?? ?? 8D 0C 86 3B F1 0F 84 ?? ?? ?? ?? "
                                 "8D 93 D0 00 00 00 89 54 24 14");

        constexpr Memory::Pattern kGetVehicleVectorsCall =
            Memory::ParsePattern("50 8D 44 24 24 50 8D 4C 24 30 51 E8 ?? ?? ?? ?? 8B 06 8B 48 04 83 C4 0C");

        constexpr Memory::Pattern kUpdateElementArtCall =
            Memory::ParsePattern("8B 8B C8 00 00 00 6A 00 51 8D 54 24 28 52 8D 44 24 34 50 8B CB E8 ?? ?? ?? ??");

        constexpr Memory::Pattern kFEngSetColorCall =
            Memory::ParsePattern("8B 44 24 1C 8B 4C 24 14 8B 11 50 52 E8 ?? ?? ?? ?? 83 C4 08");

        constexpr Memory::Pattern kVehicleClassCHOPPERRead =
            Memory::ParsePattern("FF 50 44 8B 08 3B 0D ?? ?? ?? ?? 74 14 8B 4C 24 18");

        constexpr Memory::Pattern kFEngSetTextureHashCall =
            Memory::ParsePattern("E8 ?? ?? ?? ?? 8B 0B 50 51 E8 ?? ?? ?? ?? 83 C4 20 47 83 C3 04");

        constexpr std::size_t    kUpdateCopElementsStolen = 9;
        constexpr std::ptrdiff_t kVehicleListOperand      = 2;
        constexpr std::ptrdiff_t kVehicleCountOperand     = 7;
        constexpr std::ptrdiff_t kGetVehicleVectorsOperand = 12;
        constexpr std::ptrdiff_t kUpdateElementArtOperand = 22;
        constexpr std::ptrdiff_t kFEngSetColorOperand     = 13;
        constexpr std::ptrdiff_t kCHOPPEROperand          = 7;
        constexpr std::ptrdiff_t kFEngSetTextureHashOperand = 10;

        constexpr std::ptrdiff_t kHelicopterGroup = 0xC8;
        constexpr std::ptrdiff_t kCopElements     = 0xD0;
        constexpr std::size_t    kNumCopElements  = 8;
        constexpr std::ptrdiff_t kGroupChildren   = 0x64;
        constexpr std::ptrdiff_t kFEObjectNext    = 0x04;
        constexpr std::ptrdiff_t kFEObjectType    = 0x18;
        constexpr std::ptrdiff_t kFEObjectFlags   = 0x1C;
        constexpr std::ptrdiff_t kFEImageTexture  = 0x24;
        constexpr std::uint32_t  kFEObjectHidden  = 0x01;
        constexpr int            kFEObjectImage   = 1;
        constexpr std::uint32_t  kIconColor       = 0xFFFFFFFF;

        using UpdateCopElementsFn  = void(__thiscall*)(void*, void*);
        using GetVehicleVectorsFn  = void(__cdecl*)(float*, float*, void*);
        using UpdateElementArtFn   = void(__thiscall*)(void*, const float*, const float*, void*, int);
        using FEngSetColorFn       = void(__cdecl*)(void*, std::uint32_t);
        using FEngSetTextureHashFn = void(__cdecl*)(void*, std::uint32_t);

        struct BorrowedElement {
            void*         element;
            std::uint32_t texture;
        };

        std::uintptr_t                                g_UpdateCopElements  = 0;
        std::uintptr_t                                g_GetVehicleVectors  = 0;
        std::uintptr_t                                g_UpdateElementArt   = 0;
        std::uintptr_t                                g_FEngSetColor       = 0;
        std::uintptr_t                                g_FEngSetTextureHash = 0;
        IVehicle***                                   g_vehicles           = nullptr;
        const int*                                    g_numVehicles        = nullptr;
        const std::uint32_t*                          g_CHOPPER            = nullptr;
        std::array<BorrowedElement, kNumCopElements>  g_borrowed{};
        std::size_t                                   g_numBorrowed = 0;
        ScopedHook                                    g_UpdateCopElementsHook;

        void* CopElement(void* minimap, std::size_t slot) {
            return Game::Field<void*>(minimap, kCopElements + static_cast<std::ptrdiff_t>(slot) * 4);
        }

        std::uint32_t HelicopterTexture(void* minimap) {
            auto* const group = Game::Field<std::byte*>(minimap, kHelicopterGroup);
            if (group == nullptr) return 0;

            for (auto* child = Game::Field<std::byte*>(group, kGroupChildren); child != nullptr;
                 child = Game::Field<std::byte*>(child, kFEObjectNext)) {
                if (Game::Field<int>(child, kFEObjectType) == kFEObjectImage) return Game::Field<std::uint32_t>(child, kFEImageTexture);
            }
            return 0;
        }

        bool IsCopElement(void* minimap, const void* element) {
            for (std::size_t slot = 0; slot < kNumCopElements; ++slot) {
                if (CopElement(minimap, slot) == element) return true;
            }
            return false;
        }

        void* FreeCopElement(void* minimap, std::size_t taken) {
            void* fallback = nullptr;

            for (std::size_t slot = kNumCopElements; slot-- > 0;) {
                void* const element = CopElement(minimap, slot);
                if (element == nullptr) continue;

                const bool borrowed = std::any_of(g_borrowed.begin(), g_borrowed.begin() + taken,
                                                  [element](const BorrowedElement& used) { return used.element == element; });
                if (borrowed) continue;
                if ((Game::Field<std::uint32_t>(element, kFEObjectFlags) & kFEObjectHidden) != 0) return element;
                if (fallback == nullptr) fallback = element;
            }
            return fallback;
        }

        void ReturnBorrowedElements(void* minimap) {
            for (std::size_t index = 0; index < g_numBorrowed; ++index) {
                const BorrowedElement& used = g_borrowed[index];
                if (used.element != nullptr && IsCopElement(minimap, used.element)) {
                    reinterpret_cast<FEngSetTextureHashFn>(g_FEngSetTextureHash)(used.element, used.texture);
                }
            }
            g_numBorrowed = 0;
        }

        void ShowExtraHelicopters(void* minimap) {
            IVehicle** const vehicles = g_vehicles != nullptr ? *g_vehicles : nullptr;
            const int        count    = g_numVehicles != nullptr ? *g_numVehicles : 0;
            if (vehicles == nullptr || count <= 0) return;

            std::array<IVehicle*, kNumCopElements + 1> helicopters{};
            std::size_t                                found = 0;

            for (int index = 0; index < count && found < helicopters.size(); ++index) {
                IVehicle* const vehicle = vehicles[index];
                if (vehicle == nullptr || !vehicle->IsActive()) continue;
                if (vehicle->GetVehicleClass() == *g_CHOPPER) helicopters[found++] = vehicle;
            }
            if (found < 2) return;

            const std::uint32_t texture = HelicopterTexture(minimap);

            for (std::size_t index = 0; index + 1 < found; ++index) {
                ISimable* const simable = helicopters[index]->GetSimable();
                if (simable == nullptr) continue;

                void* const element = FreeCopElement(minimap, g_numBorrowed);
                if (element == nullptr) return;

                g_borrowed[g_numBorrowed++] = { element, Game::Field<std::uint32_t>(element, kFEImageTexture) };
                if (texture != 0) reinterpret_cast<FEngSetTextureHashFn>(g_FEngSetTextureHash)(element, texture);

                std::array<float, 4> position{};
                std::array<float, 4> heading{};
                reinterpret_cast<GetVehicleVectorsFn>(g_GetVehicleVectors)(position.data(), heading.data(), simable);

                reinterpret_cast<FEngSetColorFn>(g_FEngSetColor)(element, kIconColor);
                reinterpret_cast<UpdateElementArtFn>(g_UpdateElementArt)(minimap, position.data(), heading.data(), element, 0);
            }
        }

        void __fastcall UpdateCopElementsHook(void* minimap, void*, void* vehicle) {
            Memory::Guarded([&] { ReturnBorrowedElements(minimap); });
            reinterpret_cast<UpdateCopElementsFn>(g_UpdateCopElements)(minimap, vehicle);
            Memory::Guarded([&] { ShowExtraHelicopters(minimap); });
        }

        bool InstallHelicopterIcons() noexcept {
            const auto list        = Scan::Find(kUpdateCopElementsVehicleList);
            const auto vectors     = Hook::Operand(kGetVehicleVectorsCall, kGetVehicleVectorsOperand, true);
            const auto elementArt  = Hook::Operand(kUpdateElementArtCall, kUpdateElementArtOperand, true);
            const auto setColor    = Hook::Operand(kFEngSetColorCall, kFEngSetColorOperand, true);
            const auto chopper     = Hook::Operand(kVehicleClassCHOPPERRead, kCHOPPEROperand);
            const auto textureHash = Hook::Operand(kFEngSetTextureHashCall, kFEngSetTextureHashOperand, true);
            if (!list || !vectors || !elementArt || !setColor || !chopper || !textureHash) return false;

            const auto vehicles = Scan::Absolute(*list + kVehicleListOperand);
            const auto counted  = Scan::Absolute(*list + kVehicleCountOperand);
            if (!vehicles || !counted || *vehicles == 0 || *counted == 0 || *chopper == 0) return false;

            for (const auto& function : { vectors, elementArt, setColor, textureHash }) {
                if (!Memory::IsExecutable(reinterpret_cast<const void*>(*function))) return false;
            }

            g_vehicles           = reinterpret_cast<IVehicle***>(*vehicles);
            g_numVehicles        = reinterpret_cast<const int*>(*counted);
            g_CHOPPER            = reinterpret_cast<const std::uint32_t*>(*chopper);
            g_GetVehicleVectors  = *vectors;
            g_UpdateElementArt   = *elementArt;
            g_FEngSetColor       = *setColor;
            g_FEngSetTextureHash = *textureHash;

            return Hook::Detour(g_UpdateCopElementsHook, kUpdateCopElements, kUpdateCopElementsStolen,
                                reinterpret_cast<const void*>(&UpdateCopElementsHook), g_UpdateCopElements);
        }

    }

    namespace AIVehicleHelicopter {

        constexpr Memory::Pattern kUpdateFuel =
            Memory::ParsePattern("56 8B F1 D9 86 D8 07 00 00 D8 64 24 08 D9 96 D8 07 00 00");

        constexpr std::array<std::uint8_t, 3> kReturn{ 0xC2, 0x04, 0x00 };

        std::optional<ScopedPatch> g_UpdateFuel;

        bool InstallInfiniteHelicopterFuel() noexcept {
            return Hook::Patch(g_UpdateFuel, kUpdateFuel, 0, kReturn);
        }

    }

    namespace AIVehicleCopCar {

        constexpr Memory::Pattern kCheckForPursuit =
            Memory::ParsePattern("6A FF 68 ?? ?? ?? ?? 64 A1 00 00 00 00 50 64 89 25 00 00 00 00 "
                                 "83 EC 4C 53 8B 5C 24 60 8B 03 56 57 8B F9 8B CB FF 50 04");

        constexpr std::size_t kCheckForPursuitStolen = 7;

        using CheckForPursuitFn = bool(__thiscall*)(void*, void*);

        std::uintptr_t g_CheckForPursuit = 0;
        ScopedHook     g_CheckForPursuitHook;

        bool __fastcall CheckForPursuitHook(void* cop, void*, IVehicle* target) {
            bool player = false;
            Memory::Guarded([&] { player = IVehicle::IsDriver(target, DRIVER_HUMAN); });

            return !player && reinterpret_cast<CheckForPursuitFn>(g_CheckForPursuit)(cop, target);
        }

        bool InstallCopsIgnorePlayer() noexcept {
            return Hook::Detour(g_CheckForPursuitHook, kCheckForPursuit, kCheckForPursuitStolen,
                                reinterpret_cast<const void*>(&CheckForPursuitHook), g_CheckForPursuit);
        }

    }

    namespace AIVehicleHuman {

        constexpr Memory::Pattern kConstructorVTables =
            Memory::ParsePattern("C7 46 54 ?? ?? ?? ?? C7 86 58 07 00 00 ?? ?? ?? ?? "
                                 "C7 86 60 07 00 00 ?? ?? ?? ?? C7 86 6C 07 00 00 ?? ?? ?? ?? "
                                 "C7 86 C4 07 00 00 ?? ?? ?? ?? 8B C6 5E");

        constexpr std::ptrdiff_t kIPerpetratorVTableOperand = 13;
        constexpr std::ptrdiff_t kICauseVTableOperand       = 23;

        constexpr std::ptrdiff_t kGetHeat                                = 0x04;
        constexpr std::ptrdiff_t kSetHeat                                = 0x08;
        constexpr std::ptrdiff_t kAddToPendingRepPointsFromCopDestruction = 0x38;
        constexpr std::ptrdiff_t kAddToPendingRepPointsNormal             = 0x3C;
        constexpr std::ptrdiff_t kOnCausedCollision                       = 0x04;
        constexpr std::ptrdiff_t kHeat                                    = 0x1C;

        using AddToPendingRepPointsFn = void(__thiscall*)(void*, int);
        using SetHeatFn               = void(__thiscall*)(void*, float);
        using OnCausedCollisionFn     = void(__thiscall*)(void*, const void*, ISimable*, ISimable*);

        float          g_repPointsScale                         = 1.0f;
        float          g_lockedHeat                             = 1.0f;
        bool           g_syncingHeat                            = false;
        std::uintptr_t g_AddToPendingRepPointsFromCopDestruction = 0;
        std::uintptr_t g_AddToPendingRepPointsNormal             = 0;
        std::uintptr_t g_GetHeat                                 = 0;
        std::uintptr_t g_SetHeat                                 = 0;
        std::uintptr_t g_OnCausedCollision                       = 0;

        std::optional<ScopedPatch> g_AddToPendingRepPointsFromCopDestructionSlot;
        std::optional<ScopedPatch> g_AddToPendingRepPointsNormalSlot;
        std::optional<ScopedPatch> g_GetHeatSlot;
        std::optional<ScopedPatch> g_SetHeatSlot;
        std::optional<ScopedPatch> g_OnCausedCollisionSlot;

        int ScaleRepPoints(int amount) noexcept {
            const double scaled = std::round(static_cast<double>(amount) * g_repPointsScale);
            return static_cast<int>(std::clamp(scaled, static_cast<double>(INT_MIN), static_cast<double>(INT_MAX)));
        }

        void __fastcall AddToPendingRepPointsFromCopDestructionHook(void* perpetrator, void*, int amount) {
            reinterpret_cast<AddToPendingRepPointsFn>(g_AddToPendingRepPointsFromCopDestruction)(perpetrator, ScaleRepPoints(amount));
        }

        void __fastcall AddToPendingRepPointsNormalHook(void* perpetrator, void*, int amount) {
            reinterpret_cast<AddToPendingRepPointsFn>(g_AddToPendingRepPointsNormal)(perpetrator, ScaleRepPoints(amount));
        }

        float __fastcall GetHeatHook(void* perpetrator, void*) {
            Memory::Guarded([&] {
                if (g_syncingHeat || Game::Field<float>(perpetrator, kHeat) == g_lockedHeat) return;
                g_syncingHeat = true;
                reinterpret_cast<SetHeatFn>(g_SetHeat)(perpetrator, g_lockedHeat);
                g_syncingHeat = false;
            });
            return g_lockedHeat;
        }

        void __fastcall SetHeatHook(void* perpetrator, void*, float) {
            reinterpret_cast<SetHeatFn>(g_SetHeat)(perpetrator, g_lockedHeat);
        }

        void __fastcall OnCausedCollisionHook(void* cause, void*, const void* info, ISimable* from, ISimable* to) {
            reinterpret_cast<OnCausedCollisionFn>(g_OnCausedCollision)(cause, info, from, to);

            Memory::Guarded([&] {
                if (from != nullptr && from->IsPlayer() && IVehicle::IsDriver(to, DRIVER_COP)) IDamageable::Destroy(to);
            });
        }

        bool InstallBountyMultiplier(float scale) noexcept {
            const auto vtable = Hook::Operand(kConstructorVTables, kIPerpetratorVTableOperand);
            if (!vtable) return false;

            g_repPointsScale = scale;
            if (Hook::VTable(g_AddToPendingRepPointsFromCopDestructionSlot, *vtable, kAddToPendingRepPointsFromCopDestruction,
                             reinterpret_cast<const void*>(&AddToPendingRepPointsFromCopDestructionHook),
                             g_AddToPendingRepPointsFromCopDestruction) &&
                Hook::VTable(g_AddToPendingRepPointsNormalSlot, *vtable, kAddToPendingRepPointsNormal,
                             reinterpret_cast<const void*>(&AddToPendingRepPointsNormalHook), g_AddToPendingRepPointsNormal)) {
                return true;
            }

            g_AddToPendingRepPointsNormalSlot.reset();
            g_AddToPendingRepPointsFromCopDestructionSlot.reset();
            return false;
        }

        bool InstallFreezeHeatLevel(float heat) noexcept {
            const auto vtable = Hook::Operand(kConstructorVTables, kIPerpetratorVTableOperand);
            if (!vtable) return false;

            g_lockedHeat = heat;
            if (Hook::VTable(g_SetHeatSlot, *vtable, kSetHeat, reinterpret_cast<const void*>(&SetHeatHook), g_SetHeat) &&
                Hook::VTable(g_GetHeatSlot, *vtable, kGetHeat, reinterpret_cast<const void*>(&GetHeatHook), g_GetHeat)) {
                return true;
            }

            g_GetHeatSlot.reset();
            g_SetHeatSlot.reset();
            return false;
        }

        bool InstallTouchOfDeathCops() noexcept {
            if (!IVehicle::Resolve() || !IDamageable::Resolve()) return false;

            const auto vtable = Hook::Operand(kConstructorVTables, kICauseVTableOperand);
            if (!vtable) return false;

            return Hook::VTable(g_OnCausedCollisionSlot, *vtable, kOnCausedCollision,
                                reinterpret_cast<const void*>(&OnCausedCollisionHook), g_OnCausedCollision);
        }

    }

}

void ApplyPursuit(const IniSection& pursuit, Installer& installer) {
    const AIPursuit::Cheats wanted{
        .bustProof       = pursuit.Bool("BustProof"),
        .instantCooldown = pursuit.Bool("InstantCooldown"),
        .tankMode        = pursuit.Bool("TankMode"),
    };
    if (wanted.bustProof || wanted.instantCooldown || wanted.tankMode) {
        const AIPursuit::Cheats active = AIPursuit::InstallOnTask(wanted);
        if (wanted.bustProof) installer.Check("BustProof", active.bustProof);
        if (wanted.instantCooldown) installer.Check("InstantCooldown", active.instantCooldown);
        if (wanted.tankMode) installer.Check("TankMode", active.tankMode);
    }
    installer.Toggle(pursuit, "AggressiveCopAI", InstallAggressiveCopAI);
    if (const float distance = pursuit.Float("BustDistance", 15.0f, 0.0f, 100.0f); distance != 15.0f) {
        installer.Check("BustDistance", AIPursuit::InstallBustDistance(distance));
    }
    installer.Toggle(pursuit, "DisableSafehouseBust", GManager::InstallDisableSafehouseBust);

    installer.Toggle(pursuit, "SpikeProof", DamageRacer::InstallSpikeProof);
    installer.Toggle(pursuit, "GhostCops", RBVehicle::InstallGhostCops);
    installer.Toggle(pursuit, "DisableHelicopter", AICopManager::InstallDisableHelicopter);
    if (const int helicopters = pursuit.Int("MaxHelicopters", 1, 1, 8);
        helicopters > 1 && !pursuit.Bool("DisableHelicopter")) {
        const bool limited = AICopManager::InstallMaxHelicopters(helicopters);
        installer.Check("MaxHelicopters", limited);
        if (limited) installer.Check("MaxHelicopters icons", Minimap::InstallHelicopterIcons());
    }
    installer.Toggle(pursuit, "InfiniteHelicopterFuel", AIVehicleHelicopter::InstallInfiniteHelicopterFuel);
    installer.Toggle(pursuit, "DisableRoadblocks", AIPursuit::InstallDisableRoadblocks);
    installer.Toggle(pursuit, "DisableReinforcements", AIPursuit::InstallDisableReinforcements);
    installer.Toggle(pursuit, "CopsIgnorePlayer", AIVehicleCopCar::InstallCopsIgnorePlayer);
    if (const float bounty = pursuit.Float("BountyMultiplier", 1.0f, 0.0f, 1000.0f); bounty != 1.0f) {
        installer.Check("BountyMultiplier", AIVehicleHuman::InstallBountyMultiplier(bounty));
    }
    if (pursuit.Bool("FreezeHeatLevel")) {
        const float heat = pursuit.Float("SetHeatLevel", 5.0f, 1.0f, 10.0f);
        installer.Check("FreezeHeatLevel", AIVehicleHuman::InstallFreezeHeatLevel(heat));
    }
    installer.Toggle(pursuit, "TouchOfDeathCops", AIVehicleHuman::InstallTouchOfDeathCops);
    installer.Toggle(pursuit, "PursuitBreakerNuke", AICopManager::InstallPursuitBreakerNuke);
    installer.Toggle(pursuit, "PaperWeightCops", AICopManager::InstallPaperWeightCops);
}
