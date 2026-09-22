#include "AICopManagerApplyBreakerZones.hpp"

#include "../dllmain.hpp"
#include "IDamageableHandle.hpp"
#include "IListFind.hpp"

namespace AICopManagerApplyBreakerZones {

    namespace {

        constexpr Memory::Pattern kApplyBreakerZones =
            Memory::ParsePattern("83 EC 10 57 8B F9 8B 47 7C 85 C0 0F 84 ?? ?? ?? ?? 8B 87 D0 00 00 00 "
                                 "53 55 8B AF C8 00 00 00");

        constexpr Memory::Pattern kUnSpawn =
            Memory::ParsePattern("56 8B F1 8B 46 E8 8B 48 04 57 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 85 C0 74 0A C7 05");

        constexpr Memory::Pattern kHeliExit =
            Memory::ParsePattern("68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 89 44 24 0C 83 C4 04 8D 44 24 08 50 8B CE "
                                 "E8 ?? ?? ?? ?? 5E C2 04 00");

        constexpr std::size_t    kStolenBytes         = 6;
        constexpr std::ptrdiff_t kHelicopterHandle    = 11;
        constexpr std::ptrdiff_t kExitGoalName        = 1;
        constexpr std::ptrdiff_t kStringHashCall      = 6;
        constexpr std::ptrdiff_t kSetGoalCall         = 25;
        constexpr std::ptrdiff_t kBreakerZoneCount    = 0x7C;
        constexpr std::ptrdiff_t kVehicles            = 0xC8;
        constexpr std::ptrdiff_t kVehicleCount        = 0xD0;
        constexpr std::ptrdiff_t kIsUnavailable       = 0x7C;
        constexpr std::ptrdiff_t kIsActive            = 0x88;
        constexpr std::ptrdiff_t kHelicopterInterface = 0x7A4;
        constexpr std::ptrdiff_t kHelicopterFuel      = 0x7D8;

        using ApplyBreakerZonesFn = void(__thiscall*)(void*);
        using StringHashFn        = std::uint32_t(__cdecl*)(const char*);
        using SetGoalFn           = void(__thiscall*)(void*, const std::uint32_t*);

        std::uintptr_t g_original         = 0;
        std::uintptr_t g_helicopterHandle = 0;
        std::uintptr_t g_setGoal          = 0;
        std::uint32_t  g_exitGoal         = 0;
        int            g_knownZones       = 0;
        ScopedHook     g_detour;

        void SendHome(std::byte* helicopter) {
            Game::Field<float>(helicopter, kHelicopterFuel) = 0.0f;
            reinterpret_cast<SetGoalFn>(g_setGoal)(helicopter, &g_exitGoal);
        }

        void Nuke(void* manager) {
            for (int index = 0; index < Game::Field<int>(manager, kVehicleCount); ++index) {
                void** const vehicles = Game::Field<void**>(manager, kVehicles);
                void* const  vehicle  = vehicles != nullptr ? vehicles[index] : nullptr;
                if (vehicle == nullptr || Game::CallVirtual<bool>(vehicle, kIsUnavailable) ||
                    !Game::CallVirtual<bool>(vehicle, kIsActive)) {
                    continue;
                }

                if (void* const helicopter = IListFind::Find(vehicle, g_helicopterHandle)) {
                    SendHome(static_cast<std::byte*>(helicopter) - kHelicopterInterface);
                } else {
                    IDamageableHandle::Destroy(vehicle);
                }
            }
        }

        void __fastcall ApplyBreakerZones(void* manager, void*) {
            Memory::Guarded([&] {
                if (Game::Field<int>(manager, kBreakerZoneCount) > g_knownZones) Nuke(manager);
            });
            reinterpret_cast<ApplyBreakerZonesFn>(g_original)(manager);
            g_knownZones = Game::Field<int>(manager, kBreakerZoneCount);
        }

    }

    bool InstallNuke() noexcept {
        if (!IListFind::Resolve() || !IDamageableHandle::Resolve()) return false;

        const auto handle = Hook::Operand(kUnSpawn, kHelicopterHandle);
        const auto exit   = Scan::Find(kHeliExit);
        if (!handle || *handle == 0 || !exit) return false;

        const auto name       = Scan::Absolute(*exit + kExitGoalName);
        const auto stringHash = Scan::Relative(*exit + kStringHashCall);
        const auto setGoal    = Scan::Relative(*exit + kSetGoalCall);
        if (!name || !stringHash || !setGoal || !Memory::IsExecutable(reinterpret_cast<const void*>(*stringHash)) ||
            !Memory::IsExecutable(reinterpret_cast<const void*>(*setGoal))) {
            return false;
        }

        g_exitGoal         = reinterpret_cast<StringHashFn>(*stringHash)(reinterpret_cast<const char*>(*name));
        g_helicopterHandle = *handle;
        g_setGoal          = *setGoal;
        return Hook::Detour(g_detour, kApplyBreakerZones, kStolenBytes,
                            reinterpret_cast<const void*>(&ApplyBreakerZones), g_original);
    }

}
