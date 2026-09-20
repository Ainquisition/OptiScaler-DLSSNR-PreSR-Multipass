param(
    [Parameter(Mandatory = $true)] [string] $InputPath,
    [Parameter(Mandatory = $true)] [string] $OutputPath
)

$ErrorActionPreference = 'Stop'
$expectedSize = 7304192
$officialHash = 'B108D6407EB7F094A4F9111EDD778EEE7B978B648D413A9FC7AEEDFDD914C154'
$patchedHash = '175AECA9844B5EAD3E092818DF08D2D8B059021940FF44A200A3B81564DF8012'
$patches = @(
    @{
        Name = 'standalone hook installer'
        Offset = 0x619D
        ContextOffset = 0x6195
        Context = '0031c931d24531c9ff15e5690800488b857803000048'
        Original = 'ff15e5690800'
        Replacement = '31c090909090'
    },
    @{
        Name = 'duplicate ExecuteCommandLists'
        Offset = 0x8B42
        ContextOffset = 0x8B3A
        Context = '4c89c789d34889ceff152017090048833da009090000'
        Original = 'ff1520170900'
        Replacement = '909090909090'
    }
)

function ConvertFrom-Hex([string] $Hex) {
    $bytes = [byte[]]::new($Hex.Length / 2)
    for ($i = 0; $i -lt $bytes.Length; ++$i) {
        $bytes[$i] = [Convert]::ToByte($Hex.Substring($i * 2, 2), 16)
    }
    return $bytes
}

function Assert-Bytes([byte[]] $Image, [int] $Offset, [byte[]] $Expected, [string] $Description) {
    if ($Offset -lt 0 -or $Offset + $Expected.Length -gt $Image.Length) {
        throw "$Description lies outside the runtime image."
    }
    for ($i = 0; $i -lt $Expected.Length; ++$i) {
        if ($Image[$Offset + $i] -ne $Expected[$i]) {
            throw "$Description mismatch at file offset 0x$('{0:X}' -f ($Offset + $i))."
        }
    }
}

$input = (Resolve-Path -LiteralPath $InputPath).Path
$output = [IO.Path]::GetFullPath($OutputPath)
if ($input -eq $output) { throw 'InputPath and OutputPath must differ.' }
if (Test-Path -LiteralPath $output) { throw "Output already exists: $output" }

$image = [IO.File]::ReadAllBytes($input)
if ($image.Length -ne $expectedSize) { throw "Unexpected v0.3.1 runtime size: $($image.Length)." }
if ((Get-FileHash -LiteralPath $input -Algorithm SHA256).Hash -ne $officialHash) {
    throw 'Input is not the exact official DLSS-NR-on-AMD v0.3.1 runtime.'
}

foreach ($patch in $patches) {
    $context = ConvertFrom-Hex $patch.Context
    $original = ConvertFrom-Hex $patch.Original
    $replacement = ConvertFrom-Hex $patch.Replacement
    Assert-Bytes $image $patch.ContextOffset $context "$($patch.Name) context"
    Assert-Bytes $image $patch.Offset $original "$($patch.Name) original bytes"
    [Array]::Copy($replacement, 0, $image, $patch.Offset, $replacement.Length)
}

$outputDirectory = [IO.Path]::GetDirectoryName($output)
$temporary = Join-Path $outputDirectory ('.' + [IO.Path]::GetFileName($output) + '.' + [guid]::NewGuid().ToString('N') + '.tmp')
try {
    $stream = [IO.File]::Open($temporary, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
    try {
        $stream.Write($image, 0, $image.Length)
    }
    finally {
        $stream.Dispose()
    }

    $actualPatchedHash = (Get-FileHash -LiteralPath $temporary -Algorithm SHA256).Hash
    if ($actualPatchedHash -ne $patchedHash) {
        throw "Patched runtime hash mismatch: $actualPatchedHash"
    }
    [IO.File]::Move($temporary, $output)
}
finally {
    if (Test-Path -LiteralPath $temporary) {
        Remove-Item -LiteralPath $temporary -Force
    }
}

Write-Output "Created fail-closed DLSS-NR-on-AMD v0.3.1 runtime: $output"
Write-Output "SHA-256: $actualPatchedHash"
