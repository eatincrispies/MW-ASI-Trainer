#include "FECarRecord.hpp"

#include <algorithm>
#include <cstring>

namespace FECarRecord {

    namespace {

        constexpr std::size_t    kRecordSlots        = 200;
        constexpr std::ptrdiff_t kCustomizations     = 0xFA0;
        constexpr std::size_t    kCustomizationSlots = 75;
        constexpr std::size_t    kCustomizationSize  = 0x198;
        constexpr std::size_t    kCustomizationOwner = 0x194;

    }

    std::span<Record> Records(void* database) noexcept {
        return { static_cast<Record*>(database), kRecordSlots };
    }

    Record* Find(void* database, std::uint32_t handle) noexcept {
        if (handle == kFreeHandle) return nullptr;

        const auto records = Records(database);
        const auto match   = std::ranges::find(records, handle, &Record::handle);
        return match == records.end() ? nullptr : &*match;
    }

    std::byte* Customization(void* database, std::uint8_t handle) noexcept {
        if (handle >= kCustomizationSlots) return nullptr;
        return static_cast<std::byte*>(database) + kCustomizations + static_cast<std::size_t>(handle) * kCustomizationSize;
    }

    int FreeRecords(void* database) noexcept {
        return static_cast<int>(std::ranges::count(Records(database), kFreeHandle, &Record::handle));
    }

    int FreeCustomizations(void* database) noexcept {
        int free = 0;
        for (std::size_t slot = 0; slot < kCustomizationSlots; ++slot) {
            const std::byte* const customization = Customization(database, static_cast<std::uint8_t>(slot));
            if (std::to_integer<std::uint8_t>(customization[kCustomizationOwner]) == kNoRecord) ++free;
        }
        return free;
    }

    void CopyParts(void* database, std::uint8_t from, std::uint8_t to) noexcept {
        std::byte* const source      = Customization(database, from);
        std::byte* const destination = Customization(database, to);
        if (source == nullptr || destination == nullptr || source == destination) return;
        std::memcpy(destination, source, kCustomizationOwner);
    }

}
