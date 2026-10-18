#include "../dllmain.hpp"

#include <algorithm>
#include <cstdio>
#include <string>

namespace {

    namespace VideoOptions {

        constexpr Memory::Pattern kEffectsEnabled =
            Memory::ParsePattern("A1 ?? ?? ?? ?? 85 C0 75 30 A1 ?? ?? ?? ?? 85 C0 75 27 A1 ?? ?? ?? ?? 85 C0 75 1E "
                                 "A1 ?? ?? ?? ?? 85 C0 75 15 A1 ?? ?? ?? ?? 85 C0 75 0C");

        constexpr std::ptrdiff_t kg_MotionBlurEnableOperand = 0x01;
        constexpr std::ptrdiff_t kg_VisualTreatmentOperand  = 0x25;

        int* g_MotionBlurEnable = nullptr;
        int* g_VisualTreatment  = nullptr;
        bool g_noMotionBlur     = false;
        bool g_noVisualFilters  = false;

        void ForceVideoOptions() {
            if (g_noMotionBlur && g_MotionBlurEnable != nullptr) *g_MotionBlurEnable = 0;
            if (g_noVisualFilters && g_VisualTreatment != nullptr) *g_VisualTreatment = 0;
        }

        bool Resolve() noexcept {
            if (g_MotionBlurEnable != nullptr) return true;

            const auto motionBlur = Hook::Operand(kEffectsEnabled, kg_MotionBlurEnableOperand);
            const auto treatment  = Hook::Operand(kEffectsEnabled, kg_VisualTreatmentOperand);
            if (!motionBlur || !treatment || *motionBlur == 0 || *treatment == 0) return false;

            g_MotionBlurEnable = reinterpret_cast<int*>(*motionBlur);
            g_VisualTreatment  = reinterpret_cast<int*>(*treatment);
            return true;
        }

        bool InstallDisableVisualFilters() noexcept {
            if (!Resolve() || !AddFrameCallback(&ForceVideoOptions)) return false;
            g_noVisualFilters = true;
            g_noMotionBlur    = true;
            return true;
        }

        bool InstallDisableMotionBlur() noexcept {
            if (!Resolve()) return false;
            if (!g_noVisualFilters && !AddFrameCallback(&ForceVideoOptions)) return false;
            g_noMotionBlur = true;
            return true;
        }

    }

    namespace CarRenderInfo {

        constexpr Memory::Pattern kSetDamageInfo =
            Memory::ParsePattern("56 8B F1 8B 8E A0 17 00 00 57 8B 7C 24 0C 8B 07 3B C1 8D 96 A0 17 00 00");

        constexpr std::array<std::uint8_t, 3> kReturn{ 0xC2, 0x04, 0x00 };

        std::optional<ScopedPatch> g_SetDamageInfo;

        bool InstallDisableVehicleDamage() noexcept {
            return Hook::Patch(g_SetDamageInfo, kSetDamageInfo, 0, kReturn);
        }

    }

    namespace TireState {

        constexpr Memory::Pattern kDoFX =
            Memory::ParsePattern("D9 44 24 04 64 A1 00 00 00 00 D8 1D ?? ?? ?? ?? 6A FF 68 ?? ?? ?? ??");

        constexpr std::size_t kDoFXStolen = 10;

        using DoFXFn = void(__thiscall*)(void*, float, float, float, const void*, const void*, float);

        std::uintptr_t g_DoFX = 0;
        ScopedHook     g_DoFXHook;

        void __fastcall DoFXHook(void* tire, void*, float, float, float speed, const void* velocity, const void* matrix,
                                 float deltaTime) {
            reinterpret_cast<DoFXFn>(g_DoFX)(tire, 0.0f, 0.0f, speed, velocity, matrix, deltaTime);
        }

        bool InstallDisableTireSmoke() noexcept {
            return Hook::Detour(g_DoFXHook, kDoFX, kDoFXStolen, reinterpret_cast<const void*>(&DoFXHook), g_DoFX);
        }

    }

    namespace EmitterData {

        constexpr Memory::Pattern kFindCollection =
            Memory::ParsePattern("A1 ?? ?? ?? ?? 56 8B 70 04 8B 44 24 08 83 C6 08 85 C0 74 5C");

        constexpr std::uint32_t  kEmitterDataClass = 0xB30B18AF;
        constexpr std::uint32_t  kEmCarNosFire     = 0x78F47BF0;
        constexpr std::uint32_t  kEmCarNosGlow     = 0x3BB289ED;
        constexpr std::ptrdiff_t kCollectionLayout = 0x18;
        constexpr std::uint32_t  kAlpha            = 0xFF000000;

        constexpr std::array<std::ptrdiff_t, 4> kColors{ 0xAC, 0xE4, 0xB0, 0xB4 };
        constexpr std::array<std::uint32_t, 2>  kNosEmitters{ kEmCarNosFire, kEmCarNosGlow };

        using FindCollectionFn = const void*(__cdecl*)(std::uint32_t, std::uint32_t);

        std::uintptr_t g_FindCollection = 0;
        std::uint32_t  g_flameColor     = 0;
        bool           g_recoloured     = false;

        void RecolourNosFlames() {
            if (g_recoloured) return;

            for (const std::uint32_t emitter : kNosEmitters) {
                const void* const collection = reinterpret_cast<FindCollectionFn>(g_FindCollection)(kEmitterDataClass, emitter);
                if (collection == nullptr) return;
            }

            for (const std::uint32_t emitter : kNosEmitters) {
                const void* const collection = reinterpret_cast<FindCollectionFn>(g_FindCollection)(kEmitterDataClass, emitter);
                auto* const layout = Game::Field<std::byte*>(const_cast<void*>(collection), kCollectionLayout);
                if (layout == nullptr) continue;

                for (const std::ptrdiff_t color : kColors) {
                    std::uint32_t& value = Game::Field<std::uint32_t>(layout, color);
                    value = (value & kAlpha) | g_flameColor;
                }
            }
            g_recoloured = true;
        }

        std::optional<std::uint32_t> ParseColor(const std::string& text) {
            int red = 0;
            int green = 0;
            int blue = 0;
            if (std::sscanf(text.c_str(), "%d , %d , %d", &red, &green, &blue) != 3) return std::nullopt;

            const auto channel = [](int value) { return static_cast<std::uint32_t>(std::clamp(value, 0, 255)); };
            return channel(red) << 16 | channel(green) << 8 | channel(blue);
        }

        bool InstallExhaustFlameColor(std::uint32_t color) noexcept {
            const auto find = Scan::Find(kFindCollection);
            if (!find) return false;

            g_FindCollection = *find;
            g_flameColor     = color;
            return AddFrameCallback(&RecolourNosFlames);
        }

    }

}

void ApplyVisuals(const IniSection& visuals, Installer& installer) {
    installer.Toggle(visuals, "DisableVisualFilters", VideoOptions::InstallDisableVisualFilters);
    installer.Toggle(visuals, "DisableMotionBlur", VideoOptions::InstallDisableMotionBlur);
    installer.Toggle(visuals, "DisableVehicleDamage", CarRenderInfo::InstallDisableVehicleDamage);
    installer.Toggle(visuals, "DisableTireSmoke", TireState::InstallDisableTireSmoke);
    if (const std::string color = visuals.String("ExhaustFlameColor"); !color.empty()) {
        const auto parsed = EmitterData::ParseColor(color);
        installer.Check("ExhaustFlameColor", parsed.has_value() && EmitterData::InstallExhaustFlameColor(*parsed));
    }
}
