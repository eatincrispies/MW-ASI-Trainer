#pragma once

#include <Windows.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace Memory {

    struct Pattern {
        std::array<std::uint8_t, 64> bytes{};
        std::array<bool, 64>         wildcard{};
        std::size_t                  size = 0;
    };

    inline void RejectPattern() {}

    consteval std::uint8_t ParseNibble(char c) {
        if (c >= '0' && c <= '9') return static_cast<std::uint8_t>(c - '0');
        if (c >= 'A' && c <= 'F') return static_cast<std::uint8_t>(c - 'A' + 10);
        if (c >= 'a' && c <= 'f') return static_cast<std::uint8_t>(c - 'a' + 10);
        RejectPattern();
        return 0;
    }

    consteval Pattern ParsePattern(std::string_view text) {
        Pattern pattern{};
        for (std::size_t i = 0; i < text.size();) {
            if (text[i] == ' ') {
                ++i;
                continue;
            }
            if (i + 1 >= text.size() || pattern.size == pattern.bytes.size()) RejectPattern();
            if (text[i] == '?' && text[i + 1] == '?') {
                pattern.wildcard[pattern.size] = true;
            } else {
                pattern.bytes[pattern.size] =
                    static_cast<std::uint8_t>(ParseNibble(text[i]) << 4 | ParseNibble(text[i + 1]));
            }
            ++pattern.size;
            i += 2;
        }
        return pattern;
    }

    [[nodiscard]] bool IsExecutable(const void* address) noexcept;
    [[nodiscard]] bool SafeCopy(void* destination, const void* source, std::size_t size) noexcept;

    template <class T>
    [[nodiscard]] std::optional<T> Read(std::uintptr_t address) noexcept {
        T value{};
        if (!SafeCopy(&value, reinterpret_cast<const void*>(address), sizeof(T))) return std::nullopt;
        return value;
    }

    template <class F>
    bool Guarded(F&& body) noexcept {
#if defined(_MSC_VER)
        __try {
            body();
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
        return true;
#else
        body();
        return true;
#endif
    }

}

namespace Scan {

    [[nodiscard]] std::optional<std::uintptr_t> Find(const Memory::Pattern& pattern) noexcept;
    [[nodiscard]] bool At(std::uintptr_t address, const Memory::Pattern& pattern) noexcept;
    [[nodiscard]] std::optional<std::uintptr_t> Absolute(std::uintptr_t operand) noexcept;
    [[nodiscard]] std::optional<std::uintptr_t> Relative(std::uintptr_t operand) noexcept;

}

class ScopedPatch {
public:
    ScopedPatch(std::uintptr_t address, std::span<const std::uint8_t> bytes) noexcept;
    ~ScopedPatch();

    ScopedPatch(const ScopedPatch&)            = delete;
    ScopedPatch& operator=(const ScopedPatch&) = delete;

    [[nodiscard]] explicit operator bool() const noexcept { return size_ != 0; }

private:
    std::uintptr_t               address_ = 0;
    std::array<std::uint8_t, 16> original_{};
    std::size_t                  size_ = 0;
};

class ExecutableBlock {
public:
    explicit ExecutableBlock(std::size_t size) noexcept;

    ExecutableBlock(const ExecutableBlock&)            = delete;
    ExecutableBlock& operator=(const ExecutableBlock&) = delete;

    [[nodiscard]] std::uint8_t* data() const noexcept { return static_cast<std::uint8_t*>(memory_.get()); }
    [[nodiscard]] std::uintptr_t address() const noexcept { return reinterpret_cast<std::uintptr_t>(memory_.get()); }
    [[nodiscard]] explicit operator bool() const noexcept { return memory_ != nullptr; }

private:
    struct Release {
        void operator()(void* memory) const noexcept;
    };

    std::unique_ptr<void, Release> memory_;
};

struct ScopedHook {
    std::optional<ExecutableBlock> block;
    std::optional<ScopedPatch>     patch;

    void Reset() noexcept {
        patch.reset();
        block.reset();
    }

    [[nodiscard]] explicit operator bool() const noexcept { return patch.has_value(); }
};

namespace Hook {

    [[nodiscard]] std::optional<std::uintptr_t> Operand(const Memory::Pattern& pattern, std::ptrdiff_t offset,
                                                        bool relative = false) noexcept;

    [[nodiscard]] bool Patch(std::optional<ScopedPatch>& slot, const Memory::Pattern& pattern, std::ptrdiff_t offset,
                             std::span<const std::uint8_t> bytes) noexcept;

    [[nodiscard]] bool Detour(ScopedHook& slot, const Memory::Pattern& pattern, std::size_t stolenBytes,
                              const void* detour, std::uintptr_t& original) noexcept;

    [[nodiscard]] bool Detour(ScopedHook& slot, std::uintptr_t target, std::size_t stolenBytes, const void* detour,
                              std::uintptr_t& original) noexcept;

