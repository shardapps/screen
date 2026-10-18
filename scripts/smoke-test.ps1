[CmdletBinding()]
param([Parameter(Mandatory)][string]$Executable)
$ErrorActionPreference = 'Stop'
# Exercises real windows without darkening the user's desktop. Settings are restored in finally.
Add-Type @'
using System;
using System.Runtime.InteropServices;
using System.Text;
public static class ScreenSmoke {
    public delegate bool EnumProc(IntPtr hwnd, IntPtr context);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string cls, string title);
    [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr hwnd, int id);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr hwnd, uint msg, IntPtr wp, IntPtr lp);
    [DllImport("user32.dll")] public static extern int GetWindowLong(IntPtr hwnd, int index);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc callback, IntPtr context);
    [DllImport("user32.dll")] public static extern bool GetLayeredWindowAttributes(IntPtr hwnd, out uint color, out byte alpha, out uint flags);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr hwnd, StringBuilder cls, int count);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint processId);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hwnd, out Rect bounds);
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left, Top, Right, Bottom; }
}
'@
$binary = (Resolve-Path -LiteralPath $Executable).Path
if ([ScreenSmoke]::FindWindow('Shardapps.Screen.Control', 'Screen by Shardapps') -ne [IntPtr]::Zero) {
    throw 'Close the running Screen app before running this isolated smoke test.'
}
$settingsFile = Join-Path $env:LOCALAPPDATA 'Shardapps\Screen\settings.ini'
$hadSettings = Test-Path -LiteralPath $settingsFile
$savedBytes = if ($hadSettings) { [IO.File]::ReadAllBytes($settingsFile) } else { $null }
$process = $null
try {
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
    # Wait for worker startup and the first monitor enumeration.
    Start-Sleep -Milliseconds 300
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
    foreach ($surface in $script:surfaces) {
        $style = [ScreenSmoke]::GetWindowLong($surface, -20)
        $required = 0x080800A8 # NOACTIVATE | LAYERED | TOOLWINDOW | TRANSPARENT | TOPMOST
        if (($style -band $required) -ne $required) { throw 'Overlay is missing a safety or input-transparency style.' }
        [uint32]$color = 0; [byte]$alpha = 255; [uint32]$flags = 0
        if (-not [ScreenSmoke]::GetLayeredWindowAttributes($surface, [ref]$color, [ref]$alpha, [ref]$flags)) { throw 'Cannot read overlay alpha.' }
        if ($alpha -ne 0 -or [ScreenSmoke]::IsWindowVisible($surface)) { throw 'Startup must be fully transparent.' }
    }
    # Set full brightness before activating; this exercises the command/worker path at zero opacity.
    [void][ScreenSmoke]::SendMessage($slider, 0x0405, [IntPtr]1, [IntPtr]100) # TBM_SETPOS
    [void][ScreenSmoke]::SendMessage($window, 0x0114, [IntPtr]8, $slider) # WM_HSCROLL/TB_ENDTRACK
    Start-Sleep -Milliseconds 350
    foreach ($surface in $script:surfaces) {
        [uint32]$color = 0; [byte]$alpha = 255; [uint32]$flags = 0
        [void][ScreenSmoke]::GetLayeredWindowAttributes($surface, [ref]$color, [ref]$alpha, [ref]$flags)
        if ($alpha -ne 0) { throw '100 percent brightness must remain transparent.' }
    }
    # Exercise the emergency message and close-to-tray behavior.
    [void][ScreenSmoke]::SendMessage($window, 0x0312, [IntPtr]4, [IntPtr]0)
    [void][ScreenSmoke]::SendMessage($window, 0x0010, [IntPtr]0, [IntPtr]0)
    if ($process.HasExited) { throw 'Closing the control window must keep the tray process alive.' }
    [void][ScreenSmoke]::SendMessage($window, 0x0111, [IntPtr]107, [IntPtr]0)
    if (-not $process.WaitForExit(5000)) { throw 'Quit did not stop both threads.' }
    if ($process.ExitCode -ne 0) { throw "Screen exited with $($process.ExitCode)." }
    Write-Output "PASS: $($script:surfaces.Count) monitor(s), safety styles, transparent startup, full brightness, restore, tray lifecycle, clean shutdown."
} finally {
    if ($process -and -not $process.HasExited) { Stop-Process -Id $process.Id -Force; $process.WaitForExit() }
    if ($hadSettings) { [IO.File]::WriteAllBytes($settingsFile, $savedBytes) }
    elseif (Test-Path -LiteralPath $settingsFile) { Remove-Item -LiteralPath $settingsFile }
}
