#include "MinimapUpdateCopElements.hpp"

#include "../dllmain.hpp"

#include <algorithm>
#include <array>

namespace MinimapUpdateCopElements {

    namespace {

        constexpr Memory::Pattern kUpdateCopElements =
            Memory::ParsePattern("55 8B EC 83 E4 F8 83 EC 24 53 8B D9 8B 83 C0 00 00 00 56 57 33 FF 83 F8 08");

        constexpr Memory::Pattern kCopVehicles =
            Memory::ParsePattern("8B 35 ?? ?? ?? ?? A1 ?? ?? ?? ?? 8D 0C 86 3B F1 0F 84 ?? ?? ?? ?? "
                                 "8D 93 D0 00 00 00 89 54 24 14");

        constexpr Memory::Pattern kVehicleVectors =
            Memory::ParsePattern("50 8D 44 24 24 50 8D 4C 24 30 51 E8 ?? ?? ?? ?? 8B 06 8B 48 04 83 C4 0C");

        constexpr Memory::Pattern kElementArt =
            Memory::ParsePattern("8B 8B C8 00 00 00 6A 00 51 8D 54 24 28 52 8D 44 24 34 50 8B CB E8 ?? ?? ?? ??");

        constexpr Memory::Pattern kSetColor =
            Memory::ParsePattern("8B 44 24 1C 8B 4C 24 14 8B 11 50 52 E8 ?? ?? ?? ?? 83 C4 08");

        constexpr Memory::Pattern kChopperClass =
            Memory::ParsePattern("FF 50 44 8B 08 3B 0D ?? ?? ?? ?? 74 14 8B 4C 24 18");

        constexpr Memory::Pattern kTextureHashCall =
            Memory::ParsePattern("E8 ?? ?? ?? ?? 8B 0B 50 51 E8 ?? ?? ?? ?? 83 C4 20 47 83 C3 04");

        constexpr std::size_t    kStolenBytes       = 9;
        constexpr std::ptrdiff_t kVehicleListField  = 2;
        constexpr std::ptrdiff_t kVehicleCountField = 7;
        constexpr std::ptrdiff_t kVectorsCall       = 12;
        constexpr std::ptrdiff_t kElementArtCall    = 22;
        constexpr std::ptrdiff_t kSetColorCall      = 13;
        constexpr std::ptrdiff_t kChopperClassRead  = 7;
        constexpr std::ptrdiff_t kTextureHashSet    = 10;

        constexpr std::ptrdiff_t kHelicopterGroup = 0xC8;
        constexpr std::ptrdiff_t kCopIcons        = 0xD0;
        constexpr std::size_t    kCopIconCount    = 8;
        constexpr std::ptrdiff_t kGroupChild      = 0x64;
        constexpr std::ptrdiff_t kNextObject      = 0x04;
        constexpr std::ptrdiff_t kObjectFlags     = 0x1C;
        constexpr std::ptrdiff_t kObjectType      = 0x18;
        constexpr std::ptrdiff_t kTextureHash     = 0x24;
        constexpr std::uint32_t  kHiddenFlag      = 0x01;
        constexpr int            kImageType       = 1;
        constexpr std::ptrdiff_t kGetSimable      = 0x04;
        constexpr std::ptrdiff_t kIsActive        = 0x88;
        constexpr std::ptrdiff_t kGetClass        = 0x44;
        constexpr std::uint32_t  kIconColor       = 0xFFFFFFFF;

        using UpdateCopElementsFn = void(__thiscall*)(void*, void*);
        using VehicleVectorsFn    = void(__cdecl*)(float*, float*, void*);
        using ElementArtFn        = void(__thiscall*)(void*, const float*, const float*, void*, int);
        using SetColorFn          = void(__cdecl*)(void*, std::uint32_t);
        using SetTextureHashFn    = void(__cdecl*)(void*, std::uint32_t);

        struct Borrowed {
            void*         element;
            std::uint32_t texture;
        };

        std::uintptr_t                      g_original       = 0;
        std::uintptr_t                      g_vehicleVectors = 0;
        std::uintptr_t                      g_elementArt     = 0;
        std::uintptr_t                      g_setColor       = 0;
        std::uintptr_t                      g_setTextureHash = 0;
        void***                             g_vehicles       = nullptr;
        const int*                          g_vehicleCount   = nullptr;
        const std::uint32_t*                g_chopperClass   = nullptr;
        std::array<Borrowed, kCopIconCount> g_borrowed{};
        std::size_t                         g_borrowedCount = 0;
        ScopedHook                          g_detour;

        bool IsHelicopter(void* vehicle) {
            const auto* const vehicleClass = Game::CallVirtual<const std::uint32_t*>(vehicle, kGetClass);
            return vehicleClass != nullptr && *vehicleClass == *g_chopperClass;
        }

        std::uint32_t HelicopterTexture(void* minimap) {
            auto* const group = Game::Field<std::byte*>(minimap, kHelicopterGroup);
            if (group == nullptr) return 0;

            for (auto* child = Game::Field<std::byte*>(group, kGroupChild); child != nullptr;
                 child = Game::Field<std::byte*>(child, kNextObject)) {
                if (Game::Field<int>(child, kObjectType) == kImageType) {
                    return Game::Field<std::uint32_t>(child, kTextureHash);
                }
            }
            return 0;
        }

