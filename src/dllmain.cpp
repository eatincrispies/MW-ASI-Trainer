#include "dllmain.hpp"

#include <wincrypt.h>

#include <algorithm>
#include <bit>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

    constexpr std::uint8_t kCall       = 0xE8;
    constexpr std::uint8_t kJump       = 0xE9;
    constexpr std::uint8_t kNop        = 0x90;
    constexpr std::uint8_t kPushEcx    = 0x51;
    constexpr std::uint8_t kPushEdx    = 0x52;
    constexpr std::uint8_t kPopEdx     = 0x5A;
    constexpr std::uint8_t kPopEcx     = 0x59;
    constexpr std::uint8_t kReturn     = 0xC3;
    constexpr std::size_t  kBranchSize = 5;
    constexpr std::size_t  kThunkSize  = 10;
    constexpr std::size_t  kMaxStolen  = 16;

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

    bool WriteCode(std::uintptr_t address, std::span<const std::uint8_t> bytes) noexcept {
        auto* const target = reinterpret_cast<void*>(address);

        DWORD previous = 0;
        if (!VirtualProtect(target, bytes.size(), PAGE_EXECUTE_READWRITE, &previous)) return false;

        std::memcpy(target, bytes.data(), bytes.size());

        DWORD ignored = 0;
        VirtualProtect(target, bytes.size(), previous, &ignored);
        FlushInstructionCache(GetCurrentProcess(), target, bytes.size());
        return true;
    }

    bool Matches(const std::uint8_t* at, const Memory::Pattern& pattern) noexcept {
        for (std::size_t i = 0; i < pattern.size; ++i) {
            if (!pattern.wildcard[i] && at[i] != pattern.bytes[i]) return false;
        }
        return true;
    }

    void EncodeBranch(std::uint8_t* at, std::uint8_t opcode, std::uintptr_t from, std::uintptr_t to) noexcept {
        const auto relative = static_cast<std::int32_t>(to - (from + kBranchSize));
        at[0] = opcode;
        std::memcpy(at + 1, &relative, sizeof(relative));
    }

    bool Engage(ScopedHook& slot, std::uintptr_t site, std::span<const std::uint8_t> bytes) noexcept {
        FlushInstructionCache(GetCurrentProcess(), slot.block->data(), kMaxStolen + kBranchSize);
        slot.patch.emplace(site, bytes);
        if (*slot.patch) return true;

        slot.Reset();
        return false;
    }

}

namespace Memory {

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

}

namespace Scan {

    std::optional<std::uintptr_t> Find(const Memory::Pattern& pattern) noexcept {
        const auto module = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        const auto* dos   = reinterpret_cast<const IMAGE_DOS_HEADER*>(module);
        if (!IsReadable(dos, sizeof(*dos)) || dos->e_magic != IMAGE_DOS_SIGNATURE) return std::nullopt;

        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(module + dos->e_lfanew);
        if (!IsReadable(nt, sizeof(*nt)) || nt->Signature != IMAGE_NT_SIGNATURE || pattern.size == 0) return std::nullopt;

        std::optional<std::uintptr_t> found;
        const IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(nt);

        for (WORD index = 0; index < nt->FileHeader.NumberOfSections; ++index, ++section) {
            if ((section->Characteristics & IMAGE_SCN_MEM_EXECUTE) == 0) continue;

            const auto* begin = reinterpret_cast<const std::uint8_t*>(module + section->VirtualAddress);
            const std::size_t length = section->Misc.VirtualSize;
            if (length < pattern.size || !IsReadable(begin, length)) continue;

            for (std::size_t offset = 0; offset + pattern.size <= length; ++offset) {
                if (!Matches(begin + offset, pattern)) continue;
                if (found) return std::nullopt;
                found = reinterpret_cast<std::uintptr_t>(begin + offset);
            }
        }
        return found;
    }

    bool At(std::uintptr_t address, const Memory::Pattern& pattern) noexcept {
        const auto* at = reinterpret_cast<const std::uint8_t*>(address);
        return pattern.size != 0 && IsReadable(at, pattern.size) && Matches(at, pattern);
    }

    std::optional<std::uintptr_t> Absolute(std::uintptr_t operand) noexcept {
        const auto value = Memory::Read<std::uint32_t>(operand);
        if (!value) return std::nullopt;
        return static_cast<std::uintptr_t>(*value);
    }

