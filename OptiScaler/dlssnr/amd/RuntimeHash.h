#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace AmdPreSr
{
enum class RuntimeIdentity
{
    Unknown,
    ProductionV0214,
    OfficialV031NeedsPatching,
    ExperimentalV031,
};

struct RuntimeContract
{
    RuntimeIdentity identity;
    const char* version;
    std::size_t packetSize;
    bool singlePassOnly;
    bool compilerImportPatch;
    bool spinCap;
    std::size_t init, record, notify, shutdown;
    std::size_t device, queue, engine, hipDevice, inlineMode, interop, enabled, useFsrInputs, useDepth, tonemap,
        ready, nativeFailure;
    std::size_t temporal, history, historyValid, depthInverted, depthConvention, localTone, localStructure,
        skinStructure, autoMask, toneChannels;
    std::size_t pendingList, expectedJob, completedJob, timeoutCounter, abortPointer;
    std::size_t compilerImport, spinLimit;
};

inline constexpr std::array<unsigned char, 32> V0214PatchedSha256 {
    0x3c, 0x9c, 0xa1, 0x3f, 0x0f, 0x5f, 0xc3, 0x6a, 0x69, 0x0b, 0xa4, 0x24, 0xc4, 0x57, 0x00, 0x3b,
    0xcf, 0xcc, 0x10, 0x80, 0xb4, 0xb7, 0x85, 0x97, 0x4c, 0xdd, 0x7e, 0x9a, 0xe2, 0xbc, 0x1d, 0xd8 };
inline constexpr std::array<unsigned char, 32> V031OfficialSha256 {
    0xb1, 0x08, 0xd6, 0x40, 0x7e, 0xb7, 0xf0, 0x94, 0xa4, 0xf9, 0x11, 0x1e, 0xdd, 0x77, 0x8e, 0xee,
    0x7b, 0x97, 0x8b, 0x64, 0x8d, 0x41, 0x3a, 0x9f, 0xc7, 0xae, 0xed, 0xfd, 0xd9, 0x14, 0xc1, 0x54 };
inline constexpr std::array<unsigned char, 32> V031PatchedSha256 {
    0x17, 0x5a, 0xec, 0xa9, 0x84, 0x4b, 0x5e, 0xad, 0x3e, 0x09, 0x28, 0x18, 0xdf, 0x08, 0xd2, 0xd8,
    0xb0, 0x59, 0x02, 0x19, 0x40, 0xff, 0x44, 0xa2, 0x00, 0xa3, 0xb8, 0x15, 0x64, 0xdf, 0x80, 0x12 };

inline constexpr RuntimeContract ProductionV0214 {
    .identity = RuntimeIdentity::ProductionV0214,
    .version = "v0.2.14-patched",
    .packetSize = 0x50,
    .singlePassOnly = false,
    .compilerImportPatch = true,
    .spinCap = true,
    .init = 0x12380,
    .record = 0xa0b0,
    .notify = 0x4640,
    .shutdown = 0xc520,
    .device = 0x764c8,
    .queue = 0x764d0,
    .engine = 0x764d8,
    .hipDevice = 0x76f20,
    .inlineMode = 0x76be0,
    .interop = 0x76c8c,
    .enabled = 0x76e1c,
    .useFsrInputs = 0x76e1e,
    .useDepth = 0x76e1f,
    .tonemap = 0x76e20,
    .ready = 0x767f8,
    .nativeFailure = 0x767fa,
    .temporal = 0x76e1d,
    .history = 0x765f0,
    .historyValid = 0x765f8,
    .depthInverted = 0x76e10,
    .depthConvention = 0x76e14,
    .localTone = 0x76e30,
    .localStructure = 0x76e34,
    .skinStructure = 0x76e38,
    .autoMask = 0x76e40,
    .toneChannels = 0x76e44,
    .pendingList = 0x76d68,
    .expectedJob = 0x76d74,
    .completedJob = 0x76c14,
    .timeoutCounter = 0x76c18,
    .abortPointer = 0x76c68,
    .compilerImport = 0x6bb50,
    .spinLimit = 0x76c44,
};

inline constexpr RuntimeContract ExperimentalV031 {
    .identity = RuntimeIdentity::ExperimentalV031,
    .version = "v0.3.1-experimental",
    .packetSize = 0x60,
    .singlePassOnly = true,
    .compilerImportPatch = false,
    .spinCap = false,
    .init = 0x21720,
    .record = 0x13540,
    .notify = 0x9720,
    .shutdown = 0x17150,
    .device = 0x9a0e8,
    .queue = 0x9a0f0,
    .engine = 0x9a100,
    .hipDevice = 0x9ae08,
    .inlineMode = 0x9a928,
    .interop = 0x9ab58,
    .enabled = 0x9acf4,
    .useFsrInputs = 0x9acf6,
    .useDepth = 0x9acf7,
    .tonemap = 0x9acf8,
    .ready = 0x9a420,
    .nativeFailure = 0x9a422,
    .temporal = 0x9acf5,
    .history = 0x9a218,
    .historyValid = 0x9a220,
    .depthInverted = 0x9ace8,
    .depthConvention = 0x9acec,
    .localTone = 0x9ad08,
    .localStructure = 0x9ad0c,
    .skinStructure = 0x9ad10,
    .autoMask = 0x9ad18,
    .toneChannels = 0x9ad1c,
    .pendingList = 0x9ac38,
    .expectedJob = 0x9ac44,
    .completedJob = 0x9a95c,
    .timeoutCounter = 0x9a960,
    .abortPointer = 0x9aa38,
    .compilerImport = 0,
    .spinLimit = 0,
};

constexpr bool DigestEquals(const unsigned char* digest, const std::array<unsigned char, 32>& expected)
{
    for (std::size_t i = 0; i < expected.size(); ++i)
        if (digest[i] != expected[i])
            return false;
    return true;
}

constexpr RuntimeIdentity IdentifyRuntime(std::uint64_t size, const unsigned char* digest)
{
    if (!digest)
        return RuntimeIdentity::Unknown;
    if (size == 7156224 && DigestEquals(digest, V0214PatchedSha256))
        return RuntimeIdentity::ProductionV0214;
    if (size == 7304192 && DigestEquals(digest, V031OfficialSha256))
        return RuntimeIdentity::OfficialV031NeedsPatching;
    if (size == 7304192 && DigestEquals(digest, V031PatchedSha256))
        return RuntimeIdentity::ExperimentalV031;
    return RuntimeIdentity::Unknown;
}

constexpr const RuntimeContract* ContractFor(RuntimeIdentity identity)
{
    if (identity == RuntimeIdentity::ProductionV0214)
        return &ProductionV0214;
    if (identity == RuntimeIdentity::ExperimentalV031)
        return &ExperimentalV031;
    return nullptr;
}

constexpr bool RecordAccepted(RuntimeIdentity identity, bool callResult, bool pendingRecorded)
{
    return pendingRecorded && (identity != RuntimeIdentity::ExperimentalV031 || callResult);
}
} // namespace AmdPreSr
