#include "../dllmain.hpp"

#include <bit>
#include <cstdlib>
#include <string>

namespace {

    namespace Rain {

        constexpr Memory::Pattern kSetRainIntensity =
            Memory::ParsePattern("A1 ?? ?? ?? ?? 85 C0 56 8B F1 74 0D A1 ?? ?? ?? ?? 89 86 8C 02 00 00");

        constexpr std::ptrdiff_t kAlwaysRainOperand        = 1;
        constexpr std::ptrdiff_t kGeneralRainAmountOperand = 13;

        constexpr std::array<std::uint8_t, 4> kOn{ 0x01, 0x00, 0x00, 0x00 };

        std::optional<ScopedPatch> g_AlwaysRain;
        std::optional<ScopedPatch> g_GeneralRainAmount;

        bool InstallRain(float amount) noexcept {
            const auto alwaysRain = Hook::Operand(kSetRainIntensity, kAlwaysRainOperand);
            const auto rainAmount = Hook::Operand(kSetRainIntensity, kGeneralRainAmountOperand);
            if (!alwaysRain || !rainAmount || *alwaysRain == 0 || *rainAmount == 0) return false;

            g_GeneralRainAmount.emplace(*rainAmount, std::bit_cast<std::array<std::uint8_t, 4>>(amount));
            g_AlwaysRain.emplace(*alwaysRain, kOn);
            if (*g_GeneralRainAmount && *g_AlwaysRain) return true;

            g_AlwaysRain.reset();
            g_GeneralRainAmount.reset();
            return false;
        }

        std::optional<float> WeatherAmount(const std::string& weather) {
            if (weather == "clear") return 0.0f;
            if (weather == "drizzle") return 0.35f;
            if (weather == "rain") return 0.7f;
            if (weather == "storm") return 1.0f;

            char* end = nullptr;
            const float amount = std::strtof(weather.c_str(), &end);
            if (end == weather.c_str() || *end != '\0' || amount < 0.0f || amount > 1.0f) return std::nullopt;
            return amount;
        }

    }

    namespace TimeOfDay {

        constexpr Memory::Pattern kUpdate =
            Memory::ParsePattern("83 EC 18 83 3D ?? ?? ?? ?? 03 53 55 56 57 8B F1 C7 44 24 10 33 33 B3 3E");

        constexpr std::size_t    kUpdateStolen = 10;
        constexpr std::ptrdiff_t kDirection    = 0x04;
        constexpr std::ptrdiff_t kTime         = 0x08;

        using UpdateFn = void(__thiscall*)(void*);

        std::uintptr_t g_Update = 0;
        float          g_time   = 0.5f;
        ScopedHook     g_UpdateHook;

        void __fastcall UpdateHook(void* timeOfDay, void*) {
            Memory::Guarded([&] {
                Game::Field<int>(timeOfDay, kDirection) = 0;
                Game::Field<float>(timeOfDay, kTime)    = g_time;
            });
            reinterpret_cast<UpdateFn>(g_Update)(timeOfDay);
        }

        bool InstallFreezeTimeOfDay(float time) noexcept {
            g_time = time;
            return Hook::Detour(g_UpdateHook, kUpdate, kUpdateStolen, reinterpret_cast<const void*>(&UpdateHook), g_Update);
        }

    }

}

void ApplyWeather(const IniSection& weather, Installer& installer) {
    if (const std::string preset = weather.String("SetWeather"); !preset.empty()) {
        const auto amount = Rain::WeatherAmount(preset);
        installer.Check("SetWeather", amount.has_value() && Rain::InstallRain(*amount));
    } else if (weather.Bool("AlwaysRain")) {
        installer.Check("AlwaysRain", Rain::InstallRain(1.0f));
    }
    if (weather.Has("TimeOfDay")) {
        installer.Check("TimeOfDay", TimeOfDay::InstallFreezeTimeOfDay(weather.Float("TimeOfDay", 0.5f, 0.0f, 1.0f)));
    }
}
