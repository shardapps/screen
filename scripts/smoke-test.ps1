[CmdletBinding()]
param([Parameter(Mandatory)][string]$Executable)
$ErrorActionPreference = 'Stop'
# Real-window tests. Nonzero opacity is exercised only on tiny offscreen surfaces.
Add-Type @'
using System;
using System.Runtime.InteropServices;
using System.Text;
public static class ScreenSmoke {
    public delegate bool EnumProc(IntPtr hwnd, IntPtr context);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string cls, string title);
    [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr hwnd, int id);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr hwnd, uint msg, IntPtr wp, IntPtr lp);
    [DllImport("user32.dll")] public static extern IntPtr GetWindow(IntPtr hwnd, uint relation);
    [DllImport("user32.dll")] public static extern int GetWindowLong(IntPtr hwnd, int index);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc callback, IntPtr context);
    [DllImport("user32.dll")] public static extern bool GetLayeredWindowAttributes(IntPtr hwnd, out uint color, out byte alpha, out uint flags);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr hwnd, StringBuilder cls, int count);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hwnd, out Rect bounds);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr hwnd, IntPtr after, int x, int y, int cx, int cy, uint flags);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr CreateWindowEx(uint ex,string cls,string title,uint style,int x,int y,int cx,int cy,IntPtr parent,IntPtr menu,IntPtr instance,IntPtr data);
    [DllImport("user32.dll")] public static extern bool DestroyWindow(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern uint GetDpiForWindow(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
    [DllImport("user32.dll")] public static extern IntPtr MonitorFromWindow(IntPtr hwnd,uint flags);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern bool GetMonitorInfo(IntPtr monitor,ref MonitorInfo info);
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left, Top, Right, Bottom; }
    [StructLayout(LayoutKind.Sequential)] public struct MonitorInfo { public uint Size; public Rect Monitor, Work; public uint Flags; }
}
'@
$binary = (Resolve-Path -LiteralPath $Executable).Path
if ([ScreenSmoke]::FindWindow('Shardapps.Screen.Control', 'Screen by Shardapps') -ne [IntPtr]::Zero) {
    throw 'Close the running Screen app before running this isolated smoke test.'
}
$settingsFile = Join-Path $env:LOCALAPPDATA 'Shardapps\Screen\settings.ini'
$hadSettings = Test-Path -LiteralPath $settingsFile
$savedBytes = if ($hadSettings) { [IO.File]::ReadAllBytes($settingsFile) } else { $null }
$process = $null; $foreign = [IntPtr]::Zero
function Get-Surfaces {
    $script:surfaces = [Collections.Generic.List[IntPtr]]::new()
    $callback = [ScreenSmoke+EnumProc] {
        param($hwnd, $context)
        $className = [Text.StringBuilder]::new(256)
        [void][ScreenSmoke]::GetClassName($hwnd, $className, 256)
        if ($className.ToString() -eq 'Shardapps.Screen.Overlay') { $script:surfaces.Add($hwnd) }
        return $true
    }
    [void][ScreenSmoke]::EnumWindows($callback, [IntPtr]::Zero)
    if ($script:surfaces.Count -lt 1) { throw 'No monitor overlays were created.' }
}
function Assert-Alpha([int]$expected) {
    foreach ($surface in $script:surfaces) {
        [uint32]$color = 0; [byte]$alpha = 255; [uint32]$flags = 0
        if (-not [ScreenSmoke]::GetLayeredWindowAttributes($surface, [ref]$color, [ref]$alpha, [ref]$flags)) { throw 'Cannot read overlay alpha.' }
        if ($alpha -ne $expected) { throw "Expected alpha $expected, got $alpha." }
        if ($expected -eq 0 -and [ScreenSmoke]::IsWindowVisible($surface)) { throw 'Zero-opacity overlays must be hidden.' }
        if ($expected -gt 0 -and -not [ScreenSmoke]::IsWindowVisible($surface)) { throw 'Active overlay must be visible.' }
    }
}
function Set-Level([int]$level, [bool]$tracking = $false) {
    [void][ScreenSmoke]::SendMessage($slider, 0x0405, [IntPtr]1, [IntPtr]$level)
    [void][ScreenSmoke]::SendMessage($window, 0x0114, [IntPtr]$(if ($tracking) {5} else {8}), $slider)
}
function Move-Offscreen {
    foreach ($surface in $script:surfaces) {
        if (-not [ScreenSmoke]::SetWindowPos($surface, [IntPtr](-1), -32000, -32000, 16, 16, 0x0010)) { throw 'Cannot isolate test surface.' }
    }
}
function Assert-Above([IntPtr]$surface, [IntPtr]$other) {
    $above = [ScreenSmoke]::GetWindow($surface, 3) # GW_HWNDPREV
    for ($i = 0; $above -ne [IntPtr]::Zero -and $i -lt 512; $i++) {
        if ($above -eq $other) { throw 'A foreign topmost window is still above the dimming surface.' }
        $above = [ScreenSmoke]::GetWindow($above, 3)
    }
}
try {
    [void][ScreenSmoke]::SetThreadDpiAwarenessContext([IntPtr](-4))
    $process = Start-Process -FilePath $binary -ArgumentList '--background' -WindowStyle Hidden -PassThru
    $window = [IntPtr]::Zero
    for ($i = 0; $i -lt 100 -and $window -eq [IntPtr]::Zero; $i++) {
        Start-Sleep -Milliseconds 100
        $window = [ScreenSmoke]::FindWindow('Shardapps.Screen.Control', 'Screen by Shardapps')
        if ($process.HasExited) { throw 'Screen exited before creating its control window.' }
    }
    if ($window -eq [IntPtr]::Zero) { throw 'No control window within 10 seconds.' }
    $slider = [ScreenSmoke]::GetDlgItem($window, 101)
    if ($slider -eq [IntPtr]::Zero) { throw 'Brightness slider missing.' }
    $engine = [ScreenSmoke]::FindWindow('Shardapps.Screen.Engine', 'Screen engine')
    if ($engine -eq [IntPtr]::Zero) { throw 'Worker notification window missing.' }
    Start-Sleep -Milliseconds 250
    Get-Surfaces; Assert-Alpha 0
    foreach ($surface in $script:surfaces) {
        $style = [ScreenSmoke]::GetWindowLong($surface, -20)
        $required = 0x080800A8
        if (($style -band $required) -ne $required) { throw 'Missing input-transparency / no-activation / topmost style.' }
        if ([ScreenSmoke]::SendMessage($surface, 0x0084, [IntPtr]0, [IntPtr]0).ToInt64() -ne -1) { throw 'Overlay must return HTTRANSPARENT.' }
    }
    Set-Level 100; Start-Sleep -Milliseconds 350; Assert-Alpha 0
    # Repeated topology/resume notifications must preserve state and create valid surfaces.
    foreach ($notification in @(0x007E, 0x0218)) {
        [void][ScreenSmoke]::SendMessage($engine, $notification, [IntPtr]0x12, [IntPtr]0)
        Get-Surfaces; Assert-Alpha 0
    }
    # Recover a destroyed overlay without a stale invalid handle disabling dimming.
    [void][ScreenSmoke]::SendMessage($script:surfaces[0], 0x0010, [IntPtr]0, [IntPtr]0)
    Set-Level 100; Start-Sleep -Milliseconds 100; Get-Surfaces; Assert-Alpha 0
    Move-Offscreen
    Set-Level 8; Start-Sleep -Milliseconds 400; Assert-Alpha 235
    # A different process creates/promotes an intersecting topmost window.
    $foreign = [ScreenSmoke]::CreateWindowEx(0x08000088, 'STATIC', 'Screen coverage test', 2147483648,
        -32000, -32000, 16, 16, [IntPtr]::Zero, [IntPtr]::Zero, [IntPtr]::Zero, [IntPtr]::Zero)
    if ($foreign -eq [IntPtr]::Zero) { throw 'Cannot create coverage regression window.' }
    for ($round = 0; $round -lt 5; $round++) {
        [void][ScreenSmoke]::SetWindowPos($foreign, [IntPtr](-1), -32000, -32000, 16, 16, 0x0050)
        Start-Sleep -Milliseconds 400
        foreach ($surface in $script:surfaces) { Assert-Above $surface $foreign }
    }
    # Lost topmost state must self-repair through the active-only coverage audit.
    foreach ($surface in $script:surfaces) {
        [void][ScreenSmoke]::SetWindowPos($surface, [IntPtr](-2), 0, 0, 0, 0, 0x0013)
    }
    Start-Sleep -Milliseconds 400
    foreach ($surface in $script:surfaces) {
        if (([ScreenSmoke]::GetWindowLong($surface, -20) -band 8) -eq 0) { throw 'Engine did not restore topmost state.' }
        Assert-Above $surface $foreign
    }
    Assert-Alpha 235
    # High-frequency input cannot repeatedly restart a slow ease-in and freeze motion.
    for ($inputStep = 0; $inputStep -lt 80; $inputStep++) {
        Set-Level (40 + $inputStep % 35) $true
        Start-Sleep -Milliseconds 2
    }
    Set-Level 60; Start-Sleep -Milliseconds 400; Assert-Alpha 102
    [void][ScreenSmoke]::SendMessage($window, 0x0312, [IntPtr]4, [IntPtr]0)
    Start-Sleep -Milliseconds 100; Assert-Alpha 0
    # Open the new mini panel, verify bounded dimensions and work-area anchoring.
    [void][ScreenSmoke]::SendMessage($window, 0x0312, [IntPtr]5, [IntPtr]0)
    Start-Sleep -Milliseconds 250
    if (-not [ScreenSmoke]::IsWindowVisible($window)) { throw 'Open shortcut did not show the flyout.' }
    $bounds = [ScreenSmoke+Rect]::new(); [void][ScreenSmoke]::GetWindowRect($window, [ref]$bounds)
    $scale = [ScreenSmoke]::GetDpiForWindow($window) / 96.0
    if (($bounds.Right-$bounds.Left) -gt 340*$scale -or ($bounds.Bottom-$bounds.Top) -gt 150*$scale) { throw 'Flyout is not compact.' }
    $monitor = [ScreenSmoke+MonitorInfo]::new(); $monitor.Size = [Runtime.InteropServices.Marshal]::SizeOf($monitor)
    [void][ScreenSmoke]::GetMonitorInfo([ScreenSmoke]::MonitorFromWindow($window,2),[ref]$monitor)
    if ($bounds.Right -gt $monitor.Work.Right -or $bounds.Bottom -gt $monitor.Work.Bottom -or
        ($monitor.Work.Right-$bounds.Right) -gt 24*$scale -or ($monitor.Work.Bottom-$bounds.Bottom) -gt 24*$scale) { throw 'Flyout is not anchored above the taskbar at bottom right.' }
    [void][ScreenSmoke]::SendMessage($window, 0x0006, [IntPtr]0, [IntPtr]0)
    Start-Sleep -Milliseconds 150
    if ([ScreenSmoke]::IsWindowVisible($window)) { throw 'Flyout did not dismiss after losing focus.' }
    [void][ScreenSmoke]::SendMessage($window, 0x0010, [IntPtr]0, [IntPtr]0)
    if ($process.HasExited) { throw 'Dismissing the flyout must leave the tray process alive.' }
    [void][ScreenSmoke]::SendMessage($window, 0x0111, [IntPtr]107, [IntPtr]0)
    if (-not $process.WaitForExit(5000) -or $process.ExitCode -ne 0) { throw 'Unclean shutdown.' }
    Write-Output 'PASS: 8% floor, rapid retargeting, topmost repair, display/resume recovery, compact anchored flyout, dismissal, restore and tray lifecycle.'
} finally {
    if ($foreign -ne [IntPtr]::Zero) { [void][ScreenSmoke]::DestroyWindow($foreign) }
    if ($process -and -not $process.HasExited) { Stop-Process -Id $process.Id -Force; $process.WaitForExit() }
    if ($hadSettings) { [IO.File]::WriteAllBytes($settingsFile, $savedBytes) }
    elseif (Test-Path -LiteralPath $settingsFile) { Remove-Item -LiteralPath $settingsFile }
}
