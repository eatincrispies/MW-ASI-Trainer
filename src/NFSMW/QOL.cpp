#include "../dllmain.hpp"

#include <cmath>

namespace {

    namespace MoviePlayer {

        constexpr Memory::Pattern kPlaySkipMoviesRead =
            Memory::ParsePattern("51 A1 ?? ?? ?? ?? 85 C0 56 57 8B F1 74 49 8B 3D ?? ?? ?? ?? 8B 07 05 D8 00 00 00");

        constexpr std::ptrdiff_t              kSkipMoviesOperand = 2;
        constexpr std::array<std::uint8_t, 4> kOn{ 0x01, 0x00, 0x00, 0x00 };

        std::optional<ScopedPatch> g_SkipMovies;

        bool InstallSkipMovies() noexcept {
            if (g_SkipMovies) return true;

            const auto flag = Hook::Operand(kPlaySkipMoviesRead, kSkipMoviesOperand);
            if (!flag || *flag == 0) return false;

            g_SkipMovies.emplace(*flag, kOn);
            if (*g_SkipMovies) return true;

            g_SkipMovies.reset();
            return false;
        }

    }

    namespace WorldMap {

        constexpr Memory::Pattern kNotificationMessageGPS_Engage =
            Memory::ParsePattern("8B C1 D9 40 18 83 C0 10 D9 00 6A 00 D9 40 04 8D 44 24 50 D9 E0");

        constexpr Memory::Pattern kGame_NotifyRaceFinished =
            Memory::ParsePattern("64 A1 00 00 00 00 6A FF 68 ?? ?? ?? ?? 50 64 89 25 00 00 00 00 83 EC 0C 53 55 56 57 "
                                 "8B 7C 24 2C 85 FF 75 1F A1");

        constexpr std::ptrdiff_t kGPS_EngageCall       = 0x24;
        constexpr std::ptrdiff_t kLocalPlayerCountRead = 0x25;
        constexpr std::ptrdiff_t kLocalPlayerListRead  = 0x2E;
        constexpr std::ptrdiff_t kIPlayerGetSimable    = 0x04;
        constexpr int            kTeleportFrames       = 600;
        constexpr float          kHoldHeight           = 3.0f;

        using GPS_EngageFn = bool(__cdecl*)(const UMath::Vector3*, float);

        std::uintptr_t             g_GPS_Engage      = 0;
        std::uintptr_t             g_localCount      = 0;
        std::uintptr_t             g_localList       = 0;
        UMath::Vector3             g_teleportTarget{};
        int                        g_teleportFrames  = 0;
        std::optional<ScopedPatch> g_GPS_EngageCall;

        ISimable* LocalPlayerSimable() {
            if (*reinterpret_cast<const std::uint32_t*>(g_localCount) == 0) return nullptr;
            void* const* const players = *reinterpret_cast<void* const* const*>(g_localList);
            void* const        player  = players != nullptr ? players[0] : nullptr;
            return player != nullptr ? Game::CallVirtual<ISimable*>(player, kIPlayerGetSimable) : nullptr;
        }

        void TeleportToPin() {
            if (g_teleportFrames <= 0) return;
            --g_teleportFrames;

            ISimable* const   simable = LocalPlayerSimable();
            IVehicle* const   vehicle = simable != nullptr ? simable->QueryInterface<IVehicle>() : nullptr;
            IRigidBody* const body    = simable != nullptr ? simable->GetRigidBody() : nullptr;
            if (vehicle == nullptr || body == nullptr) return;

            const UMath::Vector3& position = body->GetPosition();
            UMath::Vector3        forward{ g_teleportTarget.x - position.x, 0.0f, g_teleportTarget.z - position.z };
            const float           length = std::sqrt(forward.x * forward.x + forward.z * forward.z);
            forward = length > 0.01f ? UMath::Vector3{ forward.x / length, 0.0f, forward.z / length } : UMath::Vector3{ 0.0f, 0.0f, 1.0f };

            if (vehicle->SetVehicleOnGround(g_teleportTarget, forward)) {
                g_teleportFrames = 0;
                return;
            }

            body->SetPosition({ g_teleportTarget.x, g_teleportTarget.y + kHoldHeight, g_teleportTarget.z });
            body->SetLinearVelocity({ 0.0f, 0.0f, 0.0f });
        }

        bool __cdecl GPS_EngageHook(const UMath::Vector3* target, float maxDeviation) {
            const bool engaged = reinterpret_cast<GPS_EngageFn>(g_GPS_Engage)(target, maxDeviation);
            if (target != nullptr) {
                g_teleportTarget = *target;
                g_teleportFrames = kTeleportFrames;
            }
            return engaged;
        }

