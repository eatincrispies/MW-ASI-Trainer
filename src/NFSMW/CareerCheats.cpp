#include "../dllmain.hpp"

#include <algorithm>
#include <climits>
#include <cstring>
#include <vector>

namespace {

    namespace CareerSettings {

        constexpr Memory::Pattern kSpendCash =
            Memory::ParsePattern("8B 41 0C 8B 54 24 04 3B D0 76 0A C7 41 0C 00 00 00 00 C2 04 00 "
                                 "2B C2 89 41 0C C2 04 00");

        constexpr std::array<std::uint8_t, 3> kReturn{ 0xC2, 0x04, 0x00 };

        std::optional<ScopedPatch> g_SpendCash;

    }

    namespace cFrontendDatabase {

        constexpr Memory::Pattern kFEDatabaseRead =
            Memory::ParsePattern("8B 0D ?? ?? ?? ?? 8B 41 10 8B 0C 96 8B 90 B4 00 00 00 05 A8 00 00 00 "
                                 "03 D1 89 50 0C");

        constexpr std::ptrdiff_t kFEDatabaseOperand   = 2;
        constexpr std::ptrdiff_t kCurrentUserProfiles = 0x10;
        constexpr int            kMaxUserProfiles     = 2;
        constexpr std::ptrdiff_t kTheCareerSettings   = 0xA8;
        constexpr std::ptrdiff_t kCurrentCash         = kTheCareerSettings + 0x0C;
        constexpr std::ptrdiff_t kPlayersCarStable    = 0x414;
        constexpr std::uint32_t  kFullWallet          = 9'999'999;

        std::uintptr_t g_FEDatabase   = 0;
        bool           g_infiniteCash = false;

        bool Resolve() noexcept {
            if (g_FEDatabase != 0) return true;

            const auto database = Hook::Operand(kFEDatabaseRead, kFEDatabaseOperand);
            if (!database || *database == 0) return false;

            g_FEDatabase = *database;
            return true;
        }

        std::optional<std::uintptr_t> GetUserProfile(int player) noexcept {
            if (g_FEDatabase == 0 || player < 0 || player >= kMaxUserProfiles) return std::nullopt;

            const auto database = Memory::Read<std::uintptr_t>(g_FEDatabase);
            if (!database || *database == 0) return std::nullopt;

            const auto profile =
                Memory::Read<std::uintptr_t>(*database + kCurrentUserProfiles + player * sizeof(std::uintptr_t));
            if (!profile || *profile == 0) return std::nullopt;
            return *profile;
        }

        void* GetPlayerCarStable(int player) noexcept {
            const auto profile = GetUserProfile(player);
            return profile ? reinterpret_cast<void*>(*profile + kPlayersCarStable) : nullptr;
        }

        bool EnableInfiniteCash() noexcept {
            if (!Resolve()) return false;

            g_infiniteCash = true;
            return true;
        }

        void RefillCash() noexcept {
            if (!g_infiniteCash) return;

            const auto profile = GetUserProfile(0);
            if (!profile) return;

            const auto cash = Memory::Read<std::uint32_t>(*profile + kCurrentCash);
            if (!cash || *cash >= kFullWallet) return;

            Memory::Guarded([&] { *reinterpret_cast<std::uint32_t*>(*profile + kCurrentCash) = kFullWallet; });
        }

    }

    namespace FEMarkerManager {

        constexpr Memory::Pattern kGetNumMarkers =
            Memory::ParsePattern("8B 54 24 08 53 56 8B 74 24 0C 57 33 C0 83 C1 08 BF 15 00 00 00 BB 01 00 00 00");

        constexpr Memory::Pattern kAddMarkerToInventoryCall =
            Memory::ParsePattern("8B 04 96 50 53 B9 ?? ?? ?? ?? E8 ?? ?? ?? ?? EB 36");

        constexpr std::ptrdiff_t kTheFEMarkerManagerOperand = 6;
        constexpr std::ptrdiff_t kAddMarkerToInventoryCallOperand = 11;
        constexpr int            kFirstPerformanceMarker = 1;
        constexpr int            kLastPerformanceMarker  = 7;
        constexpr int            kAnyCar                 = 0;

        using GetNumMarkersFn        = int(__thiscall*)(void*, int, int);
        using AddMarkerToInventoryFn = void(__thiscall*)(void*, int, int);

        std::uintptr_t g_TheFEMarkerManager   = 0;
        std::uintptr_t g_GetNumMarkers        = 0;
        std::uintptr_t g_AddMarkerToInventory = 0;
        bool           g_endlessJunkman       = false;

        bool EnableInfiniteJunkmanParts() noexcept {
            if (g_AddMarkerToInventory == 0) {
                const auto getNumMarkers = Scan::Find(kGetNumMarkers);
                const auto manager       = Hook::Operand(kAddMarkerToInventoryCall, kTheFEMarkerManagerOperand);
                const auto add           = Hook::Operand(kAddMarkerToInventoryCall, kAddMarkerToInventoryCallOperand, true);
                if (!getNumMarkers || !manager || *manager == 0 || !add) return false;
                if (!Memory::IsExecutable(reinterpret_cast<const void*>(*add))) return false;

                g_GetNumMarkers        = *getNumMarkers;
                g_TheFEMarkerManager   = *manager;
                g_AddMarkerToInventory = *add;
            }

            g_endlessJunkman = true;
            return true;
        }

