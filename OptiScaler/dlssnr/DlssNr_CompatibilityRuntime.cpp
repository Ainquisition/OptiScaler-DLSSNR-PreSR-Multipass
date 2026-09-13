#include "pch.h"
#include "DlssNr_CompatibilityRuntime.h"
#include <Logger.h>
#include <d3d12.h>
#include <nvsdk_ngx.h>
#include <bcrypt.h>
#include <algorithm>
#include <array>
#include <mutex>
#include <map>
#include <string>
#include <vector>
#include <set>
#pragma comment(lib, "bcrypt.lib")

namespace DlssNr
{
namespace
{
// ShortFuse RTX20/30/40 runtime, 310.8.0. Never apply its ABI/import assumptions to another binary.
constexpr std::array<unsigned char, 32> CompatibleHash = {
    0xe6, 0x7d, 0xee, 0x20, 0x93, 0x20, 0xcd, 0xaf, 0xe0, 0xe9, 0x3e, 0x45, 0x67, 0x5d, 0x7a, 0xa3,
    0x43, 0x23, 0xa5, 0x3a, 0xcc, 0x57, 0xa7, 0x2b, 0x2e, 0x40, 0xa1, 0x81, 0x58, 0x1c, 0x98, 0x9a };

struct File
{
    HANDLE handle = INVALID_HANDLE_VALUE;
    ~File() { if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }
};

bool Recognized(HANDLE file)
{
    LARGE_INTEGER size {};
    if (!GetFileSizeEx(file, &size) || size.QuadPart != 165840496) return false;
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return false;
    bool ok = BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) >= 0;
    std::array<unsigned char, 65536> buffer;
    DWORD count = 0;
    while (ok)
    {
        if (!ReadFile(file, buffer.data(), (DWORD)buffer.size(), &count, nullptr)) { ok = false; break; }
        if (!count) break;
        ok = BCryptHashData(hash, buffer.data(), count, 0) >= 0;
    }
    std::array<unsigned char, 32> digest {};
    ok = ok && BCryptFinishHash(hash, digest.data(), (ULONG)digest.size(), 0) >= 0 && digest == CompatibleHash;
    if (hash) BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    return ok;
}

// Only the model's import is redirected. Outside a direct NR call, even that import
// reports the real path. No process-wide hook, driver patch, or on-disk change.
thread_local HMODULE callerAlias = nullptr;
DWORD WINAPI CallerPath(HMODULE queried, LPWSTR path, DWORD capacity)
{
    if (callerAlias && queried == callerAlias)
    {
        constexpr wchar_t alias[] = L"nvngx.dll";
        if (!capacity) { SetLastError(ERROR_INSUFFICIENT_BUFFER); return 0; }
        const DWORD copied = (DWORD)std::min<size_t>(capacity - 1, std::size(alias) - 1);
        memcpy(path, alias, copied * sizeof(wchar_t));
        path[copied] = 0;
        if (capacity < std::size(alias)) { SetLastError(ERROR_INSUFFICIENT_BUFFER); return capacity; }
        return copied;
    }
    return GetModuleFileNameW(queried, path, capacity);
}

struct CallerScope
{
    HMODULE previous = callerAlias;
    CallerScope()
    {
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&CallerPath), &callerAlias);
    }
    ~CallerScope() { callerAlias = previous; }
};

bool ReplaceImport(void** slot, void* expected, void* replacement)
{
    if (*slot != expected) return false;
    DWORD protection = 0;
    if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &protection)) return false;
    const bool replaced = InterlockedCompareExchangePointer(slot, replacement, expected) == expected;
    DWORD ignored = 0;
    VirtualProtect(slot, sizeof(void*), protection, &ignored);
    return replaced;
}
}

struct CompatibilityRuntime::Module
{
    HMODULE handle = nullptr;
    void** importSlot = nullptr;
    std::filesystem::path path;
    std::recursive_mutex mutex;
    std::set<ID3D12Device*> initializedDevices;
    // The snippet Init_Ext ABI takes driver capabilities, unlike the public NGX Init_Ext ABI.
    using Init = NVSDK_NGX_Result (*)(unsigned long long, const wchar_t*, ID3D12Device*, unsigned int,
                                     NVSDK_NGX_Parameter*);
    Init init = nullptr;
    decltype(&NVSDK_NGX_D3D12_CreateFeature) create = nullptr;
    decltype(&NVSDK_NGX_D3D12_EvaluateFeature) evaluate = nullptr;
    decltype(&NVSDK_NGX_D3D12_ReleaseFeature) release = nullptr;
    using Shutdown = NVSDK_NGX_Result (*)(ID3D12Device*);
    Shutdown shutdown = nullptr;

    ~Module()
    {
        if (importSlot) ReplaceImport(importSlot, reinterpret_cast<void*>(&CallerPath),
                                      reinterpret_cast<void*>(&GetModuleFileNameW));
        if (handle) FreeLibrary(handle);
    }
};

