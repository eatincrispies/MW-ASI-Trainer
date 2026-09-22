#include "FEPlayerCarDBCreateNewCareerCar.hpp"

#include "../dllmain.hpp"
#include "FECarRecord.hpp"

namespace FEPlayerCarDBCreateNewCareerCar {

    namespace {

        constexpr Memory::Pattern kCreateNewCareerCar =
            Memory::ParsePattern("56 68 02 00 0F 00 8B F1 E8 ?? ?? ?? ?? 83 F8 19 7C 06 33 C0 5E C2 04 00");

        constexpr std::size_t   kStolenBytes = 6;
        constexpr std::uint32_t kForSale     = FECarRecord::kStockList | FECarRecord::kPresetList;

        using CreateNewCareerCarFn = FECarRecord::Record*(__thiscall*)(void*, std::uint32_t);

        std::uintptr_t g_original = 0;
        ScopedHook     g_detour;

        void KeepPresetParts(void* database, std::uint32_t handle, const FECarRecord::Record& car) {
            const FECarRecord::Record* const source = FECarRecord::Find(database, handle);
            if (source == nullptr || source == &car || (source->filter & kForSale) != kForSale) return;
            if (source->customization == FECarRecord::kNoRecord || car.customization == FECarRecord::kNoRecord) return;

            FECarRecord::CopyParts(database, source->customization, car.customization);
        }

        FECarRecord::Record* __fastcall CreateNewCareerCar(void* database, void*, std::uint32_t handle) {
            FECarRecord::Record* const car = reinterpret_cast<CreateNewCareerCarFn>(g_original)(database, handle);
            if (car != nullptr) Memory::Guarded([&] { KeepPresetParts(database, handle, *car); });
            return car;
        }

    }

    bool InstallPresetParts() noexcept {
        return Hook::Detour(g_detour, kCreateNewCareerCar, kStolenBytes,
                            reinterpret_cast<const void*>(&CreateNewCareerCar), g_original);
    }

}
