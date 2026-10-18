#include "../dllmain.hpp"

#include <algorithm>

namespace {

    namespace EngineRacer {

        constexpr Memory::Pattern kDoNosDriverClassCheck =
            Memory::ParsePattern("A0 ?? ?? ?? ?? 84 C0 D8 74 24 28 D9 5C 24 28 75 0D "
                                 "8B 4E 48 8B 11 FF 52 58 83 F8 06 75 08");

        constexpr Memory::Pattern kGetEngineTorque =
            Memory::ParsePattern("8B 44 24 04 56 8B F1 50 8D 8E 44 01 00 00 51 E8 ?? ?? ?? ?? "
                                 "8B 4E 48 8B 11 D8 0D ?? ?? ?? ?? D9 86 98 00 00 00");

        constexpr std::ptrdiff_t              kDoNosDriverClass       = 0x1B;
        constexpr std::array<std::uint8_t, 1> kDriverHuman{ DRIVER_HUMAN };
        constexpr std::size_t                 kGetEngineTorqueStolen  = 5;
        constexpr std::ptrdiff_t              kVehicle                = 0x48;

        using GetEngineTorqueFn = float(__thiscall*)(void*, float);

        std::optional<ScopedPatch> g_DoNos;
        std::uintptr_t             g_GetEngineTorque = 0;
        float                      g_torqueScale     = 1.0f;
        ScopedHook                 g_GetEngineTorqueHook;

        float __fastcall GetEngineTorqueHook(void* engine, void*, float rpm) {
            const float torque = reinterpret_cast<GetEngineTorqueFn>(g_GetEngineTorque)(engine, rpm);

            bool player = false;
            Memory::Guarded([&] { player = IVehicle::IsDriver(Game::Field<IVehicle*>(engine, kVehicle), DRIVER_HUMAN); });
            return player ? torque * g_torqueScale : torque;
        }

        bool InstallInfiniteNitro() noexcept {
            return Hook::Patch(g_DoNos, kDoNosDriverClassCheck, kDoNosDriverClass, kDriverHuman);
        }

        bool InstallEnginePower(float scale) noexcept {
            g_torqueScale = scale;
            return Hook::Detour(g_GetEngineTorqueHook, kGetEngineTorque, kGetEngineTorqueStolen,
                                reinterpret_cast<const void*>(&GetEngineTorqueHook), g_GetEngineTorque);
        }

    }

    namespace LocalPlayer {

        constexpr Memory::Pattern kDoGameBreakerDrain =
            Memory::ParsePattern("8A 86 80 00 00 00 84 C0 D8 0D ?? ?? ?? ?? 74 ?? A0 ?? ?? ?? ?? DD D8 84 C0 75 ??");

        constexpr std::ptrdiff_t              kTweak_InfiniteRaceBreakerRead = 17;
        constexpr std::array<std::uint8_t, 1> kOn{ 0x01 };

        std::optional<ScopedPatch> g_Tweak_InfiniteRaceBreaker;

        bool InstallInfiniteSpeedbreaker() noexcept {
            if (g_Tweak_InfiniteRaceBreaker) return true;

            const auto tweak = Hook::Operand(kDoGameBreakerDrain, kTweak_InfiniteRaceBreakerRead);
            if (!tweak) return false;

            const auto current = Memory::Read<std::uint8_t>(*tweak);
            if (!current || *current > 1) return false;

            g_Tweak_InfiniteRaceBreaker.emplace(*tweak, kOn);
            if (*g_Tweak_InfiniteRaceBreaker) return true;

            g_Tweak_InfiniteRaceBreaker.reset();
            return false;
        }

    }

    namespace SuspensionRacer {

        constexpr Memory::Pattern kDoWheelForces =
            Memory::ParsePattern("81 EC A4 01 00 00 53 56 8B B4 24 B0 01 00 00 8B 86 C0 00 00 00 "
                                 "57 8B D9 8D 4C 24 78 51 33 FF");

        constexpr Memory::Pattern kTireUpdateLoaded =
            Memory::ParsePattern("83 EC 24 56 8B F1 8B 86 0C 01 00 00 8B 8E 04 01 00 00 8B 49 08 "
                                 "D9 44 81 08 8B 96 00 01 00 00");