        void RefillPerformanceMarkers() noexcept {
            if (!g_endlessJunkman) return;

            Memory::Guarded([] {
                auto* const manager = reinterpret_cast<void*>(g_TheFEMarkerManager);
                for (int type = kFirstPerformanceMarker; type <= kLastPerformanceMarker; ++type) {
                    if (reinterpret_cast<GetNumMarkersFn>(g_GetNumMarkers)(manager, type, kAnyCar) > 0) continue;
                    reinterpret_cast<AddMarkerToInventoryFn>(g_AddMarkerToInventory)(manager, type, kAnyCar);
                }
            });
        }

    }

    namespace FEngine {

        constexpr Memory::Pattern kUpdate =
            Memory::ParsePattern("83 EC 0C 56 8B F1 8A 86 4E 52 00 00 84 C0 74 0B 8B 8E 08 01 00 00 "
                                 "8B 01 FF 50 5C 80 3E 00");

        constexpr std::size_t kUpdateStolen = 6;

        using UpdateFn = void(__thiscall*)(void*, std::uint32_t, std::uint32_t);

        std::uintptr_t g_Update = 0;
        ScopedHook     g_UpdateHook;

        void __fastcall UpdateHook(void* engine, void*, std::uint32_t first, std::uint32_t second) {
            reinterpret_cast<UpdateFn>(g_Update)(engine, first, second);
            cFrontendDatabase::RefillCash();
            FEMarkerManager::RefillPerformanceMarkers();
        }

        bool Install() noexcept {
            return Hook::Detour(g_UpdateHook, kUpdate, kUpdateStolen, reinterpret_cast<const void*>(&UpdateHook), g_Update);
        }

    }

    namespace GRaceParameters {

        constexpr Memory::Pattern kGetCashValue =
            Memory::ParsePattern("56 8B F1 8B 46 04 85 C0 74 0A 8D 48 22 E8 ?? ?? ?? ?? 5E C3 "
                                 "8B 46 0C 85 C0 74 11");

        constexpr std::size_t kGetCashValueStolen = 6;

        using GetCashValueFn = float(__thiscall*)(void*);
        using GetTimeLimitFn = float(__thiscall*)(void*);

        std::uintptr_t g_GetCashValue = 0;
        std::uintptr_t g_GetTimeLimit = 0;
        float          g_cashScale    = 1.0f;
        ScopedHook     g_GetCashValueHook;

        float __fastcall GetCashValueHook(void* parameters, void*) {
            return reinterpret_cast<GetCashValueFn>(g_GetCashValue)(parameters) * g_cashScale;
        }

        bool InstallCashMultiplier(float scale) noexcept {
            g_cashScale = scale;
            return Hook::Detour(g_GetCashValueHook, kGetCashValue, kGetCashValueStolen,
                                reinterpret_cast<const void*>(&GetCashValueHook), g_GetCashValue);
        }

        float GetTimeLimit(void* parameters) {
            return reinterpret_cast<GetTimeLimitFn>(g_GetTimeLimit)(parameters);
        }

    }

    namespace UnlockSystem {

        constexpr Memory::Pattern kIsCarUnlocked =
            Memory::ParsePattern("51 A0 ?? ?? ?? ?? 84 C0 74 04 B0 01 59 C3 53 8B 5C 24 0C F6 C3 01 "
                                 "56 8B 74 24 14 C6 44 24 0B 00 74 13 8B 44 24 18 50 56 53 E8");

        constexpr Memory::Pattern kIsPerfPackageUnlocked =
            Memory::ParsePattern("51 A0 ?? ?? ?? ?? 84 C0 74 04 B0 01 59 C3 53 8B 5C 24 0C F6 C3 01 "
                                 "55 8B 6C 24 20 56 8B 74 24 1C 57 8B 7C 24 1C C6 44 24 13 00 74 15 "
                                 "8B 44 24 24 55 50 56 57 53 E8 E6 F6 FF FF");

        constexpr Memory::Pattern kIsCarPartUnlocked =
            Memory::ParsePattern("51 A0 ?? ?? ?? ?? 84 C0 74 04 B0 01 59 C3 53 8B 5C 24 0C F6 C3 01 "
                                 "55 8B 6C 24 20 56 8B 74 24 1C 57 8B 7C 24 1C C6 44 24 13 00 74 15 "
                                 "8B 44 24 24 55 50 56 57 53 E8 16 F7 FF FF");

        constexpr Memory::Pattern kIsTrackUnlocked =
            Memory::ParsePattern("51 A0 ?? ?? ?? ?? 84 C0 74 04 B0 01 59 C3 8B 44 24 08 53 32 DB");

        constexpr Memory::Pattern kGetPerfPackageCost =
            Memory::ParsePattern("51 8B 44 24 0C 50 E8 ?? ?? ?? ?? 83 C4 04 85 C0 C7 04 24 00 00 00 00");

        constexpr Memory::Pattern kGetCarPartCost =
            Memory::ParsePattern("51 8B 4C 24 10 8B 44 24 0C 51 50 E8 ?? ?? ?? ?? 83 C4 08 85 C0 C7 04 24 00 00 00 00 "
                                 "74 1D 8A 49 05 C0 E9 05 0F BE D1 52 50 E8 ?? ?? ?? ?? 83 C4 08 85 C0 74 06 8B 40 08");

