#include "FEPlayerCarDBDefault.hpp"

#include "../dllmain.hpp"
#include "FECarRecord.hpp"

#include <algorithm>
#include <memory>
#include <vector>

namespace FEPlayerCarDBDefault {

    namespace {

        constexpr Memory::Pattern kDefault =
            Memory::ParsePattern("6A FF 64 A1 00 00 00 00 68 ?? ?? ?? ?? 50 64 89 25 00 00 00 00 83 EC 2C 53 55 "
                                 "8B D9 56 57 8B C3 B9 C8 00 00 00 C7 00 FF FF FF FF 83 C0 14 49 75 F4 "
                                 "8D 83 34 11 00 00 B9 4B 00 00 00");

        constexpr Memory::Pattern kPresetCars =
            Memory::ParsePattern("68 ?? ?? ?? ?? 8B CB E8 ?? ?? ?? ?? C7 00 78 56 34 12 C7 40 0C 20 00 0F 00 "
                                 "33 FF E8 ?? ?? ?? ?? 85 C0 0F 8E ?? ?? ?? ?? BD 08 00 0F 00 8D 64 24 00 "
                                 "57 E8 ?? ?? ?? ?? 8D 70 28 56 E8");

        constexpr Memory::Pattern kShowAllCars =
            Memory::ParsePattern("80 38 00 75 09 A0 ?? ?? ?? ?? 84 C0 74 66 85 F6 74 62");

        constexpr std::size_t    kStolenBytes         = 8;
        constexpr std::ptrdiff_t kCreatePresetCall    = 8;
        constexpr std::ptrdiff_t kPresetCountCall     = 28;
        constexpr std::ptrdiff_t kPresetAtCall        = 51;
        constexpr std::ptrdiff_t kHashCall            = 60;
        constexpr std::ptrdiff_t kShowAllCarsFlag     = 6;
        constexpr std::ptrdiff_t kPresetName          = 0x28;
        constexpr int            kSpareRecords        = 40;
        constexpr int            kSpareCustomizations = 30;

        constexpr std::array<std::uint8_t, 1> kShowAll{ 0x01 };

        using DefaultFn      = void(__thiscall*)(void*);
        using CreatePresetFn = FECarRecord::Record*(__thiscall*)(void*, const char*);
        using PresetCountFn  = int(__cdecl*)();
        using PresetAtFn     = std::byte*(__cdecl*)(int);
        using HashFn         = std::uint32_t(__cdecl*)(const char*);

        std::uintptr_t                   g_original     = 0;
        std::uintptr_t                   g_createPreset = 0;
        std::uintptr_t                   g_presetCount  = 0;
        std::uintptr_t                   g_presetAt     = 0;
        std::uintptr_t                   g_hash         = 0;
        std::vector<FECarRecord::Record> g_vehicles;
        std::optional<ScopedPatch>       g_showAllCars;
        ScopedHook                       g_detour;

        bool IsStock(const FECarRecord::Record& record) noexcept {
            return record.handle != FECarRecord::kFreeHandle && record.customization == FECarRecord::kNoRecord;
        }

        bool IsPreset(const FECarRecord::Record& record) noexcept {
            return record.handle != FECarRecord::kFreeHandle && record.customization != FECarRecord::kNoRecord &&
                   (record.filter & (FECarRecord::kPresetList | FECarRecord::kAiPresetList)) != 0;
        }

        void LearnVehicles() {
            if (!g_vehicles.empty()) return;

            const auto scratch = std::make_unique<std::byte[]>(FECarRecord::kDatabaseSize);
            reinterpret_cast<DefaultFn>(g_original)(scratch.get());

            for (const FECarRecord::Record& record : FECarRecord::Records(scratch.get())) {
                if (IsStock(record)) g_vehicles.push_back(record);
            }
        }

        void RevealHidden(void* database) {
            for (FECarRecord::Record& record : FECarRecord::Records(database)) {
                if (IsStock(record) && (record.filter & FECarRecord::kListMask) == FECarRecord::kHiddenList) {
                    record.filter |= FECarRecord::kStockList;
                } else if (IsPreset(record)) {
                    record.filter |= FECarRecord::kStockList | FECarRecord::kPresetList;
                }
            }
        }

        void AddMissingVehicles(void* database) {
            const auto records = FECarRecord::Records(database);

            for (const FECarRecord::Record& vehicle : g_vehicles) {
                const bool owned = std::ranges::any_of(records, [&](const FECarRecord::Record& record) {
                    return IsStock(record) && record.vehicle == vehicle.vehicle;
                });
                if (owned || FECarRecord::FreeRecords(database) <= kSpareRecords) continue;

                const auto slot = std::ranges::find(records, FECarRecord::kFreeHandle, &FECarRecord::Record::handle);
                *slot        = vehicle;
                slot->handle = static_cast<std::uint32_t>(slot - records.begin());
                slot->filter |= FECarRecord::kStockList;
            }
        }

        void AddMissingPresets(void* database) {
            const int count = reinterpret_cast<PresetCountFn>(g_presetCount)();

            for (int index = 0; index < count; ++index) {
                if (FECarRecord::FreeRecords(database) <= kSpareRecords ||
                    FECarRecord::FreeCustomizations(database) <= kSpareCustomizations) {
                    return;
                }

                std::byte* const preset = reinterpret_cast<PresetAtFn>(g_presetAt)(index);
                if (preset == nullptr) continue;

                const auto* const name = reinterpret_cast<const char*>(preset + kPresetName);
                if (FECarRecord::Find(database, reinterpret_cast<HashFn>(g_hash)(name)) != nullptr) continue;

                if (FECarRecord::Record* const record = reinterpret_cast<CreatePresetFn>(g_createPreset)(database, name)) {
                    record->filter |= FECarRecord::kStockList | FECarRecord::kPresetList;
                }
            }
        }

        void __fastcall Default(void* database, void*) {
            reinterpret_cast<DefaultFn>(g_original)(database);
            Memory::Guarded([&] { AddEveryVehicle(database); });
        }

    }

    bool InstallEveryVehicle() noexcept {
        const auto presets = Scan::Find(kPresetCars);
        const auto flag    = Hook::Operand(kShowAllCars, kShowAllCarsFlag);
        if (!presets || !flag || *flag == 0) return false;

        const auto createPreset = Scan::Relative(*presets + kCreatePresetCall);
        const auto presetCount  = Scan::Relative(*presets + kPresetCountCall);
        const auto presetAt     = Scan::Relative(*presets + kPresetAtCall);
        const auto hash         = Scan::Relative(*presets + kHashCall);
        for (const auto& function : { createPreset, presetCount, presetAt, hash }) {
            if (!function || !Memory::IsExecutable(reinterpret_cast<const void*>(*function))) return false;
        }

        g_createPreset = *createPreset;
        g_presetCount  = *presetCount;
        g_presetAt     = *presetAt;
        g_hash         = *hash;

        if (!Hook::Detour(g_detour, kDefault, kStolenBytes, reinterpret_cast<const void*>(&Default), g_original)) {
            return false;
        }

        g_showAllCars.emplace(*flag, kShowAll);
        if (*g_showAllCars) return true;

        g_showAllCars.reset();
        g_detour.Reset();
        g_original = 0;
        return false;
    }

    void AddEveryVehicle(void* database) {
        if (database == nullptr || g_original == 0) return;

        LearnVehicles();
        RevealHidden(database);
        AddMissingVehicles(database);
        AddMissingPresets(database);
    }

}