        constexpr std::size_t    kDoWheelForcesStolen = 6;
        constexpr std::size_t    kUpdateLoadedStolen  = 6;
        constexpr std::ptrdiff_t kOwner               = 0x34;
        constexpr std::ptrdiff_t kTireBrake           = 0xC8;
        constexpr std::ptrdiff_t kTireTractionBoost   = 0xEC;
        constexpr std::ptrdiff_t kTireGripBoost       = 0x128;
        constexpr std::ptrdiff_t kTireDriftFriction   = 0x12C;
        constexpr float          kFullGrip            = 1000.0f;
        constexpr float          kGripBoostFloor      = 3.0f;

        struct TireCheats {
            bool  infiniteGrip = false;
            float brakeScale   = 1.0f;
            float gripScale    = 1.0f;
        };

        using DoWheelForcesFn = void(__thiscall*)(void*, void*);
        using UpdateLoadedFn  = float(__thiscall*)(void*, float, float, float, float, float);

        std::uintptr_t g_DoWheelForces = 0;
        std::uintptr_t g_UpdateLoaded  = 0;
        bool           g_playerWheels  = false;
        TireCheats     g_tireCheats{};
        ScopedHook     g_DoWheelForcesHook;
        ScopedHook     g_UpdateLoadedHook;

        void __fastcall DoWheelForcesHook(void* suspension, void*, void* state) {
            bool player = false;
            Memory::Guarded([&] {
                const auto* const owner = Game::Field<ISimable*>(suspension, kOwner);
                player = owner != nullptr && owner->IsPlayer();
            });

            const bool outer = g_playerWheels;
            g_playerWheels = player;
            reinterpret_cast<DoWheelForcesFn>(g_DoWheelForces)(suspension, state);
            g_playerWheels = outer;
        }

        float __fastcall TireUpdateLoadedHook(void* tire, void*, float lateralVelocity, float forwardVelocity,
                                              float carSpeed, float load, float deltaTime) {
            const auto original = reinterpret_cast<UpdateLoadedFn>(g_UpdateLoaded);
            if (!g_playerWheels) return original(tire, lateralVelocity, forwardVelocity, carSpeed, load, deltaTime);

            float& brake         = Game::Field<float>(tire, kTireBrake);
            float& tractionBoost = Game::Field<float>(tire, kTireTractionBoost);
            float& gripBoost     = Game::Field<float>(tire, kTireGripBoost);
            float& driftFriction = Game::Field<float>(tire, kTireDriftFriction);
            const float savedBrake         = brake;
            const float savedTractionBoost = tractionBoost;
            const float savedGripBoost     = gripBoost;
            const float savedDriftFriction = driftFriction;

            brake         = savedBrake * g_tireCheats.brakeScale;
            tractionBoost = savedTractionBoost * g_tireCheats.gripScale;
            if (g_tireCheats.infiniteGrip) {
                gripBoost     = std::max(savedGripBoost, kGripBoostFloor);
                driftFriction = kFullGrip;
            }
            const float result = original(tire, lateralVelocity, forwardVelocity, carSpeed, load, deltaTime);
            brake         = savedBrake;
            tractionBoost = savedTractionBoost;
            gripBoost     = savedGripBoost;
            driftFriction = savedDriftFriction;
            return result;
        }

        bool InstallTireCheats(const TireCheats& cheats) noexcept {
            g_tireCheats = cheats;
            if (!Hook::Detour(g_DoWheelForcesHook, kDoWheelForces, kDoWheelForcesStolen,
                              reinterpret_cast<const void*>(&DoWheelForcesHook), g_DoWheelForces)) {
                return false;
            }
            if (Hook::Detour(g_UpdateLoadedHook, kTireUpdateLoaded, kUpdateLoadedStolen,
                             reinterpret_cast<const void*>(&TireUpdateLoadedHook), g_UpdateLoaded)) {
                return true;
            }

            g_DoWheelForcesHook.Reset();
            return false;
        }

    }

    namespace Chassis {

