#include "../dllmain.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

    namespace CubicCameraMover {

        constexpr Memory::Pattern kUpdateFovStore =
            Memory::ParsePattern("8B 0D ?? ?? ?? ?? 83 C4 28 D1 E0 85 C9 75 0A 8B 4E 1C 66 89 81 C4 00 00 00 "
                                 "8D 54 24 70");

        constexpr std::ptrdiff_t kFovStoreSite   = 15;
        constexpr std::size_t    kFovStoreLength = 10;
        constexpr std::size_t    kCallSize       = 5;
        constexpr std::uint8_t   kCall           = 0xE8;
        constexpr std::uint8_t   kNop            = 0x90;
        constexpr float          kAngleUnitsPerDegree = 65536.0f / 360.0f;
        constexpr int            kNarrowestFov   = static_cast<int>(10.0f * kAngleUnitsPerDegree);
        constexpr int            kWidestFov      = static_cast<int>(160.0f * kAngleUnitsPerDegree);

        constexpr std::array<std::uint8_t, 6>  kPushCameraAndFov{ 0x8B, 0x4E, 0x1C, 0x51, 0x50, 0x51 };
        constexpr std::array<std::uint8_t, 12> kStoreFov{ 0x83, 0xC4, 0x08, 0x59, 0x66, 0x89,
                                                          0x81, 0xC4, 0x00, 0x00, 0x00, 0xC3 };

        struct WidenedFov {
            const void*   camera;
            std::uint16_t game;
            std::uint16_t shown;
        };

        std::array<WidenedFov, 4> g_widened{};
        std::size_t               g_nextWidened = 0;
        int                       g_extraFov    = 0;
        ScopedHook                g_FovStore;

        std::uint32_t __cdecl WidenFov(const void* camera, std::uint32_t fov) {
            const auto game  = static_cast<std::uint16_t>(fov);
            const auto shown = static_cast<std::uint16_t>(std::clamp(game + g_extraFov, kNarrowestFov, kWidestFov));

            auto slot = std::ranges::find(g_widened, camera, &WidenedFov::camera);
            if (slot == g_widened.end()) {
                slot          = g_widened.begin() + static_cast<std::ptrdiff_t>(g_nextWidened);
                g_nextWidened = (g_nextWidened + 1) % g_widened.size();
            }
            *slot = { camera, game, shown };
            return shown;
        }

        void EncodeCall(std::uint8_t* at, std::uintptr_t from, std::uintptr_t to) noexcept {
            const auto relative = static_cast<std::int32_t>(to - (from + kCallSize));
            at[0] = kCall;
            std::memcpy(at + 1, &relative, sizeof(relative));
        }

        bool InstallFovSlider(float degrees) noexcept {
            if (g_FovStore) return true;

            const auto match = Scan::Find(kUpdateFovStore);
            if (!match) return false;

            g_FovStore.block.emplace(kPushCameraAndFov.size() + kCallSize + kStoreFov.size());
            if (!*g_FovStore.block) {
                g_FovStore.Reset();
                return false;
            }

            std::uint8_t* const  stub = g_FovStore.block->data();
            const std::uintptr_t base = g_FovStore.block->address();
            std::memcpy(stub, kPushCameraAndFov.data(), kPushCameraAndFov.size());
            EncodeCall(stub + kPushCameraAndFov.size(), base + kPushCameraAndFov.size(), reinterpret_cast<std::uintptr_t>(&WidenFov));
            std::memcpy(stub + kPushCameraAndFov.size() + kCallSize, kStoreFov.data(), kStoreFov.size());
            FlushInstructionCache(GetCurrentProcess(), stub, kPushCameraAndFov.size() + kCallSize + kStoreFov.size());

            const std::uintptr_t                      site = *match + kFovStoreSite;
            std::array<std::uint8_t, kFovStoreLength> call{};
            call.fill(kNop);
            EncodeCall(call.data(), site, base);

            g_extraFov = static_cast<int>(std::lround(degrees * kAngleUnitsPerDegree));
            g_FovStore.patch.emplace(site, call);
            if (*g_FovStore.patch) return true;

            g_FovStore.Reset();
            return false;
        }

        std::optional<std::uint16_t> GetGameFov(const void* camera, std::uint16_t shown) noexcept {
            if (camera == nullptr) return std::nullopt;

            for (const WidenedFov& widened : g_widened) {
                if (widened.camera == camera && widened.shown == shown) return widened.game;
            }
            return std::nullopt;
        }

    }

    namespace CameraMover {

        constexpr Memory::Pattern kSetEyeLook =
            Memory::ParsePattern("8B 44 24 0C 53 56 57 50 8B F1 E8 ?? ?? ?? ?? 8B 7C 24 20 8B 5C 24 1C");

        constexpr Memory::Pattern kFovCubicInit =
            Memory::ParsePattern("51 8B 41 1C 0F B7 88 84 02 00 00 0F B7 90 C4 00 00 00 89 0C 24 8B 4C 24 08 "
                                 "DB 04 24");

        constexpr std::ptrdiff_t kFovCubicInitCall   = 11;
        constexpr std::size_t    kFovCubicInitStolen = 11;
        constexpr std::ptrdiff_t kCamera             = 0x1C;
        constexpr std::ptrdiff_t kCameraFov          = 0xC4;

        using FovCubicInitFn = void(__thiscall*)(void*, void*);

        std::uintptr_t g_FovCubicInit = 0;
        ScopedHook     g_FovCubicInitHook;

        void __fastcall FovCubicInitHook(void* mover, void*, void* cubic) {
            std::uint16_t* fov   = nullptr;
            std::uint16_t  shown = 0;

            Memory::Guarded([&] {
                auto* const camera = Game::Field<std::byte*>(mover, kCamera);
                if (camera == nullptr) return;

                std::uint16_t& current = Game::Field<std::uint16_t>(camera, kCameraFov);
                if (const auto game = CubicCameraMover::GetGameFov(camera, current)) {
                    shown   = current;
                    current = *game;
                    fov     = &current;
                }
            });

            reinterpret_cast<FovCubicInitFn>(g_FovCubicInit)(mover, cubic);
            if (fov != nullptr) *fov = shown;
        }

        bool InstallFovCubicInit() noexcept {
            const auto target = Hook::Operand(kSetEyeLook, kFovCubicInitCall, true);
            if (!target || !Scan::At(*target, kFovCubicInit)) return false;

            return Hook::Detour(g_FovCubicInitHook, *target, kFovCubicInitStolen,
                                reinterpret_cast<const void*>(&FovCubicInitHook), g_FovCubicInit);
        }

    }

}

void ApplyCamera(const IniSection& camera, const IniSection& misc, Installer& installer) {
    const IniSection& section = camera.Has("FOVSlider") ? camera : misc;
    if (const float degrees = section.Float("FOVSlider", 0.0f, -60.0f, 90.0f); degrees != 0.0f) {
        installer.Check("FOVSlider", CubicCameraMover::InstallFovSlider(degrees) && CameraMover::InstallFovCubicInit());
    }
}