        bool InstallTeleportToMarker() noexcept {
            if (!IVehicle::Resolve()) return false;

            const auto map      = Scan::Find(kNotificationMessageGPS_Engage);
            const auto finished = Scan::Find(kGame_NotifyRaceFinished);
            if (!map || !finished) return false;

            const std::uintptr_t site  = *map + kGPS_EngageCall;
            const auto           count = Scan::Absolute(*finished + kLocalPlayerCountRead);
            const auto           list  = Scan::Absolute(*finished + kLocalPlayerListRead);
            if (!count || !list || *count == 0 || *list == 0) return false;

            g_localCount = *count;
            g_localList  = *list;
            if (!AddFrameCallback(&TeleportToPin)) return false;

            return Hook::RedirectCall(g_GPS_EngageCall, site, reinterpret_cast<const void*>(&GPS_EngageHook), g_GPS_Engage);
        }

    }

    namespace Rumble {

        constexpr Memory::Pattern kApplyCameraShake =
            Memory::ParsePattern("55 8B EC 83 E4 F0 83 EC 5C 56 E8 ?? ?? ?? ?? D8 1D ?? ?? ?? ??");

        constexpr std::array<std::uint8_t, 1> kReturn{ 0xC3 };

        std::optional<ScopedPatch> g_ApplyCameraShake;

        bool InstallDisableCameraShake() noexcept {
            return Hook::Patch(g_ApplyCameraShake, kApplyCameraShake, 0, kReturn);
        }

    }

    namespace FEngHud {

        constexpr Memory::Pattern kShouldRearViewMirrorBeVisible =
            Memory::ParsePattern("A1 ?? ?? ?? ?? 53 8B 5C 24 08 56 8B F3 6B F6 70 81 C6 ?? ?? ?? ??");

        constexpr std::array<std::uint8_t, 3> kReturnFalse{ 0x32, 0xC0, 0xC3 };

        std::optional<ScopedPatch> g_ShouldRearViewMirrorBeVisible;

        bool InstallDisableRearviewMirror() noexcept {
            return Hook::Patch(g_ShouldRearViewMirrorBeVisible, kShouldRearViewMirrorBeVisible, 0, kReturnFalse);
        }

    }

    namespace WindowProc {

        constexpr Memory::Pattern kActivateLostFocus =
            Memory::ParsePattern("A1 ?? ?? ?? ?? 0F 94 C3 85 C0 0F 84 45 02 00 00 3A 1D ?? ?? ?? ??");

        constexpr std::ptrdiff_t              kFocusBranch = 10;
        constexpr std::array<std::uint8_t, 2> kAlwaysSkip{ 0x90, 0xE9 };

        std::optional<ScopedPatch> g_FocusBranch;

        bool InstallUnmuteInBackground() noexcept {
            return Hook::Patch(g_FocusBranch, kActivateLostFocus, kFocusBranch, kAlwaysSkip);
        }

    }

    namespace Speech {

        constexpr Memory::Pattern kIsSpeechEnabledRead =
            Memory::ParsePattern("A1 ?? ?? ?? ?? 85 C0 74 15 D9 44 24 2C D8 1D ?? ?? ?? ?? DF E0");

        constexpr std::ptrdiff_t              kIsSpeechEnabledOperand = 1;
        constexpr std::array<std::uint8_t, 4> kOff{ 0x00, 0x00, 0x00, 0x00 };

        std::optional<ScopedPatch> g_IsSpeechEnabled;

        bool InstallDisableCopRadio() noexcept {
            if (g_IsSpeechEnabled) return true;

            const auto flag = Hook::Operand(kIsSpeechEnabledRead, kIsSpeechEnabledOperand);
            if (!flag || *flag == 0) return false;

            g_IsSpeechEnabled.emplace(*flag, kOff);
            if (*g_IsSpeechEnabled) return true;

            g_IsSpeechEnabled.reset();
            return false;
        }

    }

}

void ApplyQOL(const IniSection& qol, Installer& installer) {
    installer.Toggle(qol, "SkipMovies", MoviePlayer::InstallSkipMovies);
    installer.Toggle(qol, "TeleportToMarker", WorldMap::InstallTeleportToMarker);
    installer.Toggle(qol, "DisableCameraShake", Rumble::InstallDisableCameraShake);
    installer.Toggle(qol, "DisableRearviewMirror", FEngHud::InstallDisableRearviewMirror);
    installer.Toggle(qol, "UnmuteInBackground", WindowProc::InstallUnmuteInBackground);
    installer.Toggle(qol, "DisableCopRadio", Speech::InstallDisableCopRadio);
}
