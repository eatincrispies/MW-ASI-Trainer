#include "FEMarkerManagerAddMarkerToInventory.hpp"

#include "../dllmain.hpp"
#include "FEMarkerManagerGetNumMarkers.hpp"

namespace FEMarkerManagerAddMarkerToInventory {

    namespace {

        constexpr Memory::Pattern kMarkerAward =
            Memory::ParsePattern("8B 04 96 50 53 B9 ?? ?? ?? ?? E8 ?? ?? ?? ?? EB 36");

        constexpr std::ptrdiff_t kManagerLoad            = 6;
        constexpr std::ptrdiff_t kAddCall                = 11;
        constexpr int            kFirstPerformanceMarker = 1;
        constexpr int            kLastPerformanceMarker  = 7;
        constexpr int            kAnyCar                 = 0;

        using AddMarkerToInventoryFn = void(__thiscall*)(void*, int, int);

        std::uintptr_t g_manager  = 0;
        std::uintptr_t g_function = 0;
        bool           g_endless  = false;

        void RefillPerformanceMarkers() {
            auto* const manager = reinterpret_cast<void*>(g_manager);
            for (int type = kFirstPerformanceMarker; type <= kLastPerformanceMarker; ++type) {
                if (FEMarkerManagerGetNumMarkers::Call(manager, type, kAnyCar) > 0) continue;
                reinterpret_cast<AddMarkerToInventoryFn>(g_function)(manager, type, kAnyCar);
            }
        }

    }

    bool EnableEndlessPerformanceMarkers() noexcept {
        if (g_function == 0) {
            if (!FEMarkerManagerGetNumMarkers::Resolve()) return false;

            const auto manager  = Hook::Operand(kMarkerAward, kManagerLoad);
            const auto function = Hook::Operand(kMarkerAward, kAddCall, true);
            if (!manager || *manager == 0 || !function) return false;
            if (!Memory::IsExecutable(reinterpret_cast<const void*>(*function))) return false;

            g_manager  = *manager;
            g_function = *function;
        }

        g_endless = true;
        return true;
    }

    void Tick() noexcept {
        if (!g_endless) return;
        Memory::Guarded([] { RefillPerformanceMarkers(); });
    }

}
