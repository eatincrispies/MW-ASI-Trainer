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
        std::array<bool, 64> wildcard{};
        std::size_t size = 0;
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

    [[nodiscard]] bool IsReadable(const void* address, std::size_t size) noexcept;
    [[nodiscard]] bool IsExecutable(const void* address) noexcept;
    [[nodiscard]] bool SafeCopy(void* destination, const void* source, std::size_t size) noexcept;
    [[nodiscard]] bool Write(std::uintptr_t address, std::span<const std::uint8_t> bytes) noexcept;

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

namespace Offsets {

    enum class DriverClass : int {
        Human   = 0,
        Traffic = 1,
        Cop     = 2,
        Racer   = 3,
    };

    struct HookSite {
        std::string_view name;
        Memory::Pattern  pattern;
        std::size_t      stolenBytes = 0;
    };

    struct OperandSite {
        std::string_view name;
        Memory::Pattern  pattern;
        std::ptrdiff_t   operand  = 0;
        bool             relative = false;
    };

    struct ByteSite {
        std::string_view name;
        Memory::Pattern  pattern;
        std::ptrdiff_t   offset  = 0;
        std::uint8_t     vanilla = 0;
    };

    struct PatchSite {
        std::string_view            name;
        Memory::Pattern             pattern;
        std::array<std::uint8_t, 8> bytes{};
        std::size_t                 size = 0;
    };

    namespace Pursuit {
        inline constexpr HookSite OnTask{
            .name        = "AIPursuit::OnTask",
            .pattern     = Memory::ParsePattern("8B 44 24 04 83 EC 44 53 55 56 8B F1 3B 46 50 57 75 ?? "
                                                "D9 86 20 01 00 00 D8 86 1C 01 00 00"),
            .stolenBytes = 7,
        };

        inline constexpr std::ptrdiff_t kTaskable         = 0x008;
        inline constexpr std::ptrdiff_t kIPursuit         = 0x048;
        inline constexpr std::ptrdiff_t kTarget           = 0x0BC;
        inline constexpr std::ptrdiff_t kBustTimer        = 0x124;
        inline constexpr std::ptrdiff_t kCoolDownRequired = 0x14C;
        inline constexpr std::ptrdiff_t kCoolDownHidden   = 0x16C;
        inline constexpr std::ptrdiff_t kInCoolDown       = 0x174;
    }

    namespace IPursuit {
        inline constexpr std::ptrdiff_t kIsPlayerPursuit = 0x8C;
    }

    namespace AITarget {
        inline constexpr std::ptrdiff_t kSimable = 0x1C;
    }

    namespace Behavior {
        inline constexpr std::ptrdiff_t kOwner = 0x34;
    }

    namespace ISimable {
        inline constexpr std::ptrdiff_t kGetPlayer    = 0x20;
        inline constexpr std::ptrdiff_t kGetRigidBody = 0x54;
    }

    namespace IVehicle {
        inline constexpr std::ptrdiff_t kGetDriverClass = 0x58;
    }

    namespace IRigidBody {
        inline constexpr std::ptrdiff_t kGetMass = 0x18;
    }

    namespace IRBVehicle {
        inline constexpr std::ptrdiff_t kSetCollisionMass = 0x04;
    }

    namespace IPerpetrator {
        inline constexpr std::ptrdiff_t kAddToPendingRepPointsFromCopDestruction = 0x38;
        inline constexpr std::ptrdiff_t kAddToPendingRepPointsNormal             = 0x3C;
    }

    namespace COM {
        inline constexpr std::ptrdiff_t kInterfaceList = 0x04;

        inline constexpr Memory::Pattern kVehicleLookup =
            Memory::ParsePattern("8B 46 34 C7 06 ?? ?? ?? ?? C7 46 08 ?? ?? ?? ?? 8B 48 04 68 ?? ?? ?? ?? E8 ?? ?? ?? ??");

        inline constexpr OperandSite Find{
            .name     = "UTL::COM::Object::_IList::Find",
            .pattern  = kVehicleLookup,
            .operand  = 25,
            .relative = true,
        };

        inline constexpr OperandSite IVehicleHandle{
            .name    = "IVehicle::_IHandle",
            .pattern = kVehicleLookup,
            .operand = 20,
        };

        inline constexpr OperandSite IRBVehicleHandle{
            .name    = "IRBVehicle::_IHandle",
            .pattern = Memory::ParsePattern("8B 4D 04 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B F0 85 F6 74 ?? 84 DB 74 ?? "
                                            "8B 45 00 8B CD FF 50 54 8B 10 8B 1E 8B C8 FF 52 18"),
            .operand = 4,
        };
    }