        constexpr std::array<std::uint8_t, 3> kReturnTrue{ 0xB0, 0x01, 0xC3 };
        constexpr std::array<std::uint8_t, 3> kReturnZero{ 0x33, 0xC0, 0xC3 };

        std::optional<ScopedPatch> g_IsCarUnlocked;
        std::optional<ScopedPatch> g_IsPerfPackageUnlocked;
        std::optional<ScopedPatch> g_IsCarPartUnlocked;
        std::optional<ScopedPatch> g_IsTrackUnlocked;
        std::optional<ScopedPatch> g_GetPerfPackageCost;
        std::optional<ScopedPatch> g_GetCarPartCost;

        bool InstallUnlockAllCars() noexcept {
            return Hook::Patch(g_IsCarUnlocked, kIsCarUnlocked, 0, kReturnTrue);
        }

        bool InstallUnlockAllPerformanceParts() noexcept {
            return Hook::Patch(g_IsPerfPackageUnlocked, kIsPerfPackageUnlocked, 0, kReturnTrue);
        }

        bool InstallUnlockAllVisualParts() noexcept {
            return Hook::Patch(g_IsCarPartUnlocked, kIsCarPartUnlocked, 0, kReturnTrue);
        }

        bool InstallUnlockAllChallengeSeries() noexcept {
            return Hook::Patch(g_IsTrackUnlocked, kIsTrackUnlocked, 0, kReturnTrue);
        }

        bool InstallFreeUpgrades() noexcept {
            if (Hook::Patch(g_GetPerfPackageCost, kGetPerfPackageCost, 0, kReturnZero) &&
                Hook::Patch(g_GetCarPartCost, kGetCarPartCost, 0, kReturnZero)) {
                return true;
            }

            g_GetCarPartCost.reset();
            g_GetPerfPackageCost.reset();
            return false;
        }

    }

    namespace FECarRecord {

        struct Record {
            std::uint32_t Handle;
            std::uint32_t FEKey;
            std::uint32_t VehicleKey;
            std::uint32_t FilterBits;
            std::uint8_t  Customization;
            std::uint8_t  CareerHandle;
            std::uint16_t Padd;
        };

        static_assert(sizeof(Record) == 0x14);

        constexpr std::size_t    kFEPlayerCarDBSize    = 0x8CC8;
        constexpr std::size_t    kCarRecordSlots       = 200;
        constexpr std::ptrdiff_t kCustomizations       = 0xFA0;
        constexpr std::size_t    kCustomizationSlots   = 75;
        constexpr std::size_t    kCustomizationSize    = 0x198;
        constexpr std::size_t    kCustomizationHandle  = 0x194;
        constexpr std::uint32_t  kInvalidHandle        = 0xFFFFFFFF;
        constexpr std::uint8_t   kNoCustomization      = 0xFF;
        constexpr std::uint32_t  kStockList            = 0x01;
        constexpr std::uint32_t  kBonusList            = 0x08;
        constexpr std::uint32_t  kPresetList           = 0x10;
        constexpr std::uint32_t  kDebugList            = 0x20;
        constexpr std::uint32_t  kListBits             = 0xFFFF;

        std::span<Record> Records(void* database) noexcept {
            return { static_cast<Record*>(database), kCarRecordSlots };
        }

        Record* GetCarRecordByHandle(void* database, std::uint32_t handle) noexcept {
            if (handle == kInvalidHandle) return nullptr;

            const auto records = Records(database);
            const auto match   = std::ranges::find(records, handle, &Record::Handle);
            return match == records.end() ? nullptr : &*match;
        }

        std::byte* GetCustomizationRecordByHandle(void* database, std::uint8_t handle) noexcept {
            if (handle >= kCustomizationSlots) return nullptr;
            return static_cast<std::byte*>(database) + kCustomizations + static_cast<std::size_t>(handle) * kCustomizationSize;
        }

        int GetNumFreeCarRecords(void* database) noexcept {
            return static_cast<int>(std::ranges::count(Records(database), kInvalidHandle, &Record::Handle));
        }

        int GetNumFreeCustomizations(void* database) noexcept {
            int free = 0;
            for (std::size_t slot = 0; slot < kCustomizationSlots; ++slot) {
                const std::byte* const customization = GetCustomizationRecordByHandle(database, static_cast<std::uint8_t>(slot));
                if (std::to_integer<std::uint8_t>(customization[kCustomizationHandle]) == kNoCustomization) ++free;
            }
            return free;
        }

        void CopyCustomizations(void* database, std::uint8_t from, std::uint8_t to) noexcept {
            std::byte* const source      = GetCustomizationRecordByHandle(database, from);
            std::byte* const destination = GetCustomizationRecordByHandle(database, to);
            if (source == nullptr || destination == nullptr || source == destination) return;
            std::memcpy(destination, source, kCustomizationHandle);
        }

        bool IsStockCar(const Record& record) noexcept {
            return record.Handle != kInvalidHandle && record.Customization == kNoCustomization;
        }