        constexpr Memory::Pattern kDoAerodynamics =
            Memory::ParsePattern("83 EC 18 53 55 56 57 8B F9 8B 4F 34 8B 01 FF 50 54 D9 44 24 30");

        constexpr std::size_t    kDoAerodynamicsStolen = 5;
        constexpr std::ptrdiff_t kOwner                = 0x34;
        constexpr std::ptrdiff_t kStateGasInput        = 0x4C;
        constexpr std::ptrdiff_t kStateGroundEffect    = 0x6C;
        constexpr float          kAllWheelsOnGround    = 1.0f;
        constexpr float          kDownforceScale       = 1.5f;

        using DoAerodynamicsFn = void(__thiscall*)(void*, void*, float, float, float, float, const void*);

        std::uintptr_t g_DoAerodynamics = 0;
        ScopedHook     g_DoAerodynamicsHook;

        void __fastcall DoAerodynamicsHook(void* chassis, void*, void* state, float dragPct, float aeroPct,
                                           float aeroFrontZ, float aeroRearZ, const void* tunings) {
            bool  player       = false;
            float gasInput     = 0.0f;
            float groundEffect = 0.0f;
            Memory::Guarded([&] {
                const auto* const owner = Game::Field<ISimable*>(chassis, kOwner);
                player       = owner != nullptr && owner->IsPlayer();
                gasInput     = Game::Field<float>(state, kStateGasInput);
                groundEffect = Game::Field<float>(state, kStateGroundEffect);
            });
            if (player) {
                dragPct *= 1.0f - std::clamp(gasInput, 0.0f, 1.0f);
                if (groundEffect >= kAllWheelsOnGround) aeroPct *= kDownforceScale;
            }
            reinterpret_cast<DoAerodynamicsFn>(g_DoAerodynamics)(chassis, state, dragPct, aeroPct, aeroFrontZ, aeroRearZ,
                                                                 tunings);
        }

        bool InstallPerfectAerodynamics() noexcept {
            return Hook::Detour(g_DoAerodynamicsHook, kDoAerodynamics, kDoAerodynamicsStolen,
                                reinterpret_cast<const void*>(&DoAerodynamicsHook), g_DoAerodynamics);
        }

    }

    namespace RBVehicle {

        constexpr Memory::Pattern kModifyCollision =
            Memory::ParsePattern("83 EC 24 8B 44 24 2C 53 8B 5C 24 2C 55 56 8B 74 24 3C 57 56");

        constexpr std::size_t    kModifyCollisionStolen    = 7;
        constexpr std::ptrdiff_t kOwner                    = 0x34;
        constexpr std::ptrdiff_t kData                     = 0x78;
        constexpr std::ptrdiff_t kSpecs                    = 0x84;
        constexpr std::ptrdiff_t kSimableType              = 0x108;
        constexpr std::ptrdiff_t kDetachForce              = 0x10C;
        constexpr std::ptrdiff_t kImmobileObjectCollisions = 0xFD;
        constexpr std::ptrdiff_t kVolatileStatus           = 0x1C;
        constexpr std::uint8_t   kIsAttachedToWorld        = 0x02;
        constexpr int            kSimableSmackable         = 2;
        constexpr std::ptrdiff_t kMomentImmobile           = 0xC8;
        constexpr std::ptrdiff_t kMomentBreakingForce      = 0xCC;

        using ModifyCollisionFn = void(__thiscall*)(void*, void*, const void*, void*);

        std::uintptr_t g_ModifyCollision = 0;
        ScopedHook     g_ModifyCollisionHook;

        bool BreaksLoose(void* other) {
            if (Game::Field<int>(other, kSimableType) != kSimableSmackable) return false;
            if (Game::Field<std::uint8_t>(Game::Field<void*>(other, kSpecs), kImmobileObjectCollisions) != 0) return false;

            void* const data = *Game::Field<void**>(other, kData);
            if ((Game::Field<std::uint8_t>(data, kVolatileStatus) & kIsAttachedToWorld) == 0) return true;
            return Game::Field<float>(other, kDetachForce) > 0.0f;
        }