    namespace DamageRacer {
        inline constexpr HookSite Puncture{
            .name        = "DamageRacer::Puncture",
            .pattern     = Memory::ParsePattern("64 A1 00 00 00 00 6A FF 68 ?? ?? ?? ?? 50 64 89 25 00 00 00 00 "
                                                "56 57 8B 7C 24 18 83 FF 04 8B F1 73 ?? 8A 44 37 1C"),
            .stolenBytes = 6,
        };

        inline constexpr std::ptrdiff_t kVehicle = -0x70;
    }

    namespace EngineRacer {
        inline constexpr ByteSite DoNosDriverClass{
            .name    = "EngineRacer::DoNos",
            .pattern = Memory::ParsePattern("A0 ?? ?? ?? ?? 84 C0 D8 74 24 28 D9 5C 24 28 75 0D "
                                            "8B 4E 48 8B 11 FF 52 58 83 F8 06 75 08"),
            .offset  = 0x1B,
            .vanilla = 0x06,
        };
    }

    namespace LocalPlayer {
        inline constexpr OperandSite InfiniteRaceBreaker{
            .name    = "Tweak_InfiniteRaceBreaker",
            .pattern = Memory::ParsePattern("8A 86 80 00 00 00 84 C0 D8 0D ?? ?? ?? ?? 74 ?? A0 ?? ?? ?? ?? DD D8 84 C0 75 ??"),
            .operand = 17,
        };
    }

    namespace RBVehicle {
        inline constexpr HookSite CanCollideWith{
            .name        = "RBVehicle::CanCollideWith",
            .pattern     = Memory::ParsePattern("8A 81 70 01 00 00 84 C0 74 1C 8B 41 78 8B 08 8A 51 1D"),
            .stolenBytes = 6,
        };
    }

    namespace AICopManager {
        inline constexpr PatchSite SpawnPursuitHelicopter{
            .name    = "AICopManager::SpawnPursuitHelicopter",
            .pattern = Memory::ParsePattern("6A FF 68 ?? ?? ?? ?? 64 A1 00 00 00 00 50 64 89 25 00 00 00 00 "
                                            "81 EC A0 03 00 00 A1"),
            .bytes   = { 0x32, 0xC0, 0xC2, 0x04, 0x00 },
            .size    = 5,
        };
    }

    namespace AIVehicleCopCar {
        inline constexpr HookSite CheckForPursuit{
            .name        = "AIVehicleCopCar::CheckForPursuit",
            .pattern     = Memory::ParsePattern("6A FF 68 ?? ?? ?? ?? 64 A1 00 00 00 00 50 64 89 25 00 00 00 00 "
                                                "83 EC 4C 53 8B 5C 24 60 8B 03 56 57 8B F9 8B CB FF 50 04"),
            .stolenBytes = 7,
        };
    }

    namespace AIVehicleHuman {
        inline constexpr OperandSite PerpetratorVTable{
            .name    = "AIVehicleHuman::IPerpetrator vtable",
            .pattern = Memory::ParsePattern("C7 46 54 ?? ?? ?? ?? C7 86 58 07 00 00 ?? ?? ?? ?? "
                                            "C7 86 60 07 00 00 ?? ?? ?? ?? C7 86 6C 07 00 00 ?? ?? ?? ?? "
                                            "C7 86 C4 07 00 00 ?? ?? ?? ?? 8B C6 5E"),
            .operand = 13,
        };
    }

}

namespace Tuning {
    inline constexpr float kTankCollisionMassScale = 10.0f;
    inline constexpr float kBountyMultiplierMax    = 1000.0f;
}

namespace Scan {
    [[nodiscard]] std::uintptr_t HostModule() noexcept;
    [[nodiscard]] std::optional<std::uintptr_t> Find(const Memory::Pattern& pattern,
                                                     std::uintptr_t module = HostModule()) noexcept;
    [[nodiscard]] std::optional<std::uintptr_t> Resolve(const Offsets::OperandSite& site,
                                                        std::uintptr_t module = HostModule()) noexcept;
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

class ScopedDetour {
public:
    ScopedDetour(std::uintptr_t target, const void* detour, std::size_t stolenBytes,
                 std::uintptr_t& original) noexcept;

    ScopedDetour(const ScopedDetour&)            = delete;
    ScopedDetour& operator=(const ScopedDetour&) = delete;

