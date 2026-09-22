#include "CubicCameraMoverUpdate.hpp"

#include "../dllmain.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace CubicCameraMoverUpdate {

    namespace {

        constexpr Memory::Pattern kFieldOfViewStore =
            Memory::ParsePattern("8B 0D ?? ?? ?? ?? 83 C4 28 D1 E0 85 C9 75 0A 8B 4E 1C 66 89 81 C4 00 00 00 "
                                 "8D 54 24 70");

        constexpr std::ptrdiff_t kStoreSite      = 15;
        constexpr std::size_t    kStoreLength    = 10;
        constexpr std::size_t    kBranchSize     = 5;
        constexpr std::uint8_t   kCall           = 0xE8;
        constexpr std::uint8_t   kNop            = 0x90;
        constexpr float          kUnitsPerDegree = 65536.0f / 360.0f;
        constexpr int            kNarrowest      = static_cast<int>(10.0f * kUnitsPerDegree);
        constexpr int            kWidest         = static_cast<int>(160.0f * kUnitsPerDegree);

        constexpr std::array<std::uint8_t, 6>  kLoadCamera{ 0x8B, 0x4E, 0x1C, 0x51, 0x50, 0x51 };
        constexpr std::array<std::uint8_t, 12> kStoreAngle{ 0x83, 0xC4, 0x08, 0x59, 0x66, 0x89,
                                                            0x81, 0xC4, 0x00, 0x00, 0x00, 0xC3 };

        struct Widened {
            const void*   camera;
            std::uint16_t raw;
            std::uint16_t shown;
        };

        std::array<Widened, 4> g_widened{};
        std::size_t            g_next   = 0;
        int                    g_offset = 0;
        ScopedHook             g_hook;

        std::uint32_t __cdecl Widen(const void* camera, std::uint32_t fieldOfView) {
            const auto raw   = static_cast<std::uint16_t>(fieldOfView);
            const auto shown = static_cast<std::uint16_t>(std::clamp(raw + g_offset, kNarrowest, kWidest));

            auto slot = std::ranges::find(g_widened, camera, &Widened::camera);
            if (slot == g_widened.end()) {
                slot   = g_widened.begin() + static_cast<std::ptrdiff_t>(g_next);
                g_next = (g_next + 1) % g_widened.size();
            }
            *slot = { camera, raw, shown };
            return shown;
        }

        void EncodeCall(std::uint8_t* at, std::uintptr_t from, std::uintptr_t to) noexcept {
            const auto relative = static_cast<std::int32_t>(to - (from + kBranchSize));
            at[0] = kCall;
            std::memcpy(at + 1, &relative, sizeof(relative));
        }

    }

    bool InstallFieldOfView(float degrees) noexcept {
        if (g_hook) return true;

        const auto match = Scan::Find(kFieldOfViewStore);
        if (!match) return false;

        g_hook.block.emplace(kLoadCamera.size() + kBranchSize + kStoreAngle.size());
        if (!*g_hook.block) {
            g_hook.Reset();
            return false;
        }

        std::uint8_t* const  stub = g_hook.block->data();
        const std::uintptr_t base = g_hook.block->address();
        std::memcpy(stub, kLoadCamera.data(), kLoadCamera.size());
        EncodeCall(stub + kLoadCamera.size(), base + kLoadCamera.size(), reinterpret_cast<std::uintptr_t>(&Widen));
        std::memcpy(stub + kLoadCamera.size() + kBranchSize, kStoreAngle.data(), kStoreAngle.size());
        FlushInstructionCache(GetCurrentProcess(), stub, kLoadCamera.size() + kBranchSize + kStoreAngle.size());

        const std::uintptr_t                  site = *match + kStoreSite;
        std::array<std::uint8_t, kStoreLength> call{};
        call.fill(kNop);
        EncodeCall(call.data(), site, base);

        g_offset = static_cast<int>(std::lround(degrees * kUnitsPerDegree));
        g_hook.patch.emplace(site, call);
        if (*g_hook.patch) return true;

        g_hook.Reset();
        return false;
    }

    std::optional<std::uint16_t> UnwidenedFieldOfView(const void* camera, std::uint16_t shown) noexcept {
        if (camera == nullptr) return std::nullopt;

        for (const Widened& widened : g_widened) {
            if (widened.camera == camera && widened.shown == shown) return widened.raw;
        }
        return std::nullopt;
    }

}
