[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet('x64', 'arm64')][string]$Architecture,
    [Parameter(Mandatory)][string]$Executable,
    [Parameter(Mandatory)][ValidatePattern('^[A-Za-z0-9.-]{3,50}$')][string]$IdentityName,
    [Parameter(Mandatory)][string]$Publisher,
    [ValidatePattern('^\d+\.\d+\.\d+\.0$')][string]$Version = '0.3.0.0',
    [string]$OutputDirectory = 'dist'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$binary = (Resolve-Path -LiteralPath $Executable).Path
# Refuse to mislabel a native binary as a different architecture.
$binaryReader = [IO.BinaryReader]::new([IO.File]::OpenRead($binary))
try {
    if ($binaryReader.ReadUInt16() -ne 0x5A4D) { throw 'Executable is not a Windows PE file.' }
    [void]$binaryReader.BaseStream.Seek(0x3C, [IO.SeekOrigin]::Begin)
    $peOffset = $binaryReader.ReadInt32()
    [void]$binaryReader.BaseStream.Seek($peOffset, [IO.SeekOrigin]::Begin)
    if ($binaryReader.ReadUInt32() -ne 0x00004550) { throw 'Invalid PE signature.' }
    $machine = $binaryReader.ReadUInt16()
    $expectedMachine = if ($Architecture -eq 'arm64') { 0xAA64 } else { 0x8664 }
    if ($machine -ne $expectedMachine) { throw "Binary architecture does not match $Architecture." }
} finally { $binaryReader.Dispose() }
if (-not $Publisher.StartsWith('CN=')) { throw 'Publisher must be the full certificate subject from Partner Center, starting with CN=.' }
$sdkRoot = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin'
$sdk = Get-ChildItem -LiteralPath $sdkRoot -Directory |
    Where-Object { $_.Name -match '^10\.0\.\d+\.0$' } |
    Sort-Object { [version]$_.Name } -Descending |
    Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'x64\makeappx.exe') } |
    Select-Object -First 1
if (-not $sdk) { throw 'Install the Windows 11 SDK with MakeAppx.' }
$stage = Join-Path $projectRoot ('build\package-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $stage -Force | Out-Null
Copy-Item -LiteralPath $binary -Destination (Join-Path $stage 'Screen.exe')
Copy-Item -LiteralPath (Join-Path $projectRoot 'packaging\Assets') -Destination $stage -Recurse
Copy-Item -LiteralPath (Join-Path $projectRoot 'LICENSE') -Destination $stage
[xml]$manifest = Get-Content -LiteralPath (Join-Path $projectRoot 'packaging\AppxManifest.xml') -Raw
$manifest.Package.Identity.Name = $IdentityName
$manifest.Package.Identity.Publisher = $Publisher
$manifest.Package.Identity.Version = $Version
$manifest.Package.Identity.ProcessorArchitecture = $Architecture
$manifest.Save((Join-Path $stage 'AppxManifest.xml'))
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$outputRoot = (Resolve-Path -LiteralPath $OutputDirectory).Path
$output = Join-Path $outputRoot "Screen-$Version-$Architecture.msix"
if (Test-Path -LiteralPath $output) { throw "Output already exists: $output" }
& (Join-Path $sdk.FullName 'x64\makeappx.exe') pack /d $stage /p $output
if ($LASTEXITCODE -ne 0) { throw "MakeAppx failed with code $LASTEXITCODE" }
Write-Output "Unsigned package: $output"
Write-Output 'Store submission requires the exact Partner Center identity. Local installation requires a trusted matching signature.'