    [[nodiscard]] explicit operator bool() const noexcept { return jump_.has_value(); }

private:
    struct Release {
        void operator()(void* memory) const noexcept;
    };

    std::unique_ptr<void, Release> trampoline_;
    std::optional<ScopedPatch>     jump_;
};

class ScopedVTableHook {
public:
    ScopedVTableHook(std::uintptr_t vtable, std::ptrdiff_t slot, const void* replacement,
                     std::uintptr_t& original) noexcept;

    ScopedVTableHook(const ScopedVTableHook&)            = delete;
    ScopedVTableHook& operator=(const ScopedVTableHook&) = delete;

    [[nodiscard]] explicit operator bool() const noexcept { return slot_.has_value(); }

private:
    std::optional<ScopedPatch> slot_;
};

struct Settings {
    bool  bustProof            = false;
    bool  spikeProof           = false;
    bool  infiniteNitro        = false;
    bool  infiniteSpeedbreaker = false;
    bool  tankMode             = false;
    bool  ghostCops            = false;
    bool  disableHelicopter    = false;
    bool  copsIgnorePlayer     = false;
    bool  instantCooldown      = false;
    float bountyMultiplier     = 1.0f;
};

class PursuitCheats {
public:
    PursuitCheats() = default;
    ~PursuitCheats();

    PursuitCheats(const PursuitCheats&)            = delete;
    PursuitCheats& operator=(const PursuitCheats&) = delete;

    void LoadSettings();
    void ApplyPatches();

private:
    using OnTaskFn          = bool(__thiscall*)(void*, void*, float);
    using PunctureFn        = void(__thiscall*)(void*, unsigned);
    using CanCollideWithFn  = bool(__thiscall*)(void*, void*);
    using CheckForPursuitFn = bool(__thiscall*)(void*, void*);
    using AddRepPointsFn    = void(__thiscall*)(void*, int);
    using FindFn            = void*(__thiscall*)(void*, void*);

    struct Addresses {
        std::uintptr_t find            = 0;
        std::uintptr_t vehicleHandle   = 0;
        std::uintptr_t rbVehicleHandle = 0;
    };

    struct Installed {
        std::optional<ScopedDetour>     onTask;
        std::optional<ScopedDetour>     puncture;
        std::optional<ScopedDetour>     canCollideWith;
        std::optional<ScopedDetour>     checkForPursuit;
        std::optional<ScopedVTableHook> repPointsFromCops;
        std::optional<ScopedVTableHook> repPointsNormal;
        std::optional<ScopedPatch>      infiniteNitro;
        std::optional<ScopedPatch>      infiniteSpeedbreaker;
        std::optional<ScopedPatch>      disableHelicopter;
    };

    static bool __fastcall OnTask(void* taskable, void* edx, void* task, float dt);
    static void __fastcall Puncture(void* damage, void* edx, unsigned wheel);
    static bool __fastcall CanCollideWith(void* body, void* edx, void* other);
    static bool __fastcall CheckForPursuit(void* cop, void* edx, void* candidate);
    static void __fastcall AddToPendingRepPointsFromCopDestruction(void* perpetrator, void* edx, int amount);
    static void __fastcall AddToPendingRepPointsNormal(void* perpetrator, void* edx, int amount);

    void ResolveAddresses();
    bool InstallInfiniteNitro();
    bool InstallInfiniteSpeedbreaker();
    bool InstallDisableHelicopter();
    bool InstallBountyMultiplier();

    void AfterPursuitTask(std::byte* pursuit) const;
    void ApplyTankMode(std::byte* pursuit) const;
    [[nodiscard]] bool IsPlayerCopPair(void* body, void* other) const;
    [[nodiscard]] bool IsCop(void* simable) const;
    [[nodiscard]] void* FindInterface(void* object, std::uintptr_t handle) const;
    [[nodiscard]] int ScaleBounty(int amount) const noexcept;

    inline static PursuitCheats*  active_            = nullptr;
    inline static std::uintptr_t  onTask_            = 0;
    inline static std::uintptr_t  puncture_          = 0;
    inline static std::uintptr_t  canCollideWith_    = 0;
    inline static std::uintptr_t  checkForPursuit_   = 0;
    inline static std::uintptr_t  repPointsFromCops_ = 0;
    inline static std::uintptr_t  repPointsNormal_   = 0;

    Settings    settings_{};
    Addresses   addresses_{};
    std::string directory_;
    Installed   installed_{};
};