        void __fastcall ModifyCollisionHook(void* body, void*, void* other, const void* plane, void* moment) {
            reinterpret_cast<ModifyCollisionFn>(g_ModifyCollision)(body, other, plane, moment);
            Memory::Guarded([&] {
                const auto* const owner = Game::Field<ISimable*>(body, kOwner);
                if (owner == nullptr || !owner->IsPlayer() || !BreaksLoose(other)) return;

                Game::Field<bool>(moment, kMomentImmobile)       = true;
                Game::Field<float>(moment, kMomentBreakingForce) = 0.0f;
            });
        }

        bool InstallSmackablesNoMass() noexcept {
            return Hook::Detour(g_ModifyCollisionHook, kModifyCollision, kModifyCollisionStolen,
                                reinterpret_cast<const void*>(&ModifyCollisionHook), g_ModifyCollision);
        }

    }

    namespace EngineDragster {

        constexpr Memory::Pattern kCalcPotentialShiftBonus =
            Memory::ParsePattern("56 8B F1 8B 4E 34 8B 01 FF 50 24 84 C0 75 0A D9 05 ?? ?? ?? ??");

        constexpr std::size_t kCalcPotentialShiftBonusStolen = 6;
        constexpr int         kFirstGear              = 2;
        constexpr float       kPerfectShiftBonus           = 1.0f;

        using CalcPotentialShiftBonusFn = float(__thiscall*)(void*, float, int, int);

        std::uintptr_t g_CalcPotentialShiftBonus = 0;
        ScopedHook     g_CalcPotentialShiftBonusHook;

        float __fastcall CalcPotentialShiftBonusHook(void* engine, void*, float rpm, int gear, int nextGear) {
            const float bonus = reinterpret_cast<CalcPotentialShiftBonusFn>(g_CalcPotentialShiftBonus)(engine, rpm, gear, nextGear);
            return gear >= kFirstGear && nextGear > gear ? kPerfectShiftBonus : bonus;
        }

        bool InstallPerfectShift() noexcept {
            return Hook::Detour(g_CalcPotentialShiftBonusHook, kCalcPotentialShiftBonus, kCalcPotentialShiftBonusStolen,
                                reinterpret_cast<const void*>(&CalcPotentialShiftBonusHook), g_CalcPotentialShiftBonus);
        }

    }

    namespace GRacerInfo {

        constexpr Memory::Pattern kForceStop =
            Memory::ParsePattern("8B 01 85 C0 74 3E 8B 0D ?? ?? ?? ?? 50 A1 ?? ?? ?? ?? 8D 14 C8 52 50 E8 ?? ?? ?? ?? "
                                 "83 C4 0C 85 C0 74 21 8B 40 04 85 C0 74 1A 8B 48 04 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? "
                                 "85 C0 74 09 8B 10 6A 01");

        constexpr std::ptrdiff_t kInstanceCountRead = 8;
        constexpr std::ptrdiff_t kInstanceTableRead = 14;
        constexpr std::ptrdiff_t kSearchCall        = 24;
        constexpr std::ptrdiff_t kKeyedNodeSimable  = 0x04;
        constexpr std::size_t    kKeyedNodeSize     = 8;
        constexpr std::ptrdiff_t kSimable           = 0x00;
        constexpr char           kForceStopRace     = 0x01;

        using KeyedNodeSearchFn = void*(__cdecl*)(void*, void*, std::uint32_t);

        std::uintptr_t g_instanceCount   = 0;
        std::uintptr_t g_instanceTable   = 0;
        std::uintptr_t g_KeyedNodeSearch = 0;

        bool Resolve() noexcept {
            if (g_KeyedNodeSearch != 0) return true;

            const auto count  = Hook::Operand(kForceStop, kInstanceCountRead);
            const auto table  = Hook::Operand(kForceStop, kInstanceTableRead);
            const auto search = Hook::Operand(kForceStop, kSearchCall, true);
            if (!count || !table || !search || !Memory::IsExecutable(reinterpret_cast<const void*>(*search))) return false;

            g_instanceCount   = *count;
            g_instanceTable   = *table;
            g_KeyedNodeSearch = *search;
            return true;
        }

