[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet('x64', 'arm64')][string]$Architecture,
    [Parameter(Mandatory)][string]$BuildDirectory
)
$ErrorActionPreference = 'Stop'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Install Visual Studio with Desktop development with C++.' }
$installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -format json | ConvertFrom-Json
if (-not $installation) { throw 'No Visual Studio C++ installation was found.' }
$major = ([version]$installation.installationVersion).Major
$generator = switch ($major) { 18 { 'Visual Studio 18 2026' } 17 { 'Visual Studio 17 2022' } default { throw "Unsupported Visual Studio major: $major" } }
$cmake = Join-Path $installation.installationPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
if (-not (Test-Path -LiteralPath $cmake)) { $cmake = (Get-Command cmake -ErrorAction Stop).Source }
$platform = if ($Architecture -eq 'arm64') { 'ARM64' } else { 'x64' }
Write-Output "Configuring $Architecture with $generator using $cmake"
& $cmake -S (Split-Path -Parent $PSScriptRoot) -B $BuildDirectory -G $generator -A $platform
if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed: $LASTEXITCODE" }
