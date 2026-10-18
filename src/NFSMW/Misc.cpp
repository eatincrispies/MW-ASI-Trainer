#include "../dllmain.hpp"

#include "../UI/Animation/360anim.hpp"

#include <atomic>
#include <bit>
#include <cstring>
#include <new>

namespace {

    namespace eDisplayFrame {

        constexpr Memory::Pattern kEndSceneCall =
            Memory::ParsePattern("57 E8 ?? ?? ?? ?? A1 ?? ?? ?? ?? 8B 10 83 C4 04 50 FF 92 A8 00 00 00 "
                                 "E8 ?? ?? ?? ?? 39 3D ?? ?? ?? ?? 74 0F");

        constexpr std::ptrdiff_t kEndSceneSite = 17;
        constexpr std::size_t    kCallSize     = 6;
        constexpr std::uint8_t   kNop          = 0x90;
        constexpr std::uint8_t   kCall         = 0xE8;

        std::atomic<Xbox360Anim*>  g_popup{ nullptr };
        std::optional<ScopedPatch> g_EndScene;

        HRESULT __stdcall EndSceneHook(IDirect3DDevice9* device) {
            if (Xbox360Anim* const popup = g_popup.load(std::memory_order_acquire)) {
                bool playing = false;
                const bool clean = Memory::Guarded([&] { playing = popup->Render(device); });
                if (!playing) {
                    g_popup.store(nullptr, std::memory_order_release);
                    if (clean) delete popup;
                    g_EndScene.reset();
                }
            }
            return device->EndScene();
        }

        bool InstallLoadedPopup(HMODULE resources) noexcept {
            if (g_EndScene) return true;

            const auto match = Scan::Find(kEndSceneCall);
            if (!match) return false;

            auto* const popup = new (std::nothrow) Xbox360Anim(resources);
            if (popup == nullptr) return false;

            const std::uintptr_t site     = *match + kEndSceneSite;
            const auto           relative = static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(&EndSceneHook) -
                                                                      (site + kCallSize));
            std::array<std::uint8_t, kCallSize> call{ kNop, kCall };
            std::memcpy(call.data() + 2, &relative, sizeof(relative));

            g_popup.store(popup, std::memory_order_release);
            g_EndScene.emplace(site, call);
            if (*g_EndScene) return true;

            g_EndScene.reset();
            g_popup.store(nullptr, std::memory_order_release);
            delete popup;
            return false;
        }

    }

    namespace QuickGame {

        constexpr Memory::Pattern kOnManageTime =
            Memory::ParsePattern("51 D9 05 ?? ?? ?? ?? 56 D9 05 ?? ?? ?? ?? 8B F1 DA E9 DF E0");

        constexpr std::ptrdiff_t kTweak_GameSpeedOperand = 10;

        std::optional<ScopedPatch> g_Tweak_GameSpeed;

        bool InstallGameSpeed(float speed) noexcept {
            if (g_Tweak_GameSpeed) return true;

            const auto tweak = Hook::Operand(kOnManageTime, kTweak_GameSpeedOperand);
            if (!tweak || *tweak == 0) return false;

            g_Tweak_GameSpeed.emplace(*tweak, std::bit_cast<std::array<std::uint8_t, 4>>(speed));
            if (*g_Tweak_GameSpeed) return true;

            g_Tweak_GameSpeed.reset();
            return false;
        }

    }

}

void ApplyMisc(const IniSection& misc, Installer& installer, HMODULE resources) {
    if (misc.Bool("LoadedPopup", true)) installer.Check("LoadedPopup", eDisplayFrame::InstallLoadedPopup(resources));
    if (const float speed = misc.Float("GameSpeedMultiplier", 1.0f, 0.1f, 4.0f); speed != 1.0f) {
        installer.Check("GameSpeedMultiplier", QuickGame::InstallGameSpeed(speed));
    }
}

void ShowLoadedPopup() noexcept {
    if (Xbox360Anim* const popup = eDisplayFrame::g_popup.load(std::memory_order_acquire)) popup->Trigger();
}