        ISimable* GetSimable(std::byte* racer) {
            const auto handle = Game::Field<std::uint32_t>(racer, kSimable);
            if (handle == 0 || g_KeyedNodeSearch == 0) return nullptr;

            const auto table = Memory::Read<std::uintptr_t>(g_instanceTable);
            const auto count = Memory::Read<std::uint32_t>(g_instanceCount);
            if (!table || !count || *table == 0) return nullptr;

            auto* const begin = reinterpret_cast<std::byte*>(*table);
            auto* const end   = begin + static_cast<std::size_t>(*count) * kKeyedNodeSize;
            void* const node  = reinterpret_cast<KeyedNodeSearchFn>(g_KeyedNodeSearch)(begin, end, handle);
            return node != nullptr ? Game::Field<ISimable*>(node, kKeyedNodeSimable) : nullptr;
        }

        void ForceStop(std::byte* racer) {
            ISimable* const simable = GetSimable(racer);
            IVehicle* const vehicle = simable != nullptr ? simable->QueryInterface<IVehicle>() : nullptr;
            if (!IVehicle::IsDriver(vehicle, DRIVER_RACER)) return;
            if ((vehicle->GetForceStop() & kForceStopRace) == 0) vehicle->ForceStopOn(kForceStopRace);
        }

    }

    namespace GRaceStatus {

        constexpr Memory::Pattern kComputeCatchUpSkill =
            Memory::ParsePattern("83 EC 20 8A 44 24 34 53 32 DB 84 C0 56 8B F1 C6 44 24 0B 00 74 0D "
                                 "C7 44 24 3C 00 00 80 3F E9");

        constexpr Memory::Pattern kUpdate =
            Memory::ParsePattern("83 EC 28 53 55 56 8B F1 8B 86 60 19 00 00 83 F8 01 57 75 17 "
                                 "8A 8E A5 19 00 00 84 C9 74 0D");

        constexpr std::array<std::uint8_t, 5> kReturnFalse{ 0x32, 0xC0, 0xC2, 0x14, 0x00 };
        constexpr std::size_t                 kUpdateStolen   = 6;
        constexpr std::ptrdiff_t              kRacerInfo      = 0x18;
        constexpr std::ptrdiff_t              kRacerInfoSize  = 0x194;
        constexpr std::ptrdiff_t              kRacerCount     = 0x1958;
        constexpr int                         kMaxRacers      = 16;

        using UpdateFn = void(__thiscall*)(void*, float);

        std::optional<ScopedPatch> g_ComputeCatchUpSkill;
        std::uintptr_t             g_Update = 0;
        ScopedHook                 g_UpdateHook;

        void ForceStopOpponents(std::byte* status) {
            const int count = std::clamp(Game::Field<int>(status, kRacerCount), 0, kMaxRacers);
            for (int index = 0; index < count; ++index) {
                GRacerInfo::ForceStop(status + kRacerInfo + index * kRacerInfoSize);
            }
        }

        void __fastcall UpdateHook(void* status, void*, float deltaTime) {
            reinterpret_cast<UpdateFn>(g_Update)(status, deltaTime);
            Memory::Guarded([&] { ForceStopOpponents(static_cast<std::byte*>(status)); });
        }

        bool InstallDisableRubberbanding() noexcept {
            return Hook::Patch(g_ComputeCatchUpSkill, kComputeCatchUpSkill, 0, kReturnFalse);
        }

        bool InstallFreezeAI() noexcept {
            if (!GRacerInfo::Resolve() || !IVehicle::Resolve()) return false;
            return Hook::Detour(g_UpdateHook, kUpdate, kUpdateStolen, reinterpret_cast<const void*>(&UpdateHook), g_Update);
        }

    }

    namespace AITrafficManager {

        constexpr Memory::Pattern kComputeDensity =
            Memory::ParsePattern("A1 ?? ?? ?? ?? 85 C0 75 13 8B 0D ?? ?? ?? ?? 85 C9 74 10 8B 01 FF 50 14 84 C0 74 07 "
                                 "D9 05 ?? ?? ?? ?? C3");

        constexpr std::array<std::uint8_t, 3> kReturnZero{ 0xD9, 0xEE, 0xC3 };