std::shared_ptr<CompatibilityRuntime> CompatibilityRuntime::Open(const std::filesystem::path& candidate,
                                                                ID3D12Device* device, Allocate allocate, Destroy destroy,
                                                                const std::filesystem::path& dataPath)
{
    if (!device || !allocate || !destroy) return {};
    // Cache only live owners. Last-owner shutdown/unload occurs after GPU retirement.
    static std::mutex mutex;
    static std::weak_ptr<Module> liveModule;
    static std::map<ID3D12Device*, std::weak_ptr<CompatibilityRuntime>> devices;
    std::lock_guard lock(mutex);
    try
    {
        auto path = std::filesystem::absolute(candidate).lexically_normal();
        auto loaded = liveModule.lock();
        if (loaded && !std::filesystem::equivalent(loaded->path, path)) return {};
        for (auto it = devices.begin(); it != devices.end();)
            if (it->second.expired()) it = devices.erase(it); else ++it;
        if (auto it = devices.find(device); it != devices.end())
            if (auto existing = it->second.lock()) return existing;
        if (!loaded)
        {
            // Keep the verified file locked against replacement through LoadLibrary.
            File file { CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr) };
            if (file.handle == INVALID_HANDLE_VALUE || !Recognized(file.handle)) return {};
            // Do not adopt or alter a runtime somebody else already loaded.
            if (GetModuleHandleW(L"nvngx_dlssnr.dll"))
            {
                LOG_WARN("NR compatibility: runtime already loaded outside this backend; refusing to modify it");
                return {};
            }
            loaded = std::make_shared<Module>();
            loaded->path = path;
            loaded->handle = LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
            if (!loaded->handle) { LOG_ERROR("NR compatibility: LoadLibrary failed ({})", GetLastError()); return {}; }
            auto symbol = [&](const char* name) { return GetProcAddress(loaded->handle, name); };
            loaded->init = reinterpret_cast<Module::Init>(symbol("NVSDK_NGX_D3D12_Init_Ext"));
            loaded->create = reinterpret_cast<decltype(loaded->create)>(symbol("NVSDK_NGX_D3D12_CreateFeature"));
            loaded->evaluate = reinterpret_cast<decltype(loaded->evaluate)>(symbol("NVSDK_NGX_D3D12_EvaluateFeature"));
            loaded->release = reinterpret_cast<decltype(loaded->release)>(symbol("NVSDK_NGX_D3D12_ReleaseFeature"));
            loaded->shutdown = reinterpret_cast<Module::Shutdown>(symbol("NVSDK_NGX_D3D12_Shutdown1"));
            if (!loaded->init || !loaded->create || !loaded->evaluate || !loaded->release || !loaded->shutdown) return {};

            // GetModuleFileNameW IAT slot in the exact hash above; executable instructions are untouched.
            auto slot = reinterpret_cast<void**>(reinterpret_cast<unsigned char*>(loaded->handle) + 0xac080);
            if (!ReplaceImport(slot, reinterpret_cast<void*>(&GetModuleFileNameW), reinterpret_cast<void*>(&CallerPath)))
            { LOG_ERROR("NR compatibility: unexpected caller-path import; refusing runtime"); return {}; }
            loaded->importSlot = slot;
            liveModule = loaded;
            LOG_INFO("NR compatibility: verified 310.8.0 runtime loaded with scoped caller adapter, no helper DLL");
        }

        auto owner = std::shared_ptr<CompatibilityRuntime>(new CompatibilityRuntime());
        owner->module = loaded;
        owner->device = device;
        device->AddRef();
        owner->destroyParameters = destroy;
        std::lock_guard runtimeLock(loaded->mutex);
        // An expired weak owner may still be executing its destructor on another thread.
        if (loaded->initializedDevices.contains(device)) return {};
        auto result = allocate(&owner->capabilities);
        if (result != NVSDK_NGX_Result_Success || !owner->capabilities) return {};
        CallerScope caller;
        const auto writablePath = dataPath.empty() ? std::filesystem::temp_directory_path() : dataPath;
        result = loaded->init(0x24480451ull, writablePath.c_str(), device, 0x15, owner->capabilities);
        LOG_INFO("NR compatibility: Init_Ext result=0x{:08X}", (unsigned)result);
        if (result != NVSDK_NGX_Result_Success) return {};
        owner->initialized = true;
        loaded->initializedDevices.insert(device);
        devices[device] = owner;
        return owner;
    }
    catch (const std::exception& error)
    {
        LOG_ERROR("NR compatibility: {}", error.what());
        return {};
    }
}

CompatibilityRuntime::~CompatibilityRuntime()
{
    std::lock_guard lock(module->mutex);
    CallerScope caller;
    if (initialized)
    {
        const auto result = module->shutdown(device);
        LOG_INFO("NR compatibility: Shutdown1 result=0x{:08X}", (unsigned)result);
        module->initializedDevices.erase(device);
    }
    if (capabilities) destroyParameters(capabilities);
    if (device) device->Release();
}

NVSDK_NGX_Result CompatibilityRuntime::Create(ID3D12GraphicsCommandList* commands, NVSDK_NGX_Parameter* params,
                                             NVSDK_NGX_Handle** feature)
{
    std::lock_guard lock(module->mutex);
    CallerScope caller;
    return module->create(commands, (NVSDK_NGX_Feature)18, params, feature);
}

NVSDK_NGX_Result CompatibilityRuntime::Evaluate(ID3D12GraphicsCommandList* commands, const NVSDK_NGX_Handle* feature,
                                               NVSDK_NGX_Parameter* params)
{
    std::lock_guard lock(module->mutex);
    CallerScope caller;
    return module->evaluate(commands, feature, params, nullptr);
}

NVSDK_NGX_Result CompatibilityRuntime::Release(NVSDK_NGX_Handle* feature)
{
    std::lock_guard lock(module->mutex);
    CallerScope caller;
    return module->release(feature);
}
}