    std::optional<std::uintptr_t> Relative(std::uintptr_t operand) noexcept {
        const auto value = Memory::Read<std::int32_t>(operand);
        if (!value) return std::nullopt;
        return static_cast<std::uintptr_t>(static_cast<std::intptr_t>(operand) + 4 + *value);
    }

}

ScopedPatch::ScopedPatch(std::uintptr_t address, std::span<const std::uint8_t> bytes) noexcept {
    if (bytes.empty() || bytes.size() > original_.size()) return;
    if (!Memory::SafeCopy(original_.data(), reinterpret_cast<const void*>(address), bytes.size())) return;
    if (!WriteCode(address, bytes)) return;

    address_ = address;
    size_    = bytes.size();
}

ScopedPatch::~ScopedPatch() {
    if (size_ != 0) static_cast<void>(WriteCode(address_, std::span{ original_.data(), size_ }));
}

void ExecutableBlock::Release::operator()(void* memory) const noexcept {
    if (memory != nullptr) VirtualFree(memory, 0, MEM_RELEASE);
}

ExecutableBlock::ExecutableBlock(std::size_t size) noexcept
    : memory_(VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE)) {}

namespace Hook {

    std::optional<std::uintptr_t> Operand(const Memory::Pattern& pattern, std::ptrdiff_t offset, bool relative) noexcept {
        const auto match = Scan::Find(pattern);
        if (!match) return std::nullopt;
        return relative ? Scan::Relative(*match + offset) : Scan::Absolute(*match + offset);
    }

    bool Patch(std::optional<ScopedPatch>& slot, const Memory::Pattern& pattern, std::ptrdiff_t offset,
               std::span<const std::uint8_t> bytes) noexcept {
        if (slot) return true;

        const auto match = Scan::Find(pattern);
        if (!match) return false;

        slot.emplace(*match + offset, bytes);
        if (*slot) return true;

        slot.reset();
        return false;
    }

    bool Detour(ScopedHook& slot, const Memory::Pattern& pattern, std::size_t stolenBytes, const void* detour,
                std::uintptr_t& original) noexcept {
        if (slot) return true;

        const auto target = Scan::Find(pattern);
        return target && Detour(slot, *target, stolenBytes, detour, original);
    }

    bool Detour(ScopedHook& slot, std::uintptr_t target, std::size_t stolenBytes, const void* detour,
                std::uintptr_t& original) noexcept {
        if (slot) return true;
        if (stolenBytes < kBranchSize || stolenBytes > kMaxStolen || target == 0) return false;

        slot.block.emplace(kMaxStolen + kBranchSize);
        if (!*slot.block || !Memory::SafeCopy(slot.block->data(), reinterpret_cast<const void*>(target), stolenBytes)) {
            slot.Reset();
            return false;
        }
        EncodeBranch(slot.block->data() + stolenBytes, kJump, slot.block->address() + stolenBytes, target + stolenBytes);
        original = slot.block->address();

        std::array<std::uint8_t, kMaxStolen> jump{};
        jump.fill(kNop);
        EncodeBranch(jump.data(), kJump, target, reinterpret_cast<std::uintptr_t>(detour));
        if (Engage(slot, target, std::span{ jump.data(), stolenBytes })) return true;

        original = 0;
        return false;
    }

    bool Thunk(ScopedHook& slot, std::uintptr_t site, std::size_t length, const void* function) noexcept {
        if (slot) return true;
        if (length < kBranchSize || length > kMaxStolen) return false;

        slot.block.emplace(kMaxStolen + kBranchSize);
        if (!*slot.block) {
            slot.Reset();
            return false;
        }

        std::uint8_t* const code = slot.block->data();
        code[0] = kPushEcx;
        code[1] = kPushEdx;
        EncodeBranch(code + 2, kCall, slot.block->address() + 2, reinterpret_cast<std::uintptr_t>(function));
        code[kThunkSize - 3] = kPopEdx;
        code[kThunkSize - 2] = kPopEcx;
        code[kThunkSize - 1] = kReturn;

        std::array<std::uint8_t, kMaxStolen> call{};
        call.fill(kNop);
        EncodeBranch(call.data(), kCall, site, slot.block->address());
        return Engage(slot, site, std::span{ call.data(), length });
    }