        std::optional<ScopedPatch> g_ComputeDensity;

        bool InstallDisableTraffic() noexcept {
            return Hook::Patch(g_ComputeDensity, kComputeDensity, 0, kReturnZero);
        }

    }

    namespace AIActionTraffic {

        constexpr Memory::Pattern kUpdate =
            Memory::ParsePattern("83 EC 2C 53 55 56 8B F1 8B 4E 40 8B 01 57 FF 50 48 8B 4E 30");

        constexpr Memory::Pattern kUpdateCopElementsVehicleList =
            Memory::ParsePattern("8B 35 ?? ?? ?? ?? A1 ?? ?? ?? ?? 8D 0C 86 3B F1 0F 84 ?? ?? ?? ?? "
                                 "8D 93 D0 00 00 00 89 54 24 14");

        constexpr std::size_t    kUpdateStolen          = 6;
        constexpr std::ptrdiff_t kVehicleListOperand    = 2;
        constexpr std::ptrdiff_t kOwner                 = 0x30;
        constexpr std::ptrdiff_t kAI                    = 0x40;
        constexpr std::ptrdiff_t kAccident              = 0xAC;
        constexpr std::ptrdiff_t kIVehicleAIDoDriving   = 0x2C;
        constexpr std::ptrdiff_t kIVehicleAISetSpeed    = 0x38;
        constexpr std::ptrdiff_t kIVehicleAISetTarget   = 0x3C;
        constexpr std::size_t    kVehicleListSize       = 0x60;
        constexpr std::ptrdiff_t kVehicleListBegin      = 0x04;
        constexpr std::ptrdiff_t kVehicleListCount      = 0x0C;
        constexpr int            kVEHICLE_AICOPS        = 4;
        constexpr int            kVEHICLE_RACERS        = 6;
        constexpr int            kDriveFlags            = 3;
        constexpr float          kTargetRange           = 60.0f;
        constexpr float          kMinTrafficSpeed       = 2.0f;
        constexpr float          kLeadTime              = 0.3f;
        constexpr float          kRamSpeed              = 30.0f;

        using UpdateFn = void(__thiscall*)(void*, float);

        std::uintptr_t g_Update       = 0;
        std::uintptr_t g_vehicleLists = 0;
        ScopedHook     g_UpdateHook;

        float DotXZ(const UMath::Vector3& a, const UMath::Vector3& b) {
            return a.x * b.x + a.z * b.z;
        }

        UMath::Vector3 Minus(const UMath::Vector3& a, const UMath::Vector3& b) {
            return { a.x - b.x, a.y - b.y, a.z - b.z };
        }

        bool FindOncomingTarget(const IRigidBody& body, UMath::Vector3& aim) {
            const UMath::Vector3& position = body.GetPosition();
            const UMath::Vector3& velocity = body.GetLinearVelocity();
            float best = kTargetRange * kTargetRange;
            bool  found = false;

            for (const int list : { kVEHICLE_RACERS, kVEHICLE_AICOPS }) {
                const std::uintptr_t header = g_vehicleLists + static_cast<std::uintptr_t>(list) * kVehicleListSize;
                auto* const* const vehicles = *reinterpret_cast<IVehicle* const* const*>(header + kVehicleListBegin);
                const auto count = *reinterpret_cast<const std::uint32_t*>(header + kVehicleListCount);
                if (vehicles == nullptr) continue;

                for (std::uint32_t index = 0; index < count; ++index) {
                    IVehicle* const vehicle = vehicles[index];
                    if (vehicle == nullptr || vehicle->IsDestroyed() || !vehicle->IsActive()) continue;
                    ISimable* const simable = vehicle->GetSimable();
                    const IRigidBody* const other = simable != nullptr ? simable->GetRigidBody() : nullptr;
                    if (other == nullptr || other == &body) continue;

                    const UMath::Vector3 offset  = Minus(other->GetPosition(), position);
                    const UMath::Vector3 closing = Minus(other->GetLinearVelocity(), velocity);
                    const float distance = DotXZ(offset, offset);
                    if (distance >= best || DotXZ(offset, velocity) <= 0.0f || DotXZ(offset, closing) >= 0.0f) continue;

                    const UMath::Vector3& otherVelocity = other->GetLinearVelocity();
                    aim   = { other->GetPosition().x + otherVelocity.x * kLeadTime, other->GetPosition().y,
                              other->GetPosition().z + otherVelocity.z * kLeadTime };
                    best  = distance;
                    found = true;
                }
            }
            return found;
        }

