# Requires a Visual Studio developer PowerShell and an NVIDIA GPU. Uses supplied local DLLs only.
param(
    [Parameter(Mandatory)][string]$Driver,
    [Parameter(Mandatory)][string]$RuntimeDirectory,
    [string[]]$RejectRuntimeDirectories = @()
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path "$PSScriptRoot/../..").Path
$build = Join-Path ([IO.Path]::GetTempPath()) ('nr-compatibility-' + [guid]::NewGuid())
New-Item -ItemType Directory -Path $build | Out-Null
foreach ($header in @('pch.h', 'Logger.h')) {
    Set-Content -LiteralPath "$build/$header" -Value '// Dependencies supplied by Adapter.h.'
}
& cl.exe /nologo /std:c++20 /EHsc /O2 /MD /W4 "/FI$PSScriptRoot/Adapter.h" "/I$build" `
    "/I$repo/external/nvngx_dlss_sdk" "$PSScriptRoot/HardwareSmoke.cpp" `
    "$repo/OptiScaler/dlssnr/DlssNr_CompatibilityRuntime.cpp" "/Fo$build/" "/Fe:$build/smoke.exe" `
    /link d3d12.lib dxgi.lib
if ($LASTEXITCODE) { throw 'Compatibility smoke compilation failed.' }
foreach ($directory in $RejectRuntimeDirectories) {
    & "$build/smoke.exe" $Driver $directory --reject
    if ($LASTEXITCODE) { throw "Runtime rejection test failed: $directory" }
}
& "$build/smoke.exe" $Driver $RuntimeDirectory
if ($LASTEXITCODE) { throw 'Compatibility GPU smoke failed.' }