    bool VTable(std::optional<ScopedPatch>& slot, std::uintptr_t vtable, std::ptrdiff_t index, const void* replacement,
                std::uintptr_t& original) noexcept {
        if (slot) return true;

        const std::uintptr_t entry = vtable + index;
        const auto current = Memory::Read<std::uintptr_t>(entry);
        if (!current || !Memory::IsExecutable(reinterpret_cast<const void*>(*current))) return false;

        original = *current;
        const auto bytes = std::bit_cast<std::array<std::uint8_t, sizeof(std::uintptr_t)>>(
            reinterpret_cast<std::uintptr_t>(replacement));
        slot.emplace(entry, bytes);
        if (*slot) return true;

        slot.reset();
        original = 0;
        return false;
    }

    bool RedirectCall(std::optional<ScopedPatch>& slot, std::uintptr_t site, const void* replacement,
                      std::uintptr_t& original) noexcept {
        if (slot) return true;

        const auto opcode = Memory::Read<std::uint8_t>(site);
        const auto target = Scan::Relative(site + 1);
        if (!opcode || *opcode != kCall || !target || !Memory::IsExecutable(reinterpret_cast<const void*>(*target))) return false;

        std::array<std::uint8_t, kBranchSize> call{};
        EncodeBranch(call.data(), kCall, site, reinterpret_cast<std::uintptr_t>(replacement));
        original = *target;
        slot.emplace(site, call);
        if (*slot) return true;

        slot.reset();
        original = 0;
        return false;
    }

}

bool IniSection::Bool(const char* key, bool fallback) const {
    const std::string value = Value(key);
    if (value.empty()) return fallback;
    return value == "true" || value == "1" || value == "yes" || value == "on";
}

float IniSection::Float(const char* key, float fallback, float low, float high) const {
    const std::string value = Value(key);
    char* end = nullptr;
    const float parsed = std::strtof(value.c_str(), &end);
    if (value.empty() || end == value.c_str()) return fallback;
    return std::clamp(parsed, low, high);
}

int IniSection::Int(const char* key, int fallback, int low, int high) const {
    const std::string value = Value(key);
    char* end = nullptr;
    const long parsed = std::strtol(value.c_str(), &end, 10);
    if (value.empty() || end == value.c_str()) return fallback;
    return static_cast<int>(std::clamp<long>(parsed, low, high));
}

bool IniSection::Has(const char* key) const {
    return !Value(key).empty();
}

std::string IniSection::String(const char* key) const {
    return Value(key);
}