        bool IsCopIcon(void* minimap, const void* element) {
            for (std::size_t slot = 0; slot < kCopIconCount; ++slot) {
                if (Game::Field<void*>(minimap, kCopIcons + static_cast<std::ptrdiff_t>(slot) * 4) == element) {
                    return true;
                }
            }
            return false;
        }

        void* FreeIcon(void* minimap, std::size_t taken) {
            void* fallback = nullptr;

            for (std::size_t slot = kCopIconCount; slot-- > 0;) {
                void* const element = Game::Field<void*>(minimap, kCopIcons + static_cast<std::ptrdiff_t>(slot) * 4);
                if (element == nullptr) continue;

                const bool borrowed = std::any_of(g_borrowed.begin(), g_borrowed.begin() + taken,
                                                  [element](const Borrowed& used) { return used.element == element; });
                if (borrowed) continue;
                if ((Game::Field<std::uint32_t>(element, kObjectFlags) & kHiddenFlag) != 0) return element;
                if (fallback == nullptr) fallback = element;
            }
            return fallback;
        }

        void Restore(void* minimap) {
            for (std::size_t index = 0; index < g_borrowedCount; ++index) {
                const Borrowed& used = g_borrowed[index];
                if (used.element != nullptr && IsCopIcon(minimap, used.element)) {
                    reinterpret_cast<SetTextureHashFn>(g_setTextureHash)(used.element, used.texture);
                }
            }
            g_borrowedCount = 0;
        }

        void ShowHelicopters(void* minimap) {
            void** const vehicles = g_vehicles != nullptr ? *g_vehicles : nullptr;
            const int    count    = g_vehicleCount != nullptr ? *g_vehicleCount : 0;
            if (vehicles == nullptr || count <= 0) return;

            std::array<void*, kCopIconCount + 1> helicopters{};
            std::size_t                          found = 0;

            for (int index = 0; index < count && found < helicopters.size(); ++index) {
                void* const vehicle = vehicles[index];
                if (vehicle == nullptr || !Game::CallVirtual<bool>(vehicle, kIsActive)) continue;
                if (IsHelicopter(vehicle)) helicopters[found++] = vehicle;
            }
            if (found < 2) return;

            const std::uint32_t texture = HelicopterTexture(minimap);

            for (std::size_t index = 0; index + 1 < found; ++index) {
                void* const simable = Game::CallVirtual<void*>(helicopters[index], kGetSimable);
                if (simable == nullptr) continue;

                void* const element = FreeIcon(minimap, g_borrowedCount);
                if (element == nullptr) return;

                g_borrowed[g_borrowedCount++] = { element, Game::Field<std::uint32_t>(element, kTextureHash) };
                if (texture != 0) reinterpret_cast<SetTextureHashFn>(g_setTextureHash)(element, texture);

                std::array<float, 4> position{};
                std::array<float, 4> heading{};
                reinterpret_cast<VehicleVectorsFn>(g_vehicleVectors)(position.data(), heading.data(), simable);

                reinterpret_cast<SetColorFn>(g_setColor)(element, kIconColor);
                reinterpret_cast<ElementArtFn>(g_elementArt)(minimap, position.data(), heading.data(), element, 0);
            }
        }

        void __fastcall UpdateCopElements(void* minimap, void*, void* vehicle) {
            Memory::Guarded([&] { Restore(minimap); });
            reinterpret_cast<UpdateCopElementsFn>(g_original)(minimap, vehicle);
            Memory::Guarded([&] { ShowHelicopters(minimap); });
        }

    }

    bool InstallHelicopterIcons() noexcept {
        const auto list          = Scan::Find(kCopVehicles);
        const auto vectors       = Hook::Operand(kVehicleVectors, kVectorsCall, true);
        const auto elementArt    = Hook::Operand(kElementArt, kElementArtCall, true);
        const auto setColor      = Hook::Operand(kSetColor, kSetColorCall, true);
        const auto chopperClass  = Hook::Operand(kChopperClass, kChopperClassRead);
        const auto textureHash   = Hook::Operand(kTextureHashCall, kTextureHashSet, true);
        if (!list || !vectors || !elementArt || !setColor || !chopperClass || !textureHash) return false;

        const auto vehicles = Scan::Absolute(*list + kVehicleListField);
        const auto counted  = Scan::Absolute(*list + kVehicleCountField);
        if (!vehicles || !counted || *vehicles == 0 || *counted == 0 || *chopperClass == 0) return false;

        for (const auto& function : { vectors, elementArt, setColor, textureHash }) {
            if (!Memory::IsExecutable(reinterpret_cast<const void*>(*function))) return false;
        }

        g_vehicles       = reinterpret_cast<void***>(*vehicles);
        g_vehicleCount   = reinterpret_cast<const int*>(*counted);
        g_chopperClass   = reinterpret_cast<const std::uint32_t*>(*chopperClass);
        g_vehicleVectors = *vectors;
        g_elementArt     = *elementArt;
        g_setColor       = *setColor;
        g_setTextureHash = *textureHash;

        return Hook::Detour(g_detour, kUpdateCopElements, kStolenBytes,
                            reinterpret_cast<const void*>(&UpdateCopElements), g_original);
    }

}