        bool IsPresetCar(const Record& record) noexcept {
            return record.Handle != kInvalidHandle && record.Customization != kNoCustomization &&
                   (record.FilterBits & (kBonusList | kPresetList)) != 0;
        }

        constexpr Memory::Pattern kGetReleaseFromImpoundCost =
            Memory::ParsePattern("51 E8 ?? ?? ?? ?? 85 C0 89 04 24 DB 04 24 7D 06 D8 05 ?? ?? ?? ?? "
                                 "D9 05 ?? ?? ?? ?? D8 C9 E8");

        constexpr std::array<std::uint8_t, 3> kReturnZero{ 0x33, 0xC0, 0xC3 };

        std::optional<ScopedPatch> g_GetReleaseFromImpoundCost;

        bool InstallFreeImpoundBail() noexcept {
            return Hook::Patch(g_GetReleaseFromImpoundCost, kGetReleaseFromImpoundCost, 0, kReturnZero);
        }

    }

    namespace FEPlayerCarDB {

        constexpr Memory::Pattern kDefault =
            Memory::ParsePattern("6A FF 64 A1 00 00 00 00 68 ?? ?? ?? ?? 50 64 89 25 00 00 00 00 83 EC 2C 53 55 "
                                 "8B D9 56 57 8B C3 B9 C8 00 00 00 C7 00 FF FF FF FF 83 C0 14 49 75 F4 "
                                 "8D 83 34 11 00 00 B9 4B 00 00 00");

        constexpr Memory::Pattern kDefaultPresetCars =
            Memory::ParsePattern("33 FF E8 ?? ?? ?? ?? 85 C0 0F 8E FC 00 00 00 BD 08 00 0F 00 8D 64 24 00 "
                                 "57 E8 ?? ?? ?? ?? 8D 70 28 56 E8 ?? ?? ?? ?? 83 C4 08");

        constexpr Memory::Pattern kCreateNewPresetCar =
            Memory::ParsePattern("6A FF 68 ?? ?? ?? ?? 64 A1 00 00 00 00 50 64 89 25 00 00 00 00 83 EC 3C "
                                 "8B 44 24 4C");

        constexpr Memory::Pattern kShowAllCarsInFERead =
            Memory::ParsePattern("80 38 00 75 09 A0 ?? ?? ?? ?? 84 C0 74 66 85 F6 74 62");

        constexpr Memory::Pattern kCreateCar =
            Memory::ParsePattern("8B 54 24 04 81 EC 10 03 00 00 53 55 56 8B E9 33 C0 39 11 74 19");

        constexpr std::size_t    kCreateCarStolen        = 10;
        constexpr std::ptrdiff_t kGetNumPresetCarsCall   = 0x03;
        constexpr std::ptrdiff_t kGetPresetCarAtCall     = 0x1A;
        constexpr std::ptrdiff_t kFEHashUpperCall        = 0x23;
        constexpr std::ptrdiff_t kShowAllCarsInFEOperand = 6;
        constexpr std::ptrdiff_t kPresetCarName          = 0x28;
        constexpr int            kSpareCarRecords        = 40;
        constexpr int            kSpareCustomizations    = 30;

        using DefaultFn            = void(__thiscall*)(void*);
        using CreateNewPresetCarFn = FECarRecord::Record*(__thiscall*)(void*, const char*);
        using CreateCarFn          = FECarRecord::Record*(__thiscall*)(void*, std::uint32_t, std::uint32_t);
        using GetNumPresetCarsFn   = int(__cdecl*)();
        using GetPresetCarAtFn     = std::byte*(__cdecl*)(int);
        using FEHashUpperFn        = std::uint32_t(__cdecl*)(const char*);

        std::uintptr_t                   g_Default            = 0;
        std::uintptr_t                   g_CreateNewPresetCar = 0;
        std::uintptr_t                   g_GetNumPresetCars   = 0;
        std::uintptr_t                   g_GetPresetCarAt     = 0;
        std::uintptr_t                   g_FEHashUpper        = 0;
        std::uintptr_t                   g_CreateCar          = 0;
        bool*                            g_ShowAllCarsInFE    = nullptr;
        std::vector<FECarRecord::Record> g_stockCars;
        ScopedHook                       g_CreateCarHook;

        void LearnStockCars() {
            if (!g_stockCars.empty()) return;

            const auto scratch = std::make_unique<std::byte[]>(FECarRecord::kFEPlayerCarDBSize);
            const bool shown   = *g_ShowAllCarsInFE;
            *g_ShowAllCarsInFE = true;
            const bool built   = Memory::Guarded([&] { reinterpret_cast<DefaultFn>(g_Default)(scratch.get()); });
            *g_ShowAllCarsInFE = shown;
            if (!built) return;

            for (const FECarRecord::Record& record : FECarRecord::Records(scratch.get())) {
                if (FECarRecord::IsStockCar(record)) g_stockCars.push_back(record);
            }
        }

        void ShowHiddenCars(void* database) {
            for (FECarRecord::Record& record : FECarRecord::Records(database)) {
                if (FECarRecord::IsStockCar(record) &&
                    (record.FilterBits & FECarRecord::kListBits) == FECarRecord::kDebugList) {
                    record.FilterBits |= FECarRecord::kStockList;
                } else if (FECarRecord::IsPresetCar(record)) {
                    record.FilterBits |= FECarRecord::kStockList;
                }
            }
        }

