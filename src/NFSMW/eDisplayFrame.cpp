#include "eDisplayFrame.hpp"

#include "../dllmain.hpp"
#include "../UI/Animation/360anim.hpp"

#include <atomic>
#include <cstring>
#include <new>

namespace eDisplayFrame {

    namespace {

        constexpr Memory::Pattern kEndScene =
            Memory::ParsePattern("57 E8 ?? ?? ?? ?? A1 ?? ?? ?? ?? 8B 10 83 C4 04 50 FF 92 A8 00 00 00 "
                                 "E8 ?? ?? ?? ?? 39 3D ?? ?? ?? ?? 74 0F");

        constexpr std::ptrdiff_t kEndSceneCall = 17;
        constexpr std::size_t    kCallSize     = 6;
        constexpr std::uint8_t   kNop          = 0x90;
        constexpr std::uint8_t   kCall         = 0xE8;

        std::atomic<Xbox360Anim*>  g_popup{ nullptr };
        std::optional<ScopedPatch> g_patch;

        HRESULT __stdcall EndScene(IDirect3DDevice9* device) {
            if (Xbox360Anim* const popup = g_popup.load(std::memory_order_acquire)) {
                bool playing = false;
                const bool clean = Memory::Guarded([&] { playing = popup->Render(device); });
                if (!playing) {
                    g_popup.store(nullptr, std::memory_order_release);
                    if (clean) delete popup;
                    g_patch.reset();
                }
            }
            return device->EndScene();
        }

    }

    bool InstallPopup(HMODULE resources) noexcept {
        if (g_patch) return true;

        const auto match = Scan::Find(kEndScene);
        if (!match) return false;

        auto* const popup = new (std::nothrow) Xbox360Anim(resources);
        if (popup == nullptr) return false;

        const std::uintptr_t site     = *match + kEndSceneCall;
        const auto           relative = static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(&EndScene) -
                                                                  (site + kCallSize));
        std::array<std::uint8_t, kCallSize> call{ kNop, kCall };
        std::memcpy(call.data() + 2, &relative, sizeof(relative));

        g_popup.store(popup, std::memory_order_release);
        g_patch.emplace(site, call);
        if (*g_patch) return true;

        g_patch.reset();
        g_popup.store(nullptr, std::memory_order_release);
        delete popup;
        return false;
    }

    void ShowPopup() noexcept {
        if (Xbox360Anim* const popup = g_popup.load(std::memory_order_acquire)) popup->Trigger();
    }

}
