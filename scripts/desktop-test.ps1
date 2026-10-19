[CmdletBinding()]
param([Parameter(Mandatory)][string]$Executable)
$ErrorActionPreference = 'Stop'
Add-Type @'
using System;
using System.Runtime.InteropServices;
using System.Text;
public static class DesktopTest {
 public delegate bool EnumProc(IntPtr hwnd,IntPtr context);
 [StructLayout(LayoutKind.Sequential)] public struct Effect {[MarshalAs(UnmanagedType.ByValArray,SizeConst=25)]public float[] V;}
 [DllImport("Magnification.dll")]public static extern bool MagInitialize();
 [DllImport("Magnification.dll")]public static extern bool MagUninitialize();
 [DllImport("Magnification.dll")]public static extern bool MagGetFullscreenColorEffect(out Effect effect);
 [DllImport("Magnification.dll",SetLastError=true)]public static extern bool MagSetFullscreenColorEffect(ref Effect effect);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)]public static extern IntPtr FindWindow(string cls,string title);
 [DllImport("user32.dll")]public static extern IntPtr GetDlgItem(IntPtr hwnd,int id);
 [DllImport("user32.dll")]public static extern IntPtr SendMessage(IntPtr hwnd,uint message,IntPtr wp,IntPtr lp);
 [DllImport("user32.dll")]public static extern IntPtr GetDC(IntPtr hwnd);
 [DllImport("user32.dll")]public static extern int ReleaseDC(IntPtr hwnd,IntPtr dc);
 [DllImport("user32.dll")]public static extern int GetSystemMetrics(int metric);
 [DllImport("user32.dll")]public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
 [DllImport("gdi32.dll")]public static extern uint GetPixel(IntPtr dc,int x,int y);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)]public static extern int GetWindowText(IntPtr hwnd,StringBuilder text,int count);
 [DllImport("user32.dll")]public static extern bool EnumChildWindows(IntPtr hwnd,EnumProc callback,IntPtr context);
 public static bool OverlayStatus(IntPtr hwnd){bool found=false;EnumChildWindows(hwnd,(child,data)=>{var text=new StringBuilder(256);GetWindowText(child,text,256);found|=text.ToString().Contains("Overlay");return true;},IntPtr.Zero);return found;}
 public static uint Pixel(){var dc=GetDC(IntPtr.Zero);try{return GetPixel(dc,GetSystemMetrics(0)/2,GetSystemMetrics(1)/2);}finally{ReleaseDC(IntPtr.Zero,dc);}}
}
'@
$binary=(Resolve-Path -LiteralPath $Executable).Path
if([DesktopTest]::FindWindow('Shardapps.Screen.Control','Screen Dimmer by Shardapps') -ne [IntPtr]::Zero){throw 'Close Screen before the isolated desktop test.'}
if(-not [DesktopTest]::MagInitialize()){Write-Output 'SKIP: desktop color API unavailable in this session; the separate overlay test validates compatibility rendering.';return}
$original=[DesktopTest+Effect]::new()
if(-not [DesktopTest]::MagGetFullscreenColorEffect([ref]$original)){
 [void][DesktopTest]::MagUninitialize()
 Write-Output 'SKIP: desktop color API cannot read the current session; the separate overlay test validates compatibility rendering.';return
}
$settingsFile=Join-Path $env:LOCALAPPDATA 'Shardapps/Screen/settings.ini'
$hadSettings=Test-Path -LiteralPath $settingsFile
$savedBytes=if($hadSettings){[IO.File]::ReadAllBytes($settingsFile)}else{$null}
$process=$null
function Send([IntPtr]$target,[uint32]$message,[IntPtr]$wp=[IntPtr]::Zero,[IntPtr]$lp=[IntPtr]::Zero){[void][DesktopTest]::SendMessage($target,$message,$wp,$lp)}
function Read-Effect {
 $effect=[DesktopTest+Effect]::new()
 if(-not [DesktopTest]::MagGetFullscreenColorEffect([ref]$effect)){throw 'Cannot read desktop matrix'}
 return $effect
}
function Assert-Effect($expected){
 $actual=Read-Effect
 for($index=0;$index -lt 25;$index++){
  if([Math]::Abs($actual.V[$index]-$expected.V[$index]) -gt 0.0001){throw "Unexpected desktop matrix at $index`: $($actual.V[$index]), expected $($expected.V[$index])"}
 }
}
function Expected-Dim([int]$level,[bool]$warm=$false,$base=$original){
 $effect=[DesktopTest+Effect]::new();$effect.V=[float[]]$base.V.Clone()
 $amount=[Math]::Round((100-$level)*255.0/100,[MidpointRounding]::AwayFromZero)/255.0
 for($row=0;$row -lt 5;$row++){for($column=0;$column -lt 3;$column++){
  $offset=if($warm){@(66,30,8)[$column]/255.0*$amount}else{0}
  $effect.V[$row*5+$column]=$base.V[$row*5+$column]*(1-$amount)+$base.V[$row*5+4]*$offset
 }}
 return $effect
}
function Start-Screen {
 $script:process=Start-Process -FilePath $binary -ArgumentList '--background' -WindowStyle Hidden -PassThru
 $script:window=[IntPtr]::Zero
 for($attempt=0;$attempt -lt 100 -and $script:window -eq [IntPtr]::Zero;$attempt++){
  Start-Sleep -Milliseconds 50
  $script:window=[DesktopTest]::FindWindow('Shardapps.Screen.Control','Screen Dimmer by Shardapps')
  if($script:process.HasExited){throw 'Screen exited at startup'}
 }
 if($script:window -eq [IntPtr]::Zero){throw 'Missing Screen control window'}
 $script:slider=[DesktopTest]::GetDlgItem($script:window,101)
 $script:engine=[DesktopTest]::FindWindow('Shardapps.Screen.Engine','Screen engine')
 Start-Sleep -Milliseconds 250
}
function Set-Level([int]$level,[bool]$tracking=$false){
 Send $slider 0x0405 1 $level
 Send $window 0x0114 $(if($tracking){5}else{8}) $slider
}
function Quit-Screen {
 Send $window 0x0111 107
 if(-not $process.WaitForExit(5000) -or $process.ExitCode -ne 0){throw 'Unclean Screen exit'}
}
try{
 [void][DesktopTest]::SetThreadDpiAwarenessContext([IntPtr](-4))
 # Uniform deterministic settings; restore the user's exact file in finally.
 [void][IO.Directory]::CreateDirectory((Split-Path -Parent $settingsFile))
 [IO.File]::WriteAllText($settingsFile,"brightness=70`nwarm=0`nsmooth=1`n")
 Start-Screen;Assert-Effect $original
 $beforePixel=[DesktopTest]::Pixel()
 Set-Level 50;Start-Sleep -Milliseconds 500;Assert-Effect (Expected-Dim 50)
 $dimmedPixel=[DesktopTest]::Pixel()
 Write-Output ('Desktop center sample before=0x{0:X6}, at 50%=0x{1:X6} (visible-content diagnostic, not an assertion)' -f $beforePixel,$dimmedPixel)
 # Both topology and power resume must reapply the desired desktop transform.
 Send $engine 0x007E;Start-Sleep -Milliseconds 100;Assert-Effect (Expected-Dim 50)
 Send $engine 0x0218 0x12;Start-Sleep -Milliseconds 100;Assert-Effect (Expected-Dim 50)
 Send $window 0x0111 104;Start-Sleep -Milliseconds 100;Assert-Effect (Expected-Dim 50 $true)
 Send $window 0x0111 104
 for($step=0;$step -lt 50;$step++){Set-Level (40+$step%35) $true;Start-Sleep -Milliseconds 2}
 Set-Level 8;Start-Sleep -Milliseconds 500;Assert-Effect (Expected-Dim 8)
 # Lock restores immediately; unlock reapplies the bounded preference.
 Send $engine 0x02B1 7;Start-Sleep -Milliseconds 100;Assert-Effect $original
 Send $engine 0x02B1 8;Start-Sleep -Milliseconds 500;Assert-Effect (Expected-Dim 8)
 Send $window 0x0312 4;Start-Sleep -Milliseconds 100;Assert-Effect $original
 Set-Level 60;Start-Sleep -Milliseconds 500;Assert-Effect (Expected-Dim 60)
 Quit-Screen;Start-Sleep -Milliseconds 100;Assert-Effect $original
 # Windows must remove this process's transform even on forced termination.
 Start-Screen;Set-Level 60;Start-Sleep -Milliseconds 500;Assert-Effect (Expected-Dim 60)
 Stop-Process -Id $process.Id -Force;$process.WaitForExit()
 Start-Sleep -Milliseconds 200;Assert-Effect $original
 # Windows may reserve the color writer for another accessibility app. Respect
 # that client and use the overlay fallback instead of replacing its filter.
 $external=[DesktopTest+Effect]::new();$external.V=[float[]]$original.V.Clone();$external.V[0]*=0.98
 if(-not [DesktopTest]::MagSetFullscreenColorEffect([ref]$external)){throw "Cannot install ownership regression filter: $([Runtime.InteropServices.Marshal]::GetLastWin32Error())"}
 Start-Screen;Set-Level 60;Start-Sleep -Milliseconds 500
 if([DesktopTest]::OverlayStatus($window)){Assert-Effect $external}
 else{Assert-Effect (Expected-Dim 60 $false $external)}
 Quit-Screen;Assert-Effect $external
 [void][DesktopTest]::MagSetFullscreenColorEffect([ref]$original)
 Write-Output 'PASS: desktop matrix dimming, warm tint, rapid retargeting, 8% floor, display/resume, lock/unlock, emergency restore, graceful/forced exit and busy-color-API fallback.'
}finally{
 if($process -and -not $process.HasExited){Stop-Process -Id $process.Id -Force;$process.WaitForExit()}
 [void][DesktopTest]::MagSetFullscreenColorEffect([ref]$original)
 [void][DesktopTest]::MagUninitialize()
 if($hadSettings){[IO.File]::WriteAllBytes($settingsFile,$savedBytes)}elseif(Test-Path -LiteralPath $settingsFile){Remove-Item -LiteralPath $settingsFile}
}