        void AddMissingStockCars(void* database) {
            const auto records = FECarRecord::Records(database);

            for (const FECarRecord::Record& stock : g_stockCars) {
                const bool owned = std::ranges::any_of(records, [&](const FECarRecord::Record& record) {
                    return FECarRecord::IsStockCar(record) && record.VehicleKey == stock.VehicleKey;
                });
                if (owned) continue;
                if (FECarRecord::GetNumFreeCarRecords(database) <= kSpareCarRecords) return;

                const auto slot = std::ranges::find(records, FECarRecord::kInvalidHandle, &FECarRecord::Record::Handle);
                *slot             = stock;
                slot->Handle      = static_cast<std::uint32_t>(slot - records.begin());
                slot->FilterBits |= FECarRecord::kStockList;
            }
        }

        void AddMissingPresetCars(void* database) {
            const int count = reinterpret_cast<GetNumPresetCarsFn>(g_GetNumPresetCars)();

            for (int index = 0; index < count; ++index) {
                if (FECarRecord::GetNumFreeCarRecords(database) <= kSpareCarRecords ||
                    FECarRecord::GetNumFreeCustomizations(database) <= kSpareCustomizations) {
                    return;
                }

                std::byte* const preset = reinterpret_cast<GetPresetCarAtFn>(g_GetPresetCarAt)(index);
                if (preset == nullptr) continue;

                const auto* const name = reinterpret_cast<const char*>(preset + kPresetCarName);
                if (FECarRecord::GetCarRecordByHandle(database, reinterpret_cast<FEHashUpperFn>(g_FEHashUpper)(name)) != nullptr) {
                    continue;
                }

                if (FECarRecord::Record* const record =
                        reinterpret_cast<CreateNewPresetCarFn>(g_CreateNewPresetCar)(database, name)) {
                    record->FilterBits |= FECarRecord::kStockList;
                }
            }
        }

        void AddEveryVehicle(void* database) {
            if (database == nullptr) return;

            LearnStockCars();
            ShowHiddenCars(database);
            AddMissingStockCars(database);
            AddMissingPresetCars(database);
        }

        void CopyPresetCustomizations(void* database, std::uint32_t handle, const FECarRecord::Record& car) {
            const FECarRecord::Record* const source = FECarRecord::GetCarRecordByHandle(database, handle);
            if (source == nullptr || source == &car || !FECarRecord::IsPresetCar(*source)) return;
            if ((source->FilterBits & FECarRecord::kStockList) == 0 || car.Customization == FECarRecord::kNoCustomization) {
                return;
            }

            FECarRecord::CopyCustomizations(database, source->Customization, car.Customization);
        }

        FECarRecord::Record* __fastcall CreateCarHook(void* database, void*, std::uint32_t handle, std::uint32_t filterBits) {
            FECarRecord::Record* const car = reinterpret_cast<CreateCarFn>(g_CreateCar)(database, handle, filterBits);
            if (car != nullptr) Memory::Guarded([&] { CopyPresetCustomizations(database, handle, *car); });
            return car;
        }

        bool InstallEveryVehicle() noexcept {
            const auto defaults = Scan::Find(kDefault);
            const auto presets  = Scan::Find(kDefaultPresetCars);
            const auto create   = Scan::Find(kCreateNewPresetCar);
            const auto flag     = Hook::Operand(kShowAllCarsInFERead, kShowAllCarsInFEOperand);
            if (!defaults || !presets || !create || !flag || *flag == 0) return false;

            const auto presetCount = Scan::Relative(*presets + kGetNumPresetCarsCall);
            const auto presetAt    = Scan::Relative(*presets + kGetPresetCarAtCall);
            const auto hash        = Scan::Relative(*presets + kFEHashUpperCall);
            for (const auto& function : { presetCount, presetAt, hash }) {
                if (!function || !Memory::IsExecutable(reinterpret_cast<const void*>(*function))) return false;
            }

            g_Default            = *defaults;
            g_CreateNewPresetCar = *create;
            g_GetNumPresetCars   = *presetCount;
            g_GetPresetCarAt     = *presetAt;
            g_FEHashUpper        = *hash;
            g_ShowAllCarsInFE    = reinterpret_cast<bool*>(*flag);
            return true;
        }

        bool InstallPresetCustomizations() noexcept {
            return Hook::Detour(g_CreateCarHook, kCreateCar, kCreateCarStolen, reinterpret_cast<const void*>(&CreateCarHook),
                                g_CreateCar);
        }

    }

    namespace UIQRCarSelect {

        constexpr Memory::Pattern kRefreshCarList =
            Memory::ParsePattern("83 EC 0C 53 8B D9 8B 43 2C 57 8D 7B 2C 3B C7 89 5C 24 10 74 19");

        constexpr std::size_t    kRefreshCarListStolen = 6;
        constexpr std::ptrdiff_t kiPlayerNum           = 0x1A8;

        using RefreshCarListFn = void(__thiscall*)(void*);

        std::uintptr_t g_RefreshCarList = 0;
        ScopedHook     g_RefreshCarListHook;