std::string IniSection::Value(const char* key) const {
    std::array<char, 128> buffer{};
    GetPrivateProfileStringA(name_, key, "", buffer.data(), static_cast<DWORD>(buffer.size()), path_.c_str());

    std::string value{ buffer.data() };
    if (const auto comment = value.find_first_of(";#"); comment != std::string::npos) value.resize(comment);

    const auto first = value.find_first_not_of(" \t");
    const auto last  = value.find_last_not_of(" \t");
    value = first == std::string::npos ? std::string{} : value.substr(first, last - first + 1);

    std::ranges::transform(value, value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

void Installer::Toggle(const IniSection& section, const char* key, bool (*install)()) {
    if (section.Bool(key)) Check(key, install());
}

void Installer::Check(std::string_view feature, bool installed) {
    if (installed) return;
    if (!skipped_.empty()) skipped_ += ", ";
    skipped_ += feature;
}

namespace {

    constexpr Memory::Pattern kVehicleBehaviorConstructor =
        Memory::ParsePattern("8B 46 34 C7 06 ?? ?? ?? ?? C7 46 08 ?? ?? ?? ?? 8B 48 04 68 ?? ?? ?? ?? E8 ?? ?? ?? ??");

    constexpr std::ptrdiff_t kIVehicleHandlePush = 20;
    constexpr std::ptrdiff_t kIListFindCall      = 25;
    constexpr std::ptrdiff_t kCOMObject          = 0x04;

    constexpr std::ptrdiff_t kISimableGetPlayer    = 0x20;
    constexpr std::ptrdiff_t kISimableGetRigidBody = 0x54;

    constexpr std::ptrdiff_t kIRigidBodyGetMass           = 0x18;
    constexpr std::ptrdiff_t kIRigidBodyGetPosition       = 0x20;
    constexpr std::ptrdiff_t kIRigidBodyGetLinearVelocity = 0x24;
    constexpr std::ptrdiff_t kIRigidBodyGetSpeed          = 0x2C;
    constexpr std::ptrdiff_t kIRigidBodySetPosition       = 0x5C;
    constexpr std::ptrdiff_t kIRigidBodySetLinearVelocity = 0x60;

    constexpr std::ptrdiff_t kIVehicleGetSimable      = 0x04;
    constexpr std::ptrdiff_t kIVehicleGetVehicleClass = 0x44;
    constexpr std::ptrdiff_t kIVehicleGetDriverClass  = 0x58;
    constexpr std::ptrdiff_t kIVehicleSetVehicleOnGround = 0x68;
    constexpr std::ptrdiff_t kIVehicleForceStopOn     = 0x6C;
    constexpr std::ptrdiff_t kIVehicleGetForceStop    = 0x74;
    constexpr std::ptrdiff_t kIVehicleIsDestroyed     = 0x7C;
    constexpr std::ptrdiff_t kIVehicleIsActive        = 0x88;

    using IListFindFn = void*(__thiscall*)(void*, void*);

    std::uintptr_t g_IListFind      = 0;
    std::uintptr_t g_IVehicleHandle = 0;

    constexpr Memory::Pattern k___eDisplayFrame =
        Memory::ParsePattern("55 8B EC 83 E4 F0 81 EC A4 0A 00 00 A1 ?? ?? ?? ?? 53 33 DB");

    constexpr std::size_t k___eDisplayFrameStolen = 6;
    constexpr std::size_t kMostFrameCallbacks     = 8;

    using eDisplayFrameFn = void(__cdecl*)();

    std::uintptr_t                           g___eDisplayFrame = 0;
    std::array<void (*)(), kMostFrameCallbacks> g_frameCallbacks{};
    std::size_t                              g_numFrameCallbacks = 0;
    ScopedHook                               g___eDisplayFrameHook;

    void __cdecl ___eDisplayFrameHook() {
        for (std::size_t index = 0; index < g_numFrameCallbacks; ++index) Memory::Guarded(g_frameCallbacks[index]);
        reinterpret_cast<eDisplayFrameFn>(g___eDisplayFrame)();
    }

}

bool AddFrameCallback(void (*callback)()) noexcept {
    if (callback == nullptr || g_numFrameCallbacks >= kMostFrameCallbacks) return false;
    if (!Hook::Detour(g___eDisplayFrameHook, k___eDisplayFrame, k___eDisplayFrameStolen,
                      reinterpret_cast<const void*>(&___eDisplayFrameHook), g___eDisplayFrame)) {
        return false;
    }
    g_frameCallbacks[g_numFrameCallbacks++] = callback;
    return true;
}

namespace UTL::COM {

    bool IUnknown::Resolve() noexcept {
        if (g_IListFind != 0) return true;

        const auto find = Hook::Operand(kVehicleBehaviorConstructor, kIListFindCall, true);
        if (!find || !Memory::IsExecutable(reinterpret_cast<const void*>(*find))) return false;

        g_IListFind = *find;
        return true;
    }

    void* IUnknown::Find(std::uintptr_t handle) const {
        if (g_IListFind == 0 || handle == 0) return nullptr;

        void* const object = Game::Field<void*>(const_cast<IUnknown*>(this), kCOMObject);
        if (object == nullptr) return nullptr;
        return reinterpret_cast<IListFindFn>(g_IListFind)(object, reinterpret_cast<void*>(handle));
    }

}

float IRigidBody::GetMass() const {
    return Game::CallVirtual<float>(const_cast<IRigidBody*>(this), kIRigidBodyGetMass);
}

const UMath::Vector3& IRigidBody::GetPosition() const {
    return *Game::CallVirtual<const UMath::Vector3*>(const_cast<IRigidBody*>(this), kIRigidBodyGetPosition);
}

const UMath::Vector3& IRigidBody::GetLinearVelocity() const {
    return *Game::CallVirtual<const UMath::Vector3*>(const_cast<IRigidBody*>(this), kIRigidBodyGetLinearVelocity);
}

float IRigidBody::GetSpeed() const {
    return Game::CallVirtual<float>(const_cast<IRigidBody*>(this), kIRigidBodyGetSpeed);
}

void IRigidBody::SetPosition(const UMath::Vector3& position) {
    Game::CallVirtual<void>(this, kIRigidBodySetPosition, &position);
}

void IRigidBody::SetLinearVelocity(const UMath::Vector3& velocity) {
    Game::CallVirtual<void>(this, kIRigidBodySetLinearVelocity, &velocity);
}

bool ISimable::IsPlayer() const {
    return Game::CallVirtual<void*>(const_cast<ISimable*>(this), kISimableGetPlayer) != nullptr;
}

IRigidBody* ISimable::GetRigidBody() const {
    return Game::CallVirtual<IRigidBody*>(const_cast<ISimable*>(this), kISimableGetRigidBody);
}

bool IVehicle::Resolve() noexcept {
    if (g_IVehicleHandle != 0) return true;
    if (!UTL::COM::IUnknown::Resolve()) return false;

    const auto handle = Hook::Operand(kVehicleBehaviorConstructor, kIVehicleHandlePush);
    if (!handle || *handle == 0) return false;

    g_IVehicleHandle = *handle;
    return true;
}

std::uintptr_t IVehicle::_IHandle() noexcept {
    return g_IVehicleHandle;
}

ISimable* IVehicle::GetSimable() const {
    return Game::CallVirtual<ISimable*>(const_cast<IVehicle*>(this), kIVehicleGetSimable);
}

std::uint32_t IVehicle::GetVehicleClass() const {
    const auto* const vehicleClass =
        Game::CallVirtual<const std::uint32_t*>(const_cast<IVehicle*>(this), kIVehicleGetVehicleClass);
    return vehicleClass != nullptr ? *vehicleClass : 0;
}

DriverClass IVehicle::GetDriverClass() const {
    return static_cast<DriverClass>(Game::CallVirtual<int>(const_cast<IVehicle*>(this), kIVehicleGetDriverClass));
}

void IVehicle::ForceStopOn(char forceStopBits) {
    Game::CallVirtual<void>(this, kIVehicleForceStopOn, forceStopBits);
}

char IVehicle::GetForceStop() const {
    return Game::CallVirtual<char>(const_cast<IVehicle*>(this), kIVehicleGetForceStop);
}

bool IVehicle::IsDestroyed() const {
    return Game::CallVirtual<bool>(const_cast<IVehicle*>(this), kIVehicleIsDestroyed);
}

bool IVehicle::IsActive() const {
    return Game::CallVirtual<bool>(const_cast<IVehicle*>(this), kIVehicleIsActive);
}

bool IVehicle::SetVehicleOnGround(const UMath::Vector3& position, const UMath::Vector3& forward) {
    return Game::CallVirtual<bool>(this, kIVehicleSetVehicleOnGround, &position, &forward);
}

bool IVehicle::IsDriver(const IVehicle* vehicle, DriverClass driverClass) {
    return vehicle != nullptr && vehicle->GetDriverClass() == driverClass;
}

bool IVehicle::IsDriver(const ISimable* simable, DriverClass driverClass) {
    return simable != nullptr && IsDriver(simable->QueryInterface<IVehicle>(), driverClass);
}

namespace {

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
        const std::string path = directory + "\\MWCheats.log";
        if (FILE* file = std::fopen(path.c_str(), "w")) {
            std::fputs(line.c_str(), file);
            std::fputc('\n', file);
            std::fclose(file);
        }
    }

    DWORD WINAPI Startup(LPVOID module) {
        const std::string directory = ModuleDirectory();
        const std::string ini       = directory + "\\MWCheats.ini";
        const IniSection  misc{ ini, "Misc" };

        Installer installer;
        ApplyQOL(IniSection{ ini, "QOL" }, installer);
        ApplyMisc(misc, installer, static_cast<HMODULE>(module));
        ApplyMain(IniSection{ ini, "Main" }, installer);
        ApplyCareer(IniSection{ ini, "Career" }, installer);
        ApplyPursuit(IniSection{ ini, "Pursuit" }, installer);
        ApplyWeather(IniSection{ ini, "Weather" }, installer);
        ApplyVisuals(IniSection{ ini, "Visuals" }, installer);
        ApplyCamera(IniSection{ ini, "Camera" }, misc, installer);

        std::string line = "Mod injected and applied to v1.3 and " + HostMd5();
        if (!installer.Skipped().empty()) line += " (skipped: " + installer.Skipped() + ")";
        WriteLog(directory, line);

        ShowLoadedPopup();
        return 0;
    }

}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        if (const HANDLE thread = CreateThread(nullptr, 0, Startup, module, 0, nullptr)) CloseHandle(thread);
    }
    return TRUE;
}
