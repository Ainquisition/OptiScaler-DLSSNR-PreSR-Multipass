#include "pch.h"
#include "DlssNr_CompatibilityRuntime.h"
#include "DlssNr_RuntimeImports.h"
#include <Logger.h>
#include <d3d12.h>
#include <nvsdk_ngx.h>
#include <psapi.h>
#include <algorithm>
#include <mutex>
#include <map>
#include <string>
#include <vector>
#include <set>
#pragma comment(lib, "psapi.lib")

namespace DlssNr
{
namespace
{
struct File
{
    HANDLE handle = INVALID_HANDLE_VALUE;
    ~File() { if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }
};

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

DWORD WINAPI CallerPathA(HMODULE queried, LPSTR path, DWORD capacity)
{
    if (callerAlias && queried == callerAlias)
    {
        constexpr char alias[] = "nvngx.dll";
        if (!capacity) { SetLastError(ERROR_INSUFFICIENT_BUFFER); return 0; }
        const DWORD copied = (DWORD)std::min<size_t>(capacity - 1, std::size(alias) - 1);
        memcpy(path, alias, copied);
        path[copied] = 0;
        if (capacity < std::size(alias)) { SetLastError(ERROR_INSUFFICIENT_BUFFER); return capacity; }
        return copied;
    }
    return GetModuleFileNameA(queried, path, capacity);
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
    std::vector<RuntimeImports::Slot> imports;
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
        for (const auto& slot : imports)
            ReplaceImport(slot.address, slot.wide ? reinterpret_cast<void*>(&CallerPath) : reinterpret_cast<void*>(&CallerPathA),
                          slot.wide ? reinterpret_cast<void*>(&GetModuleFileNameW) : reinterpret_cast<void*>(&GetModuleFileNameA));
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
            // Keep this file locked against replacement through LoadLibrary.
            File file { CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr) };
            if (file.handle == INVALID_HANDLE_VALUE)
            {
                const auto error = GetLastError();
                if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND)
                    LOG_ERROR("NR compatibility: cannot open {} ({})", path.string(), error);
                return {};
            }
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
            if (!loaded->init || !loaded->create || !loaded->evaluate || !loaded->release || !loaded->shutdown)
            { LOG_ERROR("NR compatibility: {} is missing required NR exports", path.string()); return {}; }

            MODULEINFO image {};
            std::vector<RuntimeImports::Slot> slots;
            if (!GetModuleInformation(GetCurrentProcess(), loaded->handle, &image, sizeof(image)) ||
                !RuntimeImports::Find({ static_cast<unsigned char*>(image.lpBaseOfDll), image.SizeOfImage }, slots))
            { LOG_ERROR("NR compatibility: invalid runtime import table in {}", path.string()); return {}; }
            // Resolve named imports from this build; never use a version-specific address.
            loaded->imports.reserve(slots.size());
            for (const auto& slot : slots)
            {
                if (!ReplaceImport(slot.address,
                                   slot.wide ? reinterpret_cast<void*>(&GetModuleFileNameW) : reinterpret_cast<void*>(&GetModuleFileNameA),
                                   slot.wide ? reinterpret_cast<void*>(&CallerPath) : reinterpret_cast<void*>(&CallerPathA)))
                { LOG_ERROR("NR compatibility: cannot adapt caller-path import in {}", path.string()); return {}; }
                loaded->imports.push_back(slot);
            }
            liveModule = loaded;
            LOG_INFO("NR compatibility: loaded {} with {} named caller-path imports, no helper DLL", path.string(), slots.size());
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