        void __fastcall RefreshCarListHook(void* screen, void*) {
            Memory::Guarded([&] {
                FEPlayerCarDB::AddEveryVehicle(cFrontendDatabase::GetPlayerCarStable(Game::Field<int>(screen, kiPlayerNum)));
            });
            reinterpret_cast<RefreshCarListFn>(g_RefreshCarList)(screen);
        }

        bool InstallEveryVehicle() noexcept {
            return cFrontendDatabase::Resolve() &&
                   Hook::Detour(g_RefreshCarListHook, kRefreshCarList, kRefreshCarListStolen,
                                reinterpret_cast<const void*>(&RefreshCarListHook), g_RefreshCarList);
        }

    }

    namespace Blacklist {

        constexpr Memory::Pattern kBin_GetNumRacesWon =
            Memory::ParsePattern("8B 44 24 04 8B 0D ?? ?? ?? ?? 50 E8 ?? ?? ?? ?? 85 C0 74 07 8B C8 E9 85 87 FD FF "
                                 "33 C0 C3");

        constexpr Memory::Pattern kBin_GetNumChallengesPassed =
            Memory::ParsePattern("8B 44 24 04 8B 0D ?? ?? ?? ?? 50 E8 ?? ?? ?? ?? 85 C0 74 07 8B C8 E9 95 87 FD FF "
                                 "33 C0 C3");

        constexpr Memory::Pattern kGame_GetPlayerBounty =
            Memory::ParsePattern("8B 0D ?? ?? ?? ?? 85 C9 56 74 5A A1 ?? ?? ?? ?? 8B 10 85 D2 74 4F "
                                 "85 C9 74 04 8B CA EB 02 33 C9 8B 01 FF 50 04 8B F0 85 F6 74 3A "
                                 "8B 16 8B CE FF 52 24 84 C0 74 2F");

        constexpr std::array<std::uint8_t, 6> kReturnThousand{ 0xB8, 0xE8, 0x03, 0x00, 0x00, 0xC3 };
        constexpr std::array<std::uint8_t, 6> kReturnHundredMillion{ 0xB8, 0x00, 0xE1, 0xF5, 0x05, 0xC3 };

        std::optional<ScopedPatch> g_Bin_GetNumRacesWon;
        std::optional<ScopedPatch> g_Bin_GetNumChallengesPassed;
        std::optional<ScopedPatch> g_Game_GetPlayerBounty;

        bool InstallUnlockAllBlacklist() noexcept {
            if (Hook::Patch(g_Bin_GetNumRacesWon, kBin_GetNumRacesWon, 0, kReturnThousand) &&
                Hook::Patch(g_Bin_GetNumChallengesPassed, kBin_GetNumChallengesPassed, 0, kReturnThousand) &&
                Hook::Patch(g_Game_GetPlayerBounty, kGame_GetPlayerBounty, 0, kReturnHundredMillion)) {
                return true;
            }

            g_Game_GetPlayerBounty.reset();
            g_Bin_GetNumChallengesPassed.reset();
            g_Bin_GetNumRacesWon.reset();
            return false;
        }

    }

    namespace FEMarkerSelection {

        constexpr Memory::Pattern kNotificationMessage =
            Memory::ParsePattern("8B 44 24 04 3D 12 89 C0 AB 56 8B F1 0F 87 ?? ?? ?? ?? "
                                 "0F 84 ?? ?? ?? ?? 3D 10 72 40 0C");

        constexpr Memory::Pattern kGetSelectedButtonIndexCall =
            Memory::ParsePattern("8B CE E8 ?? ?? ?? ?? 8B F8 8D 04 7F 8D 1C 86 8A 43 7C 84 C0");

        constexpr Memory::Pattern kRedrawCall =
            Memory::ParsePattern("8B CE E8 ?? ?? ?? ?? 5E C2 10 00 3D 3D 31 3E BB");

        constexpr std::size_t    kNotificationMessageStolen = 9;
        constexpr std::ptrdiff_t kCallOperand               = 3;
        constexpr std::uint32_t  kPickCardMessage           = 0x0C407210;
        constexpr std::ptrdiff_t kNumCards                  = 0x2C;
        constexpr std::ptrdiff_t kCards                     = 0x74;
        constexpr std::ptrdiff_t kCardSize                  = 0x0C;
        constexpr std::ptrdiff_t kCardMarker                = 0x00;
        constexpr std::ptrdiff_t kCardParam                 = 0x04;
        constexpr std::ptrdiff_t kCardPicked                = 0x08;
        constexpr int            kMaxCards                  = 6;
        constexpr int            kMaxPicks                  = 2;
        constexpr int            kMarkerPinkSlip            = 0x12;

        using NotificationMessageFn    = void(__thiscall*)(void*, std::uint32_t, void*, std::uint32_t, std::uint32_t);
        using GetSelectedButtonIndexFn = int(__thiscall*)(void*);
        using RedrawFn                 = void(__thiscall*)(void*);

        std::uintptr_t g_NotificationMessage    = 0;
        std::uintptr_t g_GetSelectedButtonIndex = 0;
        std::uintptr_t g_Redraw                 = 0;
        ScopedHook     g_NotificationMessageHook;

        std::byte* Card(std::byte* screen, int index) {
            return screen + kCards + index * kCardSize;
        }

        bool IsPicked(std::byte* card) {
            return Game::Field<std::uint8_t>(card, kCardPicked) != 0;
        }

