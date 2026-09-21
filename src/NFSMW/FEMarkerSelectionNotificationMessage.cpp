#include "FEMarkerSelectionNotificationMessage.hpp"

#include "../dllmain.hpp"
#include "FEMarkerSelectionGetSelectedButtonIndex.hpp"
#include "FEMarkerSelectionRedraw.hpp"

#include <algorithm>
#include <utility>

namespace FEMarkerSelectionNotificationMessage {

    namespace {

        constexpr Memory::Pattern kNotificationMessage =
            Memory::ParsePattern("8B 44 24 04 3D 12 89 C0 AB 56 8B F1 0F 87 ?? ?? ?? ?? "
                                 "0F 84 ?? ?? ?? ?? 3D 10 72 40 0C");

        constexpr std::size_t    kStolenBytes = 9;
        constexpr std::uint32_t  kPickCard    = 0x0C407210;
        constexpr std::ptrdiff_t kCardCount   = 0x2C;
        constexpr std::ptrdiff_t kCards       = 0x74;
        constexpr std::ptrdiff_t kCardStride  = 0x0C;
        constexpr std::ptrdiff_t kCardType    = 0x00;
        constexpr std::ptrdiff_t kCardParam   = 0x04;
        constexpr std::ptrdiff_t kCardPicked  = 0x08;
        constexpr int            kMaxCards    = 6;
        constexpr int            kMaxPicks    = 2;
        constexpr int            kPinkSlip    = 0x12;

        using NotificationMessageFn = void(__thiscall*)(void*, std::uint32_t, void*, std::uint32_t, std::uint32_t);

        std::uintptr_t g_original = 0;
        ScopedHook     g_detour;

        std::byte* Card(std::byte* screen, int index) {
            return screen + kCards + index * kCardStride;
        }

        bool IsPicked(std::byte* card) {
            return Game::Field<std::uint8_t>(card, kCardPicked) != 0;
        }

        bool MovePinkSlipToPick(std::byte* screen) {
            const int count = std::clamp(Game::Field<int>(screen, kCardCount), 0, kMaxCards);

            int picks    = 0;
            int pinkSlip = -1;
            for (int index = 0; index < count; ++index) {
                std::byte* const card = Card(screen, index);
                const int type = Game::Field<int>(card, kCardType);

                if (type != 0 && IsPicked(card)) ++picks;
                if (type != kPinkSlip) continue;
                if (IsPicked(card)) return false;
                pinkSlip = index;
            }
            if (pinkSlip < 0 || picks >= kMaxPicks) return false;

            const int choice = FEMarkerSelectionGetSelectedButtonIndex::Call(screen);
            if (choice < 0 || choice >= count || choice == pinkSlip || IsPicked(Card(screen, choice))) return false;

            std::byte* const chosen = Card(screen, choice);
            std::byte* const slip   = Card(screen, pinkSlip);
            std::swap(Game::Field<int>(chosen, kCardType), Game::Field<int>(slip, kCardType));
            std::swap(Game::Field<int>(chosen, kCardParam), Game::Field<int>(slip, kCardParam));
            return true;
        }

        void __fastcall NotificationMessage(void* screen, void*, std::uint32_t message, void* object,
                                            std::uint32_t first, std::uint32_t second) {
            bool moved = false;
            if (message == kPickCard) {
                Memory::Guarded([&] { moved = MovePinkSlipToPick(static_cast<std::byte*>(screen)); });
            }

            reinterpret_cast<NotificationMessageFn>(g_original)(screen, message, object, first, second);

            if (moved) Memory::Guarded([&] { FEMarkerSelectionRedraw::Call(screen); });
        }

    }

    bool Install() noexcept {
        if (!FEMarkerSelectionGetSelectedButtonIndex::Resolve() || !FEMarkerSelectionRedraw::Resolve()) return false;
        return Hook::Detour(g_detour, kNotificationMessage, kStolenBytes,
                            reinterpret_cast<const void*>(&NotificationMessage), g_original);
    }

}
