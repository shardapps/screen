[CmdletBinding()]
param([Parameter(Mandatory)][string]$Installer)
$ErrorActionPreference = 'Stop'
$setup = (Resolve-Path -LiteralPath $Installer).Path
$appKey = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\{A373290A-4074-4C94-8C9E-F8844D3EE424}_is1'
if (Test-Path -LiteralPath $appKey) { throw 'Screen is already installed. Use a clean user profile for installer tests.' }
if (Get-Process Screen -ErrorAction SilentlyContinue) { throw 'Close Screen before running installer tests.' }
$projectRoot = Split-Path -Parent $PSScriptRoot
$testId = [guid]::NewGuid().ToString('N')
$installRoot = Join-Path $projectRoot "build\installer-test-$testId"
$group = "Shardapps Screen Test $testId"
$shortcut = Join-Path ([Environment]::GetFolderPath('Programs')) "$group\Screen by Shardapps.lnk"
$uninstaller = Join-Path $installRoot 'unins000.exe'
function Invoke-Setup {
    $arguments = @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/SP-', ('/DIR="' + $installRoot + '"'), ('/GROUP="' + $group + '"'))
    $process = Start-Process -FilePath $setup -ArgumentList $arguments -WindowStyle Hidden -PassThru
    if (-not $process.WaitForExit(60000)) { Stop-Process -Id $process.Id -Force; throw 'Setup timed out.' }
    if ($process.ExitCode -ne 0) { throw "Setup failed: $($process.ExitCode)" }
}
try {
    Invoke-Setup
    $installedExe = Join-Path $installRoot 'Screen.exe'
    if (-not (Test-Path -LiteralPath $installedExe)) { throw 'Installed executable is missing.' }
    if (-not (Test-Path -LiteralPath $shortcut)) { throw 'Start menu shortcut is missing.' }
    if (-not (Test-Path -LiteralPath $appKey)) { throw 'Per-user uninstall registration is missing.' }
    $shell = New-Object -ComObject WScript.Shell
    if ($shell.CreateShortcut($shortcut).TargetPath -ne $installedExe) { throw 'Start menu target is incorrect.' }
    & (Join-Path $PSScriptRoot 'smoke-test.ps1') -Executable $installedExe
    & (Join-Path $PSScriptRoot 'desktop-test.ps1') -Executable $installedExe
    # Reinstall exercises the same-identity upgrade path without creating another app entry.
    Invoke-Setup
} finally {
    if (Test-Path -LiteralPath $uninstaller) {
        $uninstall = Start-Process -FilePath $uninstaller -ArgumentList '/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART' -WindowStyle Hidden -PassThru
        if (-not $uninstall.WaitForExit(60000)) { throw 'Uninstall timed out.' }
        if ($uninstall.ExitCode -ne 0) { throw "Uninstall failed: $($uninstall.ExitCode)" }
    }
}
if (Test-Path -LiteralPath (Join-Path $installRoot 'Screen.exe')) { throw 'Uninstall left the executable behind.' }
if (Test-Path -LiteralPath $shortcut) { throw 'Uninstall left the Start menu shortcut behind.' }
if (Test-Path -LiteralPath $appKey) { throw 'Uninstall left the per-user app registration behind.' }
Write-Output 'PASS: per-user installation, Start menu shortcut, uninstall registration, installed app behavior, reinstall and removal.'