        bool MovePinkSlipToPick(std::byte* screen) {
            const int count = std::clamp(Game::Field<int>(screen, kNumCards), 0, kMaxCards);

            int picks    = 0;
            int pinkSlip = -1;
            for (int index = 0; index < count; ++index) {
                std::byte* const card = Card(screen, index);
                const int marker = Game::Field<int>(card, kCardMarker);

                if (marker != 0 && IsPicked(card)) ++picks;
                if (marker != kMarkerPinkSlip) continue;
                if (IsPicked(card)) return false;
                pinkSlip = index;
            }
            if (pinkSlip < 0 || picks >= kMaxPicks) return false;

            const int choice = reinterpret_cast<GetSelectedButtonIndexFn>(g_GetSelectedButtonIndex)(screen);
            if (choice < 0 || choice >= count || choice == pinkSlip || IsPicked(Card(screen, choice))) return false;

            std::byte* const chosen = Card(screen, choice);
            std::byte* const slip   = Card(screen, pinkSlip);
            std::swap(Game::Field<int>(chosen, kCardMarker), Game::Field<int>(slip, kCardMarker));
            std::swap(Game::Field<int>(chosen, kCardParam), Game::Field<int>(slip, kCardParam));
            return true;
        }

        void __fastcall NotificationMessageHook(void* screen, void*, std::uint32_t message, void* object,
                                                std::uint32_t first, std::uint32_t second) {
            bool moved = false;
            if (message == kPickCardMessage) {
                Memory::Guarded([&] { moved = MovePinkSlipToPick(static_cast<std::byte*>(screen)); });
            }

            reinterpret_cast<NotificationMessageFn>(g_NotificationMessage)(screen, message, object, first, second);

            if (moved) Memory::Guarded([&] { reinterpret_cast<RedrawFn>(g_Redraw)(screen); });
        }

        bool InstallAlwaysWinPinkSlip() noexcept {
            const auto selected = Hook::Operand(kGetSelectedButtonIndexCall, kCallOperand, true);
            const auto redraw   = Hook::Operand(kRedrawCall, kCallOperand, true);
            if (!selected || !redraw || !Memory::IsExecutable(reinterpret_cast<const void*>(*selected)) ||
                !Memory::IsExecutable(reinterpret_cast<const void*>(*redraw))) {
                return false;
            }

            g_GetSelectedButtonIndex = *selected;
            g_Redraw                 = *redraw;
            return Hook::Detour(g_NotificationMessageHook, kNotificationMessage, kNotificationMessageStolen,
                                reinterpret_cast<const void*>(&NotificationMessageHook), g_NotificationMessage);
        }

    }

    namespace PostPursuitInfractionsScreen {

        constexpr Memory::Pattern kNotificationMessageStrikeCheck =
            Memory::ParsePattern("8B 4E 2C 3B C2 0F 9D C0 88 46 30 E8");

        constexpr std::ptrdiff_t              kImpoundedDecision = 5;
        constexpr std::array<std::uint8_t, 3> kNeverImpounded{ 0x32, 0xC0, 0x90 };

        std::optional<ScopedPatch> g_StrikeCheck;

        bool InstallNeverImpoundCar() noexcept {
            return Hook::Patch(g_StrikeCheck, kNotificationMessageStrikeCheck, kImpoundedDecision, kNeverImpounded);
        }

    }

    namespace GRaceStatus {

        constexpr Memory::Pattern kGetRaceTimeRemaining =
            Memory::ParsePattern("51 56 8B F1 8B 8E 68 19 00 00 85 C9 74 5B E8 ?? ?? ?? ?? D8 86 80 19 00 00 D9 54 24 04");

        constexpr Memory::Pattern kIsTollboothRace =
            Memory::ParsePattern("A1 ?? ?? ?? ?? 85 C0 74 1A 8B 88 68 19 00 00 85 C9 74 10 E8 ?? ?? ?? ?? "
                                 "83 F8 04 75 06 B8 01 00 00 00 C3 33 C0 C3");

        constexpr std::size_t    kGetRaceTimeRemainingStolen = 10;
        constexpr std::ptrdiff_t kGetTimeLimitCall           = 15;
        constexpr std::ptrdiff_t kRaceParameters             = 0x1968;
        constexpr std::ptrdiff_t kBonusTime                  = 0x1980;

        using GetRaceTimeRemainingFn = float(__thiscall*)(void*);
        using IsTollboothRaceFn      = int(__cdecl*)();

        std::uintptr_t g_GetRaceTimeRemaining = 0;
        std::uintptr_t g_IsTollboothRace      = 0;
        ScopedHook     g_GetRaceTimeRemainingHook;

        float __fastcall GetRaceTimeRemainingHook(void* status, void*) {
            float remaining = reinterpret_cast<GetRaceTimeRemainingFn>(g_GetRaceTimeRemaining)(status);

            Memory::Guarded([&] {
                void* const parameters = Game::Field<void*>(status, kRaceParameters);
                if (parameters == nullptr || reinterpret_cast<IsTollboothRaceFn>(g_IsTollboothRace)() == 0) return;
                remaining = GRaceParameters::GetTimeLimit(parameters) + Game::Field<float>(status, kBonusTime);
            });
            return remaining;
        }

