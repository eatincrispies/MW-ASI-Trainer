#include "Pursuit.hpp"

#include <wincrypt.h>

#include <algorithm>
#include <bit>
#include <cctype>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace Memory {

    bool IsReadable(const void* address, std::size_t size) noexcept {
        if (address == nullptr || size == 0) return false;

        constexpr DWORD readable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ |
                                   PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;

        auto cursor    = reinterpret_cast<std::uintptr_t>(address);
        const auto end = cursor + size;
        if (end < cursor) return false;

        while (cursor < end) {
            MEMORY_BASIC_INFORMATION info{};
            if (VirtualQuery(reinterpret_cast<const void*>(cursor), &info, sizeof(info)) == 0) return false;
            if (info.State != MEM_COMMIT) return false;
            if ((info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0 || (info.Protect & readable) == 0) return false;
            cursor = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
        }
        return true;
    }

    bool IsExecutable(const void* address) noexcept {
        MEMORY_BASIC_INFORMATION info{};
        if (address == nullptr || VirtualQuery(address, &info, sizeof(info)) == 0) return false;

        constexpr DWORD executable = PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
        return info.State == MEM_COMMIT && (info.Protect & executable) != 0;
    }

    bool SafeCopy(void* destination, const void* source, std::size_t size) noexcept {
        if (destination == nullptr || source == nullptr) return false;
#if defined(_MSC_VER)
        __try {
            std::memcpy(destination, source, size);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
        return true;
#else
        if (!IsReadable(source, size)) return false;
        std::memcpy(destination, source, size);
        return true;
#endif
    }

    bool Write(std::uintptr_t address, std::span<const std::uint8_t> bytes) noexcept {
        auto* const target = reinterpret_cast<void*>(address);

        DWORD previous = 0;
        if (!VirtualProtect(target, bytes.size(), PAGE_EXECUTE_READWRITE, &previous)) return false;

        std::memcpy(target, bytes.data(), bytes.size());

        DWORD ignored = 0;
        VirtualProtect(target, bytes.size(), previous, &ignored);
        FlushInstructionCache(GetCurrentProcess(), target, bytes.size());
        return true;
    }

}

namespace Scan {

    namespace {

        bool Matches(const std::uint8_t* at, const Memory::Pattern& pattern) noexcept {
            for (std::size_t i = 0; i < pattern.size; ++i) {
                if (!pattern.wildcard[i] && at[i] != pattern.bytes[i]) return false;
            }
            return true;
        }

        const IMAGE_NT_HEADERS* Headers(std::uintptr_t module) noexcept {
            const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(module);
            if (!Memory::IsReadable(dos, sizeof(*dos)) || dos->e_magic != IMAGE_DOS_SIGNATURE) return nullptr;

            const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(module + dos->e_lfanew);
            if (!Memory::IsReadable(nt, sizeof(*nt)) || nt->Signature != IMAGE_NT_SIGNATURE) return nullptr;
            return nt;
        }

    }

    std::uintptr_t HostModule() noexcept {
        return reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    }

    std::optional<std::uintptr_t> Find(const Memory::Pattern& pattern, std::uintptr_t module) noexcept {
        const IMAGE_NT_HEADERS* nt = Headers(module);
        if (nt == nullptr || pattern.size == 0) return std::nullopt;

        std::optional<std::uintptr_t> found;
        const IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(nt);

        for (WORD index = 0; index < nt->FileHeader.NumberOfSections; ++index, ++section) {
            if ((section->Characteristics & IMAGE_SCN_MEM_EXECUTE) == 0) continue;

            const auto* begin = reinterpret_cast<const std::uint8_t*>(module + section->VirtualAddress);
            const std::size_t length = section->Misc.VirtualSize;
            if (length < pattern.size || !Memory::IsReadable(begin, length)) continue;

            for (std::size_t offset = 0; offset + pattern.size <= length; ++offset) {
                if (!Matches(begin + offset, pattern)) continue;
                if (found) return std::nullopt;
                found = reinterpret_cast<std::uintptr_t>(begin + offset);
            }
        }
        return found;
    }

    std::optional<std::uintptr_t> Resolve(const Offsets::OperandSite& site, std::uintptr_t module) noexcept {
        const auto match = Find(site.pattern, module);
        if (!match) return std::nullopt;

        const std::uintptr_t at = *match + site.operand;
        const auto value = Memory::Read<std::uint32_t>(at);
        if (!value) return std::nullopt;

        if (!site.relative) return *value;
        return static_cast<std::uintptr_t>(static_cast<std::intptr_t>(at) + 4 + static_cast<std::int32_t>(*value));
    }

}

namespace {

    constexpr std::uint8_t kJump     = 0xE9;
    constexpr std::uint8_t kNop      = 0x90;
    constexpr std::size_t  kJumpSize = 5;

    void EncodeJump(std::uint8_t* at, std::uintptr_t from, std::uintptr_t to) noexcept {
        const auto relative = static_cast<std::int32_t>(to - (from + kJumpSize));
        at[0] = kJump;
        std::memcpy(at + 1, &relative, sizeof(relative));
    }

}

ScopedPatch::ScopedPatch(std::uintptr_t address, std::span<const std::uint8_t> bytes) noexcept {
    if (bytes.empty() || bytes.size() > original_.size()) return;
    if (!Memory::SafeCopy(original_.data(), reinterpret_cast<const void*>(address), bytes.size())) return;
    if (!Memory::Write(address, bytes)) return;

    address_ = address;
    size_    = bytes.size();
}

ScopedPatch::~ScopedPatch() {
    if (size_ != 0) static_cast<void>(Memory::Write(address_, std::span{ original_.data(), size_ }));
}

void ScopedDetour::Release::operator()(void* memory) const noexcept {
    if (memory != nullptr) VirtualFree(memory, 0, MEM_RELEASE);
}

ScopedDetour::ScopedDetour(std::uintptr_t target, const void* detour, std::size_t stolenBytes,
                           std::uintptr_t& original) noexcept {
    if (stolenBytes < kJumpSize || stolenBytes > 16) return;

    auto* const trampoline = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, stolenBytes + kJumpSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (trampoline == nullptr) return;
    trampoline_.reset(trampoline);

    if (!Memory::SafeCopy(trampoline, reinterpret_cast<const void*>(target), stolenBytes)) {
        trampoline_.reset();
        return;
    }
    EncodeJump(trampoline + stolenBytes, reinterpret_cast<std::uintptr_t>(trampoline + stolenBytes),
               target + stolenBytes);
    FlushInstructionCache(GetCurrentProcess(), trampoline, stolenBytes + kJumpSize);

    original = reinterpret_cast<std::uintptr_t>(trampoline);

    std::array<std::uint8_t, 16> patch{};
    patch.fill(kNop);
    EncodeJump(patch.data(), target, reinterpret_cast<std::uintptr_t>(detour));

    jump_.emplace(target, std::span{ patch.data(), stolenBytes });
    if (*jump_) return;

    jump_.reset();
    trampoline_.reset();
    original = 0;
}

ScopedVTableHook::ScopedVTableHook(std::uintptr_t vtable, std::ptrdiff_t slot, const void* replacement,
                                   std::uintptr_t& original) noexcept {
    const std::uintptr_t at = vtable + slot;
    const auto current = Memory::Read<std::uintptr_t>(at);
    if (!current || !Memory::IsExecutable(reinterpret_cast<const void*>(*current))) return;

    original = *current;

    const auto bytes = std::bit_cast<std::array<std::uint8_t, sizeof(std::uintptr_t)>>(
        reinterpret_cast<std::uintptr_t>(replacement));
    slot_.emplace(at, bytes);
    if (*slot_) return;

    slot_.reset();
    original = 0;
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

    Offsets::DriverClass GetDriverClass(void* vehicle) {
        return static_cast<Offsets::DriverClass>(CallVirtual<int>(vehicle, Offsets::IVehicle::kGetDriverClass));
    }

    bool IsHuman(void* vehicle) {
        return vehicle != nullptr && GetDriverClass(vehicle) == Offsets::DriverClass::Human;
    }

    bool IsPlayer(void* simable) {
        return simable != nullptr && CallVirtual<void*>(simable, Offsets::ISimable::kGetPlayer) != nullptr;
    }

}

namespace {

    constexpr const char* kSection = "PursuitCheats";

    std::string ModuleDirectory() {
        HMODULE module = nullptr;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(&ModuleDirectory), &module);

        std::array<char, MAX_PATH> path{};
        GetModuleFileNameA(module, path.data(), static_cast<DWORD>(path.size()));

        std::string directory{ path.data() };
        if (const auto slash = directory.find_last_of("\\/"); slash != std::string::npos) directory.resize(slash);
        return directory;
    }

    std::string ReadValue(const std::string& ini, const char* key) {
        std::array<char, 64> buffer{};
        GetPrivateProfileStringA(kSection, key, "", buffer.data(), static_cast<DWORD>(buffer.size()), ini.c_str());

        std::string value{ buffer.data() };
        std::ranges::transform(value, value.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
    }

    bool ReadBool(const std::string& ini, const char* key, bool fallback) {
        const std::string value = ReadValue(ini, key);
        if (value == "true" || value == "1" || value == "yes" || value == "on") return true;
        if (value == "false" || value == "0" || value == "no" || value == "off") return false;
        return fallback;
    }

    float ReadFloat(const std::string& ini, const char* key, float fallback, float low, float high) {
        const std::string value = ReadValue(ini, key);
        char* end = nullptr;
        const float parsed = std::strtof(value.c_str(), &end);
        if (value.empty() || end == value.c_str()) return fallback;
        return std::clamp(parsed, low, high);
    }

    std::string HostMd5() {
        std::array<char, MAX_PATH> path{};
        if (GetModuleFileNameA(nullptr, path.data(), static_cast<DWORD>(path.size())) == 0) return "unknown";

        const HANDLE file = CreateFileA(path.data(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) return "unknown";

        std::string digest = "unknown";
        HCRYPTPROV provider = 0;
        HCRYPTHASH hash     = 0;

        if (CryptAcquireContextA(&provider, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT) &&
            CryptCreateHash(provider, CALG_MD5, 0, 0, &hash)) {
            std::array<BYTE, 32768> chunk{};
            bool ok = true;

            for (;;) {
                DWORD read = 0;
                if (!ReadFile(file, chunk.data(), static_cast<DWORD>(chunk.size()), &read, nullptr)) {
                    ok = false;
                    break;
                }
                if (read == 0) break;
                if (!CryptHashData(hash, chunk.data(), read, 0)) {
                    ok = false;
                    break;
                }
            }

            std::array<BYTE, 16> bytes{};
            DWORD size = static_cast<DWORD>(bytes.size());
            if (ok && CryptGetHashParam(hash, HP_HASHVAL, bytes.data(), &size, 0) && size == bytes.size()) {
                digest.clear();
                for (const BYTE b : bytes) {
                    std::array<char, 3> hex{};
                    std::snprintf(hex.data(), hex.size(), "%02X", b);
                    digest += hex.data();
                }
            }
        }

        if (hash != 0) CryptDestroyHash(hash);
        if (provider != 0) CryptReleaseContext(provider, 0);
        CloseHandle(file);
        return digest;
    }

    void WriteLog(const std::string& directory, const std::string& line) {
        const std::string path = directory + "\\PursuitCheats.log";
        if (FILE* file = std::fopen(path.c_str(), "w")) {
            std::fputs(line.c_str(), file);
            std::fputc('\n', file);
            std::fclose(file);
        }
    }

    bool InstallDetour(std::optional<ScopedDetour>& slot, const Offsets::HookSite& site, const void* detour,
                       std::uintptr_t& original) {
        const auto target = Scan::Find(site.pattern);
        if (!target) return false;

        slot.emplace(*target, detour, site.stolenBytes, original);
        if (*slot) return true;

        slot.reset();
        return false;
    }

}

PursuitCheats::~PursuitCheats() {
    if (active_ == this) active_ = nullptr;
}

void PursuitCheats::LoadSettings() {
    directory_ = ModuleDirectory();
    const std::string ini = directory_ + "\\PursuitCheats.ini";

    settings_ = Settings{
        .bustProof            = ReadBool(ini, "BustProof", false),
        .spikeProof           = ReadBool(ini, "SpikeProof", false),
        .infiniteNitro        = ReadBool(ini, "InfiniteNitro", false),
        .infiniteSpeedbreaker = ReadBool(ini, "InfiniteSpeedbreaker", false),
        .tankMode             = ReadBool(ini, "TankMode", false),
        .ghostCops            = ReadBool(ini, "GhostCops", false),
        .disableHelicopter    = ReadBool(ini, "DisableHelicopter", false),
        .copsIgnorePlayer     = ReadBool(ini, "CopsIgnorePlayer", false),
        .instantCooldown      = ReadBool(ini, "InstantCooldown", false),
        .bountyMultiplier     = ReadFloat(ini, "BountyMultiplier", 1.0f, 0.0f, Tuning::kBountyMultiplierMax),
    };
}

void PursuitCheats::ApplyPatches() {
    active_ = this;

    std::string skipped;
    const auto skip = [&skipped](std::string_view feature) {
        if (!skipped.empty()) skipped += ", ";
        skipped += feature;
    };

    ResolveAddresses();

    if (settings_.tankMode && (addresses_.find == 0 || addresses_.rbVehicleHandle == 0)) {
        settings_.tankMode = false;
        skip("TankMode");
    }
    if (settings_.ghostCops && (addresses_.find == 0 || addresses_.vehicleHandle == 0)) {
        settings_.ghostCops = false;
        skip("GhostCops");
    }

    if (settings_.bustProof || settings_.instantCooldown || settings_.tankMode) {
        if (!InstallDetour(installed_.onTask, Offsets::Pursuit::OnTask,
                           reinterpret_cast<const void*>(&PursuitCheats::OnTask), onTask_)) {
            if (settings_.bustProof) skip("BustProof");
            if (settings_.instantCooldown) skip("InstantCooldown");
            if (settings_.tankMode) skip("TankMode");
            settings_.bustProof = settings_.instantCooldown = settings_.tankMode = false;
        }
    }

    if (settings_.spikeProof &&
        !InstallDetour(installed_.puncture, Offsets::DamageRacer::Puncture,
                       reinterpret_cast<const void*>(&PursuitCheats::Puncture), puncture_)) {
        skip("SpikeProof");
    }

    if (settings_.ghostCops &&
        !InstallDetour(installed_.canCollideWith, Offsets::RBVehicle::CanCollideWith,
                       reinterpret_cast<const void*>(&PursuitCheats::CanCollideWith), canCollideWith_)) {
        skip("GhostCops");
    }

    if (settings_.copsIgnorePlayer &&
        !InstallDetour(installed_.checkForPursuit, Offsets::AIVehicleCopCar::CheckForPursuit,
                       reinterpret_cast<const void*>(&PursuitCheats::CheckForPursuit), checkForPursuit_)) {
        skip("CopsIgnorePlayer");
    }

    if (settings_.infiniteNitro && !InstallInfiniteNitro()) skip("InfiniteNitro");
    if (settings_.infiniteSpeedbreaker && !InstallInfiniteSpeedbreaker()) skip("InfiniteSpeedbreaker");
    if (settings_.disableHelicopter && !InstallDisableHelicopter()) skip("DisableHelicopter");
    if (settings_.bountyMultiplier != 1.0f && !InstallBountyMultiplier()) skip("BountyMultiplier");

    std::string line = "Mod injected and applied to v1.3 and " + HostMd5();
    if (!skipped.empty()) line += " (skipped: " + skipped + ")";
    WriteLog(directory_, line);
}

void PursuitCheats::ResolveAddresses() {
    addresses_ = Addresses{
        .find            = Scan::Resolve(Offsets::COM::Find).value_or(0),
        .vehicleHandle   = Scan::Resolve(Offsets::COM::IVehicleHandle).value_or(0),
        .rbVehicleHandle = Scan::Resolve(Offsets::COM::IRBVehicleHandle).value_or(0),
    };

    if (addresses_.find != 0 && !Memory::IsExecutable(reinterpret_cast<const void*>(addresses_.find))) {
        addresses_.find = 0;
    }
}

bool PursuitCheats::InstallInfiniteNitro() {
    const auto& site = Offsets::EngineRacer::DoNosDriverClass;
    const auto match = Scan::Find(site.pattern);
    if (!match) return false;

    const std::uintptr_t at = *match + site.offset;
    if (Memory::Read<std::uint8_t>(at) != site.vanilla) return false;

    const std::array bytes{ static_cast<std::uint8_t>(Offsets::DriverClass::Human) };
    installed_.infiniteNitro.emplace(at, bytes);
    if (*installed_.infiniteNitro) return true;

    installed_.infiniteNitro.reset();
    return false;
}

bool PursuitCheats::InstallInfiniteSpeedbreaker() {
    const auto flag = Scan::Resolve(Offsets::LocalPlayer::InfiniteRaceBreaker);
    if (!flag) return false;

    const auto current = Memory::Read<std::uint8_t>(*flag);
    if (!current || *current > 1) return false;

    const std::array<std::uint8_t, 1> bytes{ 1 };
    installed_.infiniteSpeedbreaker.emplace(*flag, bytes);
    if (*installed_.infiniteSpeedbreaker) return true;

    installed_.infiniteSpeedbreaker.reset();
    return false;
}

bool PursuitCheats::InstallDisableHelicopter() {
    const auto& site = Offsets::AICopManager::SpawnPursuitHelicopter;
    const auto match = Scan::Find(site.pattern);
    if (!match) return false;

    installed_.disableHelicopter.emplace(*match, std::span{ site.bytes.data(), site.size });
    if (*installed_.disableHelicopter) return true;

    installed_.disableHelicopter.reset();
    return false;
}

bool PursuitCheats::InstallBountyMultiplier() {
    const auto vtable = Scan::Resolve(Offsets::AIVehicleHuman::PerpetratorVTable);
    if (!vtable) return false;

    installed_.repPointsFromCops.emplace(
        *vtable, Offsets::IPerpetrator::kAddToPendingRepPointsFromCopDestruction,
        reinterpret_cast<const void*>(&PursuitCheats::AddToPendingRepPointsFromCopDestruction), repPointsFromCops_);
    installed_.repPointsNormal.emplace(
        *vtable, Offsets::IPerpetrator::kAddToPendingRepPointsNormal,
        reinterpret_cast<const void*>(&PursuitCheats::AddToPendingRepPointsNormal), repPointsNormal_);

    if (*installed_.repPointsFromCops && *installed_.repPointsNormal) return true;

    installed_.repPointsNormal.reset();
    installed_.repPointsFromCops.reset();
    return false;
}

void* PursuitCheats::FindInterface(void* object, std::uintptr_t handle) const {
    void* const list = Game::Field<void*>(object, Offsets::COM::kInterfaceList);
    if (list == nullptr) return nullptr;
    return reinterpret_cast<FindFn>(addresses_.find)(list, reinterpret_cast<void*>(handle));
}

bool PursuitCheats::IsCop(void* simable) const {
    void* const vehicle = FindInterface(simable, addresses_.vehicleHandle);
    return vehicle != nullptr && Game::GetDriverClass(vehicle) == Offsets::DriverClass::Cop;
}

bool PursuitCheats::IsPlayerCopPair(void* body, void* other) const {
    void* const first  = Game::Field<void*>(body, Offsets::Behavior::kOwner);
    void* const second = Game::Field<void*>(other, Offsets::Behavior::kOwner);
    if (first == nullptr || second == nullptr) return false;

    if (Game::IsPlayer(first)) return IsCop(second);
    if (Game::IsPlayer(second)) return IsCop(first);
    return false;
}

int PursuitCheats::ScaleBounty(int amount) const noexcept {
    const double scaled = std::round(static_cast<double>(amount) * settings_.bountyMultiplier);
    return static_cast<int>(std::clamp(scaled, static_cast<double>(INT_MIN), static_cast<double>(INT_MAX)));
}

void PursuitCheats::AfterPursuitTask(std::byte* pursuit) const {
    void* const pursuitInterface = pursuit + Offsets::Pursuit::kIPursuit;
    if (!Game::CallVirtual<bool>(pursuitInterface, Offsets::IPursuit::kIsPlayerPursuit)) return;

    if (settings_.bustProof) Game::Field<float>(pursuit, Offsets::Pursuit::kBustTimer) = 0.0f;

    if (settings_.instantCooldown && Game::Field<std::uint8_t>(pursuit, Offsets::Pursuit::kInCoolDown) != 0) {
        const float required = Game::Field<float>(pursuit, Offsets::Pursuit::kCoolDownRequired);
        float& hidden        = Game::Field<float>(pursuit, Offsets::Pursuit::kCoolDownHidden);
        if (hidden < required) hidden = required;
    }

    if (settings_.tankMode) ApplyTankMode(pursuit);
}

void PursuitCheats::ApplyTankMode(std::byte* pursuit) const {
    void* const target = Game::Field<void*>(pursuit, Offsets::Pursuit::kTarget);
    if (target == nullptr) return;

    void* const simable = Game::Field<void*>(target, Offsets::AITarget::kSimable);
    if (simable == nullptr) return;

    void* const rbVehicle = FindInterface(simable, addresses_.rbVehicleHandle);
    void* const rigidBody = Game::CallVirtual<void*>(simable, Offsets::ISimable::kGetRigidBody);
    if (rbVehicle == nullptr || rigidBody == nullptr) return;

    const float mass = Game::CallVirtual<float>(rigidBody, Offsets::IRigidBody::kGetMass);
    Game::CallVirtual<void>(rbVehicle, Offsets::IRBVehicle::kSetCollisionMass, mass * Tuning::kTankCollisionMassScale);
}

bool __fastcall PursuitCheats::OnTask(void* taskable, void*, void* task, float dt) {
    const bool result = reinterpret_cast<OnTaskFn>(onTask_)(taskable, task, dt);

    if (const PursuitCheats* self = active_) {
        auto* const pursuit = static_cast<std::byte*>(taskable) - Offsets::Pursuit::kTaskable;
        Memory::Guarded([&] { self->AfterPursuitTask(pursuit); });
    }
    return result;
}

void __fastcall PursuitCheats::Puncture(void* damage, void*, unsigned wheel) {
    bool shielded = false;
    Memory::Guarded([&] { shielded = Game::IsHuman(Game::Field<void*>(damage, Offsets::DamageRacer::kVehicle)); });

    if (!shielded) reinterpret_cast<PunctureFn>(puncture_)(damage, wheel);
}

bool __fastcall PursuitCheats::CanCollideWith(void* body, void*, void* other) {
    if (!reinterpret_cast<CanCollideWithFn>(canCollideWith_)(body, other)) return false;

    bool ghosted = false;
    if (const PursuitCheats* self = active_) {
        Memory::Guarded([&] { ghosted = self->IsPlayerCopPair(body, other); });
    }
    return !ghosted;
}

bool __fastcall PursuitCheats::CheckForPursuit(void* cop, void*, void* candidate) {
    bool ignored = false;
    Memory::Guarded([&] { ignored = Game::IsHuman(candidate); });

    return !ignored && reinterpret_cast<CheckForPursuitFn>(checkForPursuit_)(cop, candidate);
}

void __fastcall PursuitCheats::AddToPendingRepPointsFromCopDestruction(void* perpetrator, void*, int amount) {
    const PursuitCheats* self = active_;
    reinterpret_cast<AddRepPointsFn>(repPointsFromCops_)(perpetrator, self ? self->ScaleBounty(amount) : amount);
}

void __fastcall PursuitCheats::AddToPendingRepPointsNormal(void* perpetrator, void*, int amount) {
    const PursuitCheats* self = active_;
    reinterpret_cast<AddRepPointsFn>(repPointsNormal_)(perpetrator, self ? self->ScaleBounty(amount) : amount);
}
