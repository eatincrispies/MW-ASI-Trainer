#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace FECarRecord {

    struct Record {
        std::uint32_t handle;
        std::uint32_t frontend;
        std::uint32_t vehicle;
        std::uint32_t filter;
        std::uint8_t  customization;
        std::uint8_t  career;
        std::uint16_t reserved;
    };

    static_assert(sizeof(Record) == 0x14);

    inline constexpr std::size_t   kDatabaseSize = 0x8CC8;
    inline constexpr std::uint32_t kFreeHandle   = 0xFFFFFFFF;
    inline constexpr std::uint8_t  kNoRecord     = 0xFF;
    inline constexpr std::uint32_t kStockList    = 0x01;
    inline constexpr std::uint32_t kPresetList   = 0x08;
    inline constexpr std::uint32_t kAiPresetList = 0x10;
    inline constexpr std::uint32_t kHiddenList   = 0x20;
    inline constexpr std::uint32_t kListMask     = 0xFFFF;

    [[nodiscard]] std::span<Record> Records(void* database) noexcept;
    [[nodiscard]] Record* Find(void* database, std::uint32_t handle) noexcept;
    [[nodiscard]] std::byte* Customization(void* database, std::uint8_t handle) noexcept;
    [[nodiscard]] int FreeRecords(void* database) noexcept;
    [[nodiscard]] int FreeCustomizations(void* database) noexcept;
    void CopyParts(void* database, std::uint8_t from, std::uint8_t to) noexcept;

}
