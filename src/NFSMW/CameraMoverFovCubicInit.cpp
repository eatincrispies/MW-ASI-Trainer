#include "CameraMoverFovCubicInit.hpp"

#include "../dllmain.hpp"
#include "CubicCameraMoverUpdate.hpp"

namespace CameraMoverFovCubicInit {

    namespace {

        constexpr Memory::Pattern kSetEyeLook =
            Memory::ParsePattern("8B 44 24 0C 53 56 57 50 8B F1 E8 ?? ?? ?? ?? 8B 7C 24 20 8B 5C 24 1C");

        constexpr Memory::Pattern kFovCubicInit =
            Memory::ParsePattern("51 8B 41 1C 0F B7 88 84 02 00 00 0F B7 90 C4 00 00 00 89 0C 24 8B 4C 24 08 "
                                 "DB 04 24");

        constexpr std::ptrdiff_t kFovCubicInitCall = 11;
        constexpr std::size_t    kStolenBytes      = 11;
        constexpr std::ptrdiff_t kCamera           = 0x1C;
        constexpr std::ptrdiff_t kFieldOfView      = 0xC4;

        using FovCubicInitFn = void(__thiscall*)(void*, void*);

        std::uintptr_t g_original = 0;
        ScopedHook     g_detour;

        void __fastcall FovCubicInit(void* mover, void*, void* cubic) {
            std::uint16_t* fieldOfView = nullptr;
            std::uint16_t  shown       = 0;

            Memory::Guarded([&] {
                auto* const camera = Game::Field<std::byte*>(mover, kCamera);
                if (camera == nullptr) return;

                std::uint16_t& current = Game::Field<std::uint16_t>(camera, kFieldOfView);
                if (const auto raw = CubicCameraMoverUpdate::UnwidenedFieldOfView(camera, current)) {
                    shown       = current;
                    current     = *raw;
                    fieldOfView = &current;
                }
            });

            reinterpret_cast<FovCubicInitFn>(g_original)(mover, cubic);
            if (fieldOfView != nullptr) *fieldOfView = shown;
        }

    }

    bool Install() noexcept {
        const auto target = Hook::Operand(kSetEyeLook, kFovCubicInitCall, true);
        if (!target || !Scan::At(*target, kFovCubicInit)) return false;

        return Hook::Detour(g_detour, *target, kStolenBytes, reinterpret_cast<const void*>(&FovCubicInit), g_original);
    }

}