        bool InstallInfiniteTollboothTime() noexcept {
            const auto timeLimit = Hook::Operand(kGetRaceTimeRemaining, kGetTimeLimitCall, true);
            const auto tollbooth = Scan::Find(kIsTollboothRace);
            if (!timeLimit || !tollbooth || !Memory::IsExecutable(reinterpret_cast<const void*>(*timeLimit))) return false;

            GRaceParameters::g_GetTimeLimit = *timeLimit;
            g_IsTollboothRace               = *tollbooth;
            return Hook::Detour(g_GetRaceTimeRemainingHook, kGetRaceTimeRemaining, kGetRaceTimeRemainingStolen,
                                reinterpret_cast<const void*>(&GetRaceTimeRemainingHook), g_GetRaceTimeRemaining);
        }

    }

    namespace GameEvents {

        constexpr Memory::Pattern kStartRaceTimers =
            Memory::ParsePattern("8B 0D ?? ?? ?? ?? 56 E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 33 F6");

        constexpr Memory::Pattern kNotifyRaceFinished =
            Memory::ParsePattern("64 A1 00 00 00 00 6A FF 68 ?? ?? ?? ?? 50 64 89 25 00 00 00 00 83 EC 0C 53 55 56 57 "
                                 "8B 7C 24 2C 85 FF 75 1F A1");

        constexpr std::size_t kStartRaceTimersStolen = 6;

        using StartRaceTimersFn    = void(__cdecl*)();
        using NotifyRaceFinishedFn = void(__cdecl*)(ISimable*);

        std::uintptr_t g_StartRaceTimers    = 0;
        std::uintptr_t g_NotifyRaceFinished = 0;
        ScopedHook     g_StartRaceTimersHook;

        void __cdecl StartRaceTimersHook() {
            reinterpret_cast<StartRaceTimersFn>(g_StartRaceTimers)();
            Memory::Guarded([] { reinterpret_cast<NotifyRaceFinishedFn>(g_NotifyRaceFinished)(nullptr); });
        }

        bool InstallInstantWinRace() noexcept {
            const auto finished = Scan::Find(kNotifyRaceFinished);
            if (!finished) return false;

            g_NotifyRaceFinished = *finished;
            return Hook::Detour(g_StartRaceTimersHook, kStartRaceTimers, kStartRaceTimersStolen,
                                reinterpret_cast<const void*>(&StartRaceTimersHook), g_StartRaceTimers);
        }

    }

    bool InstallInfiniteCash() noexcept {
        if (!Hook::Patch(CareerSettings::g_SpendCash, CareerSettings::kSpendCash, 0, CareerSettings::kReturn)) return false;
        if (cFrontendDatabase::EnableInfiniteCash() && FEngine::Install()) return true;

        CareerSettings::g_SpendCash.reset();
        return false;
    }

    bool InstallInfiniteJunkmanParts() noexcept {
        return FEMarkerManager::EnableInfiniteJunkmanParts() && FEngine::Install();
    }

    bool InstallEveryVehicle() noexcept {
        if (!FEPlayerCarDB::InstallEveryVehicle() || !FEPlayerCarDB::InstallPresetCustomizations()) return false;
        if (UIQRCarSelect::InstallEveryVehicle()) return true;

        FEPlayerCarDB::g_CreateCarHook.Reset();
        FEPlayerCarDB::g_CreateCar = 0;
        return false;
    }

}

void ApplyCareer(const IniSection& career, Installer& installer) {
    installer.Toggle(career, "InfiniteCash", InstallInfiniteCash);
    if (const float cash = career.Float("CashMultiplier", 1.0f, 0.0f, 1000.0f); cash != 1.0f) {
        installer.Check("CashMultiplier", GRaceParameters::InstallCashMultiplier(cash));
    }
    if (career.Bool("UnlockAllCars")) {
        installer.Check("UnlockAllCars", UnlockSystem::InstallUnlockAllCars());
        installer.Check("UnlockAllCars extra cars", InstallEveryVehicle());
    }
    installer.Toggle(career, "UnlockAllPerformanceParts", UnlockSystem::InstallUnlockAllPerformanceParts);
    installer.Toggle(career, "UnlockAllVisualParts", UnlockSystem::InstallUnlockAllVisualParts);
    installer.Toggle(career, "InfiniteJunkmanParts", InstallInfiniteJunkmanParts);
    installer.Toggle(career, "UnlockAllBlacklist", Blacklist::InstallUnlockAllBlacklist);
    installer.Toggle(career, "AlwaysWinPinkSlip", FEMarkerSelection::InstallAlwaysWinPinkSlip);
    installer.Toggle(career, "FreeImpoundBail", FECarRecord::InstallFreeImpoundBail);
    installer.Toggle(career, "NeverImpoundCar", PostPursuitInfractionsScreen::InstallNeverImpoundCar);
    installer.Toggle(career, "InfiniteTollboothTime", GRaceStatus::InstallInfiniteTollboothTime);
    installer.Toggle(career, "FreeUpgrades", UnlockSystem::InstallFreeUpgrades);
    installer.Toggle(career, "UnlockAllChallengeSeries", UnlockSystem::InstallUnlockAllChallengeSeries);
    installer.Toggle(career, "InstantWinRace", GameEvents::InstallInstantWinRace);
}