    [[nodiscard]] bool Thunk(ScopedHook& slot, std::uintptr_t site, std::size_t length, const void* function) noexcept;

    [[nodiscard]] bool VTable(std::optional<ScopedPatch>& slot, std::uintptr_t vtable, std::ptrdiff_t index,
                              const void* replacement, std::uintptr_t& original) noexcept;

    [[nodiscard]] bool RedirectCall(std::optional<ScopedPatch>& slot, std::uintptr_t site, const void* replacement,
                                    std::uintptr_t& original) noexcept;

}

namespace Game {

    template <class R, class... Args>
    R CallVirtual(void* object, std::ptrdiff_t slot, Args... args) {
        using Function = R(__thiscall*)(void*, Args...);
        const auto vtable   = *static_cast<std::uintptr_t*>(object);
        const auto function = *reinterpret_cast<Function*>(vtable + slot);
        return function(object, args...);
    }

    template <class T>
    T& Field(void* object, std::ptrdiff_t offset) {
        return *reinterpret_cast<T*>(static_cast<std::byte*>(object) + offset);
    }

}

class IniSection {
public:
    IniSection(const std::string& path, const char* name) : path_(path), name_(name) {}

    [[nodiscard]] bool Bool(const char* key, bool fallback = false) const;
    [[nodiscard]] float Float(const char* key, float fallback, float low, float high) const;
    [[nodiscard]] int Int(const char* key, int fallback, int low, int high) const;
    [[nodiscard]] bool Has(const char* key) const;
    [[nodiscard]] std::string String(const char* key) const;

private:
    [[nodiscard]] std::string Value(const char* key) const;

    const std::string& path_;
    const char*        name_;
};

class Installer {
public:
    void Toggle(const IniSection& section, const char* key, bool (*install)());
    void Check(std::string_view feature, bool installed);

    [[nodiscard]] const std::string& Skipped() const noexcept { return skipped_; }

private:
    std::string skipped_;
};

namespace UMath {

    struct Vector3 {
        float x, y, z;
    };

}

bool AddFrameCallback(void (*callback)()) noexcept;

namespace UTL::COM {

    class IUnknown {
    public:
        [[nodiscard]] static bool Resolve() noexcept;

        template <class T>
        [[nodiscard]] T* QueryInterface() const {
            return static_cast<T*>(Find(T::_IHandle()));
        }

        [[nodiscard]] void* Find(std::uintptr_t handle) const;
    };

}

class IRigidBody : public UTL::COM::IUnknown {
public:
    [[nodiscard]] float GetMass() const;
    [[nodiscard]] const UMath::Vector3& GetPosition() const;
    [[nodiscard]] const UMath::Vector3& GetLinearVelocity() const;
    [[nodiscard]] float GetSpeed() const;
    void SetPosition(const UMath::Vector3& position);
    void SetLinearVelocity(const UMath::Vector3& velocity);
};

class ISimable : public UTL::COM::IUnknown {
public:
    [[nodiscard]] bool IsPlayer() const;
    [[nodiscard]] IRigidBody* GetRigidBody() const;
};

enum DriverClass : int {
    DRIVER_HUMAN   = 0,
    DRIVER_TRAFFIC = 1,
    DRIVER_COP     = 2,
    DRIVER_RACER   = 3,
};

class IVehicle : public UTL::COM::IUnknown {
public:
    [[nodiscard]] static bool Resolve() noexcept;
    [[nodiscard]] static std::uintptr_t _IHandle() noexcept;

    [[nodiscard]] ISimable* GetSimable() const;
    [[nodiscard]] std::uint32_t GetVehicleClass() const;
    [[nodiscard]] DriverClass GetDriverClass() const;
    void ForceStopOn(char forceStopBits);
    [[nodiscard]] char GetForceStop() const;
    [[nodiscard]] bool IsDestroyed() const;
    [[nodiscard]] bool IsActive() const;
    bool SetVehicleOnGround(const UMath::Vector3& position, const UMath::Vector3& forward);

    [[nodiscard]] static bool IsDriver(const IVehicle* vehicle, DriverClass driverClass);
    [[nodiscard]] static bool IsDriver(const ISimable* simable, DriverClass driverClass);
};

void ApplyMain(const IniSection& main, Installer& installer);
void ApplyCareer(const IniSection& career, Installer& installer);
void ApplyPursuit(const IniSection& pursuit, Installer& installer);
void ApplyWeather(const IniSection& weather, Installer& installer);
void ApplyVisuals(const IniSection& visuals, Installer& installer);
void ApplyCamera(const IniSection& camera, const IniSection& misc, Installer& installer);
void ApplyQOL(const IniSection& qol, Installer& installer);
void ApplyMisc(const IniSection& misc, Installer& installer, HMODULE resources);
void ShowLoadedPopup() noexcept;