        void SteerIntoOncoming(std::byte* action) {
            if (Game::Field<int>(action, kAccident) != 0) return;

            const ISimable* const owner = Game::Field<ISimable*>(action, kOwner);
            void* const           ai    = Game::Field<void*>(action, kAI);
            const IRigidBody* const body = owner != nullptr ? owner->GetRigidBody() : nullptr;
            if (ai == nullptr || body == nullptr || body->GetSpeed() < kMinTrafficSpeed) return;

            UMath::Vector3 aim{};
            if (!FindOncomingTarget(*body, aim)) return;

            Game::CallVirtual<void>(ai, kIVehicleAISetTarget, &aim);
            Game::CallVirtual<void>(ai, kIVehicleAISetSpeed, kRamSpeed);
            Game::CallVirtual<void>(ai, kIVehicleAIDoDriving, kDriveFlags);
        }

        void __fastcall UpdateHook(void* action, void*, float deltaTime) {
            reinterpret_cast<UpdateFn>(g_Update)(action, deltaTime);
            Memory::Guarded([&] { SteerIntoOncoming(static_cast<std::byte*>(action)); });
        }

        bool InstallAggressiveTraffic() noexcept {
            const auto list = Scan::Find(kUpdateCopElementsVehicleList);
            if (!list) return false;

            const auto copsBegin = Scan::Absolute(*list + kVehicleListOperand);
            if (!copsBegin || *copsBegin == 0) return false;

            g_vehicleLists = *copsBegin - kVehicleListBegin - kVEHICLE_AICOPS * kVehicleListSize;
            return Hook::Detour(g_UpdateHook, kUpdate, kUpdateStolen, reinterpret_cast<const void*>(&UpdateHook), g_Update);
        }

    }

}

void ApplyMain(const IniSection& main, Installer& installer) {
    installer.Toggle(main, "InfiniteNitro", EngineRacer::InstallInfiniteNitro);
    installer.Toggle(main, "InfiniteSpeedbreaker", LocalPlayer::InstallInfiniteSpeedbreaker);
    if (const float power = main.Float("EnginePowerMultiplier", 1.0f, 0.1f, 10.0f); power != 1.0f) {
        installer.Check("EnginePowerMultiplier", EngineRacer::InstallEnginePower(power));
    }
    const SuspensionRacer::TireCheats tires{
        .infiniteGrip = main.Bool("InfiniteGrip"),
        .brakeScale   = main.Float("BrakePower", 1.0f, 0.1f, 10.0f),
        .gripScale    = main.Float("GripMultiplier", 1.0f, 0.1f, 10.0f),
    };
    if (tires.infiniteGrip || tires.brakeScale != 1.0f || tires.gripScale != 1.0f) {
        const bool installed = SuspensionRacer::InstallTireCheats(tires);
        if (tires.infiniteGrip) installer.Check("InfiniteGrip", installed);
        if (tires.brakeScale != 1.0f) installer.Check("BrakePower", installed);
        if (tires.gripScale != 1.0f) installer.Check("GripMultiplier", installed);
    }
    installer.Toggle(main, "PerfectShift", EngineDragster::InstallPerfectShift);
    installer.Toggle(main, "PerfectAerodynamics", Chassis::InstallPerfectAerodynamics);
    installer.Toggle(main, "SmackablesNoMass", RBVehicle::InstallSmackablesNoMass);
    installer.Toggle(main, "DisableRubberbanding", GRaceStatus::InstallDisableRubberbanding);
    installer.Toggle(main, "FreezeAI", GRaceStatus::InstallFreezeAI);
    installer.Toggle(main, "DisableTraffic", AITrafficManager::InstallDisableTraffic);
    installer.Toggle(main, "AggressiveTraffic", AIActionTraffic::InstallAggressiveTraffic);
}
