#pragma once

#include <algorithm>
#include <filesystem>
#include <string>

namespace DlssNr::AmdBridge
{
inline std::string MissingPrerequisite(const std::filesystem::path& directory)
{
    std::error_code ec;
    if (!std::filesystem::is_regular_file(directory / L"dlssnr_amd_pass1.dll", ec))
        return "AMD pre-SR unavailable: missing dlssnr_amd_pass1.dll";

    ec.clear();
    if (!std::filesystem::is_regular_file(directory / L"dlssnr_on_amd_weights.bin", ec))
        return "AMD pre-SR unavailable: missing dlssnr_on_amd_weights.bin";

    return {};
}

// Pass 1 is mandatory and validated separately. Optional passes must form a contiguous prefix.
inline unsigned int ContiguousPassFileCount(const std::filesystem::path& directory, unsigned int requested)
{
    requested = std::clamp(requested, 1u, 3u);
    unsigned int available = 1;

    while (available < requested)
    {
        std::error_code ec;
        const auto next = directory / (L"dlssnr_amd_pass" + std::to_wstring(available + 1) + L".dll");
        if (!std::filesystem::is_regular_file(next, ec))
            break;
        ++available;
    }

    return available;
}
} // namespace DlssNr::AmdBridge
