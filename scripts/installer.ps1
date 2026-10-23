[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet('x64', 'arm64')][string]$Architecture,
    [Parameter(Mandatory)][string]$Executable,
    [ValidatePattern('^\d+\.\d+\.\d+$')][string]$Version = '1.3.1',
    [string]$OutputDirectory = 'dist',
    [string]$InnoCompiler
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$binary = (Resolve-Path -LiteralPath $Executable).Path
$reader = [IO.BinaryReader]::new([IO.File]::OpenRead($binary))
try {
    if ($reader.ReadUInt16() -ne 0x5A4D) { throw 'Executable is not a Windows PE file.' }
    [void]$reader.BaseStream.Seek(0x3C, [IO.SeekOrigin]::Begin)
    $offset = $reader.ReadInt32()
    [void]$reader.BaseStream.Seek($offset, [IO.SeekOrigin]::Begin)
    if ($reader.ReadUInt32() -ne 0x00004550) { throw 'Invalid PE signature.' }
    $expected = if ($Architecture -eq 'arm64') { 0xAA64 } else { 0x8664 }
    if ($reader.ReadUInt16() -ne $expected) { throw "Binary architecture does not match $Architecture." }
} finally { $reader.Dispose() }
if (-not $InnoCompiler) {
    $command = Get-Command ISCC.exe -ErrorAction SilentlyContinue
    $candidates = @(
        $(if ($command) { $command.Source }),
        "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
        "$env:ProgramFiles\Inno Setup 6\ISCC.exe",
        "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe"
    )
    $InnoCompiler = $candidates | Where-Object { $_ -and (Test-Path -LiteralPath $_) } | Select-Object -First 1
}
if (-not $InnoCompiler) { throw 'Install Inno Setup 6.3+ or pass -InnoCompiler with the ISCC.exe path.' }
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$outputRoot = (Resolve-Path -LiteralPath $OutputDirectory).Path
$output = Join-Path $outputRoot "Screen-Setup-$Version-$Architecture.exe"
if (Test-Path -LiteralPath $output) { throw "Output already exists: $output" }
& $InnoCompiler "/DAppBinary=$binary" "/DAppVersion=$Version" "/DTargetArchitecture=$Architecture" "/O$outputRoot" (Join-Path $projectRoot 'packaging\Screen.iss')
if ($LASTEXITCODE -ne 0) { throw "Installer compilation failed: $LASTEXITCODE" }
Write-Output "Per-user setup EXE: $output"
Write-Output 'Unsigned preview: Windows may show an unknown publisher warning. No certificate import or administrator privileges are required.'
