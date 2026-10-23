[CmdletBinding()]
param([Parameter(Mandatory)][string]$Executable,[string]$CaptureDirectory,[switch]$StoreScreenshots)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;using System.Text;using System.Runtime.InteropServices;
public static class PreferencesTest {
 public static string FileText(string path){using(var stream=new System.IO.FileStream(path,System.IO.FileMode.Open,System.IO.FileAccess.Read,System.IO.FileShare.ReadWrite|System.IO.FileShare.Delete))using(var reader=new System.IO.StreamReader(stream)){return reader.ReadToEnd();}}
 public delegate bool EnumProc(IntPtr hwnd,IntPtr data);
 [StructLayout(LayoutKind.Sequential)]public struct Rect{public int Left,Top,Right,Bottom;}
 [StructLayout(LayoutKind.Sequential)]public struct Effect{[MarshalAs(UnmanagedType.ByValArray,SizeConst=25)]public float[] V;}
 [DllImport("user32.dll",CharSet=CharSet.Unicode)]public static extern IntPtr FindWindow(string cls,string title);
 [DllImport("user32.dll")]public static extern IntPtr GetDlgItem(IntPtr hwnd,int id);
 [DllImport("user32.dll")]public static extern IntPtr SendMessage(IntPtr hwnd,uint message,IntPtr wp,IntPtr lp);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)]public static extern int GetWindowText(IntPtr hwnd,StringBuilder text,int count);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)]public static extern int GetClassName(IntPtr hwnd,StringBuilder text,int count);
 [DllImport("user32.dll")]public static extern bool EnumWindows(EnumProc callback,IntPtr data);
 [DllImport("user32.dll")]public static extern bool GetWindowRect(IntPtr hwnd,out Rect rect);
 [DllImport("user32.dll",EntryPoint="GetWindowLongW")]public static extern int GetWindowLong(IntPtr hwnd,int index);
 [DllImport("user32.dll")]public static extern bool IsWindowVisible(IntPtr hwnd);
 [DllImport("user32.dll")]public static extern bool ShowWindow(IntPtr hwnd,int command);
 [DllImport("user32.dll")]public static extern bool IsWindow(IntPtr hwnd);
 [DllImport("user32.dll")]public static extern bool IsWindowEnabled(IntPtr hwnd);
 [DllImport("user32.dll")]public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")]public static extern bool SetWindowPos(IntPtr hwnd,IntPtr after,int x,int y,int w,int h,uint flags);
 [DllImport("user32.dll")]public static extern bool GetLayeredWindowAttributes(IntPtr hwnd,out uint color,out byte alpha,out uint flags);
 [DllImport("user32.dll")]public static extern uint GetGuiResources(IntPtr process,uint flags);
 [DllImport("user32.dll")]public static extern uint GetWindowThreadProcessId(IntPtr hwnd,out uint process);
 [DllImport("user32.dll")]public static extern bool PrintWindow(IntPtr hwnd,IntPtr dc,uint flags);
 [DllImport("user32.dll")]public static extern bool RedrawWindow(IntPtr hwnd,IntPtr rect,IntPtr region,uint flags);
 [DllImport("dwmapi.dll")]public static extern int DwmFlush();
 [DllImport("user32.dll")]public static extern IntPtr GetWindowDC(IntPtr hwnd);
 [DllImport("user32.dll")]public static extern int ReleaseDC(IntPtr hwnd,IntPtr dc);
 [DllImport("gdi32.dll")]public static extern bool BitBlt(IntPtr dst,int x,int y,int w,int h,IntPtr src,int sx,int sy,uint operation);
 [DllImport("user32.dll")]public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
 [DllImport("Magnification.dll")]public static extern bool MagInitialize();
 [DllImport("Magnification.dll")]public static extern bool MagUninitialize();
 [DllImport("Magnification.dll")]public static extern bool MagGetFullscreenColorEffect(out Effect effect);
}
'@
$binary=(Resolve-Path -LiteralPath $Executable).Path
if([PreferencesTest]::FindWindow('Shardapps.Screen.Control','Screen Dimmer by Shardapps') -ne [IntPtr]::Zero){throw 'Close Screen before isolated settings tests.'}
$settingsFile=Join-Path $env:LOCALAPPDATA 'Shardapps/Screen/settings.ini'
$hadSettings=Test-Path -LiteralPath $settingsFile
$savedBytes=if($hadSettings){[IO.File]::ReadAllBytes($settingsFile)}else{$null}
$runPath='Software\Microsoft\Windows\CurrentVersion\Run'
$runName='ScreenByShardapps'
$runKey=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($runPath)
$oldRun=if($runKey){$runKey.GetValue($runName,$null,[Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)}else{$null}
$oldKind=if($null -ne $oldRun){$runKey.GetValueKind($runName)}else{$null}
if($runKey){$runKey.Dispose()}
$process=$null
function Send([IntPtr]$target,[uint32]$message,[IntPtr]$wp=[IntPtr]::Zero,[IntPtr]$lp=[IntPtr]::Zero){[void][PreferencesTest]::SendMessage($target,$message,$wp,$lp)}
function Read-Text([IntPtr]$target){$text=[Text.StringBuilder]::new(512);[void][PreferencesTest]::GetWindowText($target,$text,512);return $text.ToString()}
function Wait-SettingsFile([string]$line){
 for($attempt=0;$attempt -lt 60;$attempt++){
  if(([PreferencesTest]::FileText($settingsFile) -split "`r?`n") -contains $line){return}
  Start-Sleep -Milliseconds 50
 }
 throw "Preference was not persisted: $line; exited=$($process.HasExited), window=$([PreferencesTest]::IsWindow($preferences)); UI=$(Read-Text ([PreferencesTest]::GetDlgItem($preferences,1012))); status=$(Read-Text ([PreferencesTest]::GetDlgItem($window,110))); settings=$([PreferencesTest]::FileText($settingsFile))"
}
function Read-Run {
 $key=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($runPath)
 try{if($key){return $key.GetValue($runName,$null,[Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)}}finally{if($key){$key.Dispose()}}
 return $null
}
function Restore-Run {
 $key=[Microsoft.Win32.Registry]::CurrentUser.CreateSubKey($runPath)
 try{if($null -eq $oldRun){$key.DeleteValue($runName,$false)}else{$key.SetValue($runName,$oldRun,$oldKind)}}finally{$key.Dispose()}
}
function Start-Screen([bool]$paused=$true){
 $arguments=@('--background','--overlay');if($paused){$arguments+='--paused'}
 $script:process=Start-Process -FilePath $binary -ArgumentList $arguments -WindowStyle Hidden -PassThru
 $script:window=[IntPtr]::Zero
 for($attempt=0;$attempt -lt 100 -and $script:window -eq [IntPtr]::Zero;$attempt++){
  Start-Sleep -Milliseconds 50;$script:window=[PreferencesTest]::FindWindow('Shardapps.Screen.Control','Screen Dimmer by Shardapps')
  if($script:process.HasExited){throw 'Screen exited at startup'}
 }
 if($script:window -eq [IntPtr]::Zero){throw 'No control window'}
 Start-Sleep -Milliseconds 250
}
function Open-Settings {
 Send $window 0x0111 111
 $script:preferences=[PreferencesTest]::FindWindow('Shardapps.Screen.Settings','Screen Dimmer settings')
 if($script:preferences -eq [IntPtr]::Zero){throw 'Settings did not open'}
 if($StoreScreenshots){
  # Capture the actual rendered window at Store dimensions on the isolated CI desktop.
  $captureRect=[PreferencesTest+Rect]::new();$captureRect.Right=1440;$captureRect.Bottom=1040
  $capturePointer=[Runtime.InteropServices.Marshal]::AllocHGlobal(16)
  try{
   [Runtime.InteropServices.Marshal]::StructureToPtr($captureRect,$capturePointer,$false)
   # Render fonts and controls natively at 150%, fitting every page on the
   # hosted runner's supported 1080p display rather than enlarging a bitmap.
   Send $preferences 0x02E0 (144 -bor (144 -shl 16)) $capturePointer
  }finally{[Runtime.InteropServices.Marshal]::FreeHGlobal($capturePointer)}
 }
 Start-Sleep -Milliseconds 100
}
function Quit-Screen {
 $null=$process.Handle
 Send $window 0x0111 107
 if(-not $process.WaitForExit(5000)){throw 'Shutdown timed out'}
 if($process.ExitCode -ne 0){throw "Unclean exit: $($process.ExitCode); executable=$($process.Path)"}
}
function Isolate-Overlays {
 $callback=[PreferencesTest+EnumProc]{param($hwnd,$data)
  $cls=[Text.StringBuilder]::new(128);[void][PreferencesTest]::GetClassName($hwnd,$cls,128)
  if($cls.ToString() -eq 'Shardapps.Screen.Overlay'){[void][PreferencesTest]::SetWindowPos($hwnd,[IntPtr](-1),-32000,-32000,16,16,0x0010)}
  return $true
 }
 [void][PreferencesTest]::EnumWindows($callback,[IntPtr]::Zero)
}
function Capture([string]$name,[IntPtr]$target=$preferences,[bool]$raw=$false){
 if(-not $CaptureDirectory){return}
 [void][IO.Directory]::CreateDirectory($CaptureDirectory)
 # Start-Process hides the first ShowWindow call; explicitly show the capture target.
 [void][PreferencesTest]::ShowWindow($target,4)
 if(-not [PreferencesTest]::IsWindowVisible($target)){throw 'Capture target is hidden'}
 Start-Sleep -Milliseconds 200
 [void][PreferencesTest]::RedrawWindow($target,[IntPtr]::Zero,[IntPtr]::Zero,0x0585)
 [void][PreferencesTest]::DwmFlush()
 $rect=[PreferencesTest+Rect]::new();[void][PreferencesTest]::GetWindowRect($target,[ref]$rect)
 $bitmap=[Drawing.Bitmap]::new($rect.Right-$rect.Left,$rect.Bottom-$rect.Top)
 $graphics=[Drawing.Graphics]::FromImage($bitmap);$dc=$graphics.GetHdc()
 try{
  if(-not [PreferencesTest]::PrintWindow($target,$dc,2)){
   $source=[PreferencesTest]::GetWindowDC($target)
   try{if(-not [PreferencesTest]::BitBlt($dc,0,0,$bitmap.Width,$bitmap.Height,$source,0,0,0x00CC0020)){throw 'Settings capture failed'}}
   finally{[void][PreferencesTest]::ReleaseDC($target,$source)}
  }
 }finally{$graphics.ReleaseHdc($dc);$graphics.Dispose()}
 try{
  if($StoreScreenshots -and -not $raw -and ($bitmap.Width -lt 1366 -or $bitmap.Height -lt 768)){throw "Store screenshot is too small: $($bitmap.Width)x$($bitmap.Height)"}
  $colors=[Collections.Generic.HashSet[int]]::new()
  $stride=if($raw){3}else{17}
  for($y=0;$y -lt $bitmap.Height;$y+=$stride){for($x=0;$x -lt $bitmap.Width;$x+=$stride){[void]$colors.Add($bitmap.GetPixel($x,$y).ToArgb())}}
  if($colors.Count -lt 12){throw "Capture appears blank: $name ($($colors.Count) colors)"}
  $bitmap.Save((Join-Path $CaptureDirectory $name),[Drawing.Imaging.ImageFormat]::Png)
 }finally{$bitmap.Dispose()}
}
function Capture-Slider([string]$name){
 if(-not $StoreScreenshots -or -not $CaptureDirectory){return}
 $level=[PreferencesTest]::GetDlgItem($preferences,1011)
 Send $level 0x0405 1 55;Send $preferences 0x0114 8 $level
 Wait-SettingsFile 'brightness=55';Isolate-Overlays
 [void][PreferencesTest]::ShowWindow($preferences,0)
 Send $window 0x8003
 Start-Sleep -Milliseconds 220
 # Keep the compact flyout at the runner's actual monitor DPI. Forcing an
 # artificial DPI gets corrected by Windows when the tray flyout repositions.
 Capture $name $window $true
 Send $window 0x0010
 [void][PreferencesTest]::ShowWindow($preferences,4)
 Send $preferences 0x0111 1015
 Wait-SettingsFile 'brightness=100'
}
function Assert-PickerFirstPaint {
 $picker=[PreferencesTest]::GetDlgItem($preferences,1010)
 [void][PreferencesTest]::ShowWindow($preferences,4)
 [void][PreferencesTest]::RedrawWindow($preferences,[IntPtr]::Zero,[IntPtr]::Zero,0x0585)
 [void][PreferencesTest]::DwmFlush()
 $rect=[PreferencesTest+Rect]::new();[void][PreferencesTest]::GetWindowRect($picker,[ref]$rect)
 $bitmap=[Drawing.Bitmap]::new($rect.Right-$rect.Left,$rect.Bottom-$rect.Top)
 $graphics=[Drawing.Graphics]::FromImage($bitmap);$dc=$graphics.GetHdc()
 try{if(-not [PreferencesTest]::PrintWindow($picker,$dc,2)){throw 'Could not capture picker first paint'}}finally{$graphics.ReleaseHdc($dc);$graphics.Dispose()}
 try{
  $face=$bitmap.GetPixel(8,[int]($bitmap.Height/2))
  if($face.R -gt 100 -or $face.G -gt 100 -or $face.B -gt 100){throw 'Dark display picker painted a light/white face before hover'}
  $glyphs=0
  for($y=4;$y -lt $bitmap.Height-4;$y++){for($x=12;$x -lt [Math]::Min(180,$bitmap.Width-35);$x++){
   $pixel=$bitmap.GetPixel($x,$y);if($pixel.R -gt 150 -and $pixel.G -gt 150 -and $pixel.B -gt 150){$glyphs++}
  }}
  if($glyphs -lt 10){throw 'Selected display text is missing before hover'}
 }finally{$bitmap.Dispose()}
}
try{
 [void][PreferencesTest]::SetThreadDpiAwarenessContext([IntPtr](-4))
 [void][IO.Directory]::CreateDirectory((Split-Path -Parent $settingsFile))
 [IO.File]::WriteAllText($settingsFile,"brightness=70`nwarm=0`nsmooth=1`nresume=0`ntheme=2`n")
 $key=[Microsoft.Win32.Registry]::CurrentUser.CreateSubKey($runPath);$key.DeleteValue($runName,$false);$key.Dispose()
 Start-Screen;Isolate-Overlays;Open-Settings
 $display=[PreferencesTest]::GetDlgItem($preferences,1010)
 if([PreferencesTest]::SendMessage($display,0x0146,[IntPtr]::Zero,[IntPtr]::Zero).ToInt64() -lt 2){throw 'Display picker did not enumerate screens'}
 Assert-PickerFirstPaint
 Capture 'settings-displays.png'
 if($StoreScreenshots -and ([PreferencesTest]::GetWindowLong($preferences,-16) -band 0x00200000)){throw 'Displays unnecessarily scrolls at the standard 150% capture size'}
 Send $display 0x014E 1;Send $preferences 0x0111 (1010 -bor (1 -shl 16)) $display
 Start-Sleep -Milliseconds 100
 if(-not (([PreferencesTest]::FileText($settingsFile) -split "`r?`n") -match '^selected=.+')){throw 'Selected screen did not persist'}
 Send $display 0x014E 0;Send $preferences 0x0111 (1010 -bor (1 -shl 16)) $display
 Wait-SettingsFile 'selected='
 # Presets and step preferences drive the same renderer model as the mini slider.
 Send $preferences 0x0111 1022;Wait-SettingsFile 'brightness=75'
 $level=[PreferencesTest]::GetDlgItem($preferences,1011)
 Send $level 0x0405 1 43;Send $preferences 0x0114 8 $level;Wait-SettingsFile 'brightness=43'
 Send $preferences 0x0111 1030;Wait-SettingsFile 'quick0=43'
 if((Read-Text ([PreferencesTest]::GetDlgItem($window,120))) -ne '43%'){throw 'Custom quick level did not update the flyout'}
 Send $window 0x0111 121;Wait-SettingsFile 'brightness=50'
 Send $window 0x0111 120;Wait-SettingsFile 'brightness=43'
 $pauseLength=[PreferencesTest]::GetDlgItem($preferences,1017)
 Send $pauseLength 0x014E 2;Send $preferences 0x0111 (1017 -bor (1 -shl 16)) $pauseLength;Wait-SettingsFile 'pauseMinutes=30'
 Send $preferences 0x0111 1014;Start-Sleep -Milliseconds 100
 if((Read-Text ([PreferencesTest]::GetDlgItem($preferences,1013))) -ne 'Resume dimming'){throw 'Timed pause did not pause'}
 if((Read-Text ([PreferencesTest]::GetDlgItem($preferences,1025))) -notmatch 'Resumes in \d{1,2}:\d{2}'){throw 'Visible pause countdown missing'}
 Send $window 0x0113 20;Start-Sleep -Milliseconds 100
 if((Read-Text ([PreferencesTest]::GetDlgItem($preferences,1013))) -ne 'Pause dimming'){throw 'Timed pause did not resume'}
 Send $preferences 0x0111 1015
 Wait-SettingsFile 'brightness=100';Wait-SettingsFile 'smooth=1'
 # Pinning applies only to an interactive panel; Escape/Close always dismisses it.
 Send $window 0x0312 5;Start-Sleep -Milliseconds 220
 if(-not [PreferencesTest]::IsWindowVisible($window)){throw 'The first manual flyout after quiet startup did not show'}
 Send $window 0x0111 113;Wait-SettingsFile 'keepPanelOpen=1'
 Send $window 0x0006;Start-Sleep -Milliseconds 180
 if(-not [PreferencesTest]::IsWindowVisible($window)){throw 'Pinned flyout closed on focus loss'}
 Capture 'slider-quick-controls.png' $window $true
 Send $window 0x0010;Start-Sleep -Milliseconds 200
 if([PreferencesTest]::IsWindowVisible($window)){throw 'Explicit Close must dismiss a pinned panel'}
 Send $window 0x0312 5;Send $window 0x0111 113;Wait-SettingsFile 'keepPanelOpen=0'
 Send $window 0x0006;Start-Sleep -Milliseconds 200
 if([PreferencesTest]::IsWindowVisible($window)){throw 'Unpinned flyout did not dismiss on focus loss'}
 # Every theme persists and exposes a selected accessible button name.
 Send $preferences 0x0111 1001;Start-Sleep -Milliseconds 130
 foreach($theme in 0..6){
  Send $preferences 0x0111 (1100+$theme);Wait-SettingsFile "theme=$theme";Start-Sleep -Milliseconds 50
  if((Read-Text ([PreferencesTest]::GetDlgItem($preferences,1100+$theme))) -notmatch 'selected'){throw 'Selected theme state missing'}
  Capture "settings-theme-$theme.png"
  if($theme -in @(1,2,4)){Capture-Slider "slider-theme-$theme.png"}
 }
 Send $preferences 0x0111 1102;Wait-SettingsFile 'theme=2'
 # Virtual desktop profiles are opt-in and independent of physical displays.
 Send $preferences 0x0111 1003;Start-Sleep -Milliseconds 130
 $desktopProfiles=[PreferencesTest]::GetDlgItem($preferences,1300)
 if([PreferencesTest]::SendMessage($desktopProfiles,0x00F0,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32() -ne 0){throw 'Desktop profiles must be off by default'}
 if(-not (Read-Text ([PreferencesTest]::GetDlgItem($preferences,1301)))){throw 'Desktop detection must expose a current or unavailable state'}
 Send $desktopProfiles 0x00F5;Wait-SettingsFile 'separateDesktops=1'
 $desktopPicker=[PreferencesTest]::GetDlgItem($preferences,1302)
 if([PreferencesTest]::IsWindowEnabled($desktopPicker)){
  $desktopLevel=[PreferencesTest]::GetDlgItem($preferences,1303)
  if(-not [PreferencesTest]::IsWindowEnabled($desktopLevel)){throw 'Detected desktop profile slider must be enabled'}
  Send $desktopLevel 0x0405 1 61;Send $preferences 0x0114 8 $desktopLevel
  Start-Sleep -Milliseconds 200
  if([PreferencesTest]::FileText($settingsFile) -notmatch 'desktop:\{[A-F0-9-]{36}\}:brightness=61'){throw 'Desktop profile level did not persist'}
 }
 Capture 'settings-desktops.png'
 Send $desktopProfiles 0x00F5;Wait-SettingsFile 'separateDesktops=0'
 Send $preferences 0x0111 1015;Wait-SettingsFile 'brightness=100'
 # Consumer brightness input is an explicit opt-in; testing never enables it.
 Send $preferences 0x0111 1004;Start-Sleep -Milliseconds 160
 $brightnessKeys=[PreferencesTest]::GetDlgItem($preferences,1400)
 $windowsBrightness=[PreferencesTest]::GetDlgItem($preferences,1404)
 $keyFeedback=[PreferencesTest]::GetDlgItem($preferences,1403)
 $testKeys=[PreferencesTest]::GetDlgItem($preferences,1401)
 if([PreferencesTest]::SendMessage($brightnessKeys,0x00F0,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32() -ne 0){throw 'Brightness keys must be off by default'}
 if([PreferencesTest]::SendMessage($windowsBrightness,0x00F0,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32() -ne 0){throw 'Windows brightness compatibility must be off by default'}
 if([PreferencesTest]::SendMessage($keyFeedback,0x00F0,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32() -ne 1){throw 'Optional keyboard feedback should initially be on'}
 Send $testKeys 0x00F5
 if((Read-Text $testKeys) -ne 'Stop test'){throw 'Brightness-key test did not start'}
 if([PreferencesTest]::FileText($settingsFile) -notmatch 'brightness=100'){throw 'Key test changed brightness'}
 Send $window 0x0113 31 # Expire the same one-shot test handler without waiting 15 seconds.
 if((Read-Text $testKeys) -ne 'Test brightness controls'){throw 'Brightness-key test did not expire'}
 if((Read-Text ([PreferencesTest]::GetDlgItem($preferences,1402))) -notmatch 'No brightness keys detected'){throw 'Missing compatibility result'}
 Send $brightnessKeys 0x00F5;Wait-SettingsFile 'brightnessKeys=1'
 Send $windowsBrightness 0x00F5;Wait-SettingsFile 'windowsBrightness=1';Wait-SettingsFile 'brightnessKeys=0'
 if([PreferencesTest]::SendMessage($brightnessKeys,0x00F0,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32() -ne 0){throw 'Compatibility must uncheck native input'}
 Start-Sleep -Milliseconds 180
 if([PreferencesTest]::FileText($settingsFile) -notmatch 'brightness=100'){throw 'Enabling compatibility replaced saved brightness'}
 Send $testKeys 0x00F5
 if((Read-Text $testKeys) -ne 'Stop test'){throw 'Compatibility test did not start'}
 Send $window 0x0113 31
 if((Read-Text ([PreferencesTest]::GetDlgItem($preferences,1402))) -notmatch 'Compatibility on'){throw 'Compatibility status must describe Windows changes rather than certify physical keys'}
 Capture 'settings-keyboard-compatibility.png'
 Send $window 0x0312 4;Wait-SettingsFile 'windowsBrightness=0'
 if([PreferencesTest]::SendMessage($windowsBrightness,0x00F0,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32() -ne 0){throw 'Emergency restore must stop compatibility following'}
 Send $windowsBrightness 0x00F5;Wait-SettingsFile 'windowsBrightness=1'
 Send $brightnessKeys 0x00F5;Wait-SettingsFile 'brightnessKeys=1';Wait-SettingsFile 'windowsBrightness=0'
 if([PreferencesTest]::SendMessage($windowsBrightness,0x00F0,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32() -ne 0){throw 'Native input must uncheck compatibility'}
 Send $brightnessKeys 0x00F5;Wait-SettingsFile 'brightnessKeys=0'
 Capture 'settings-keyboard.png'
 # Fallback shortcuts exercise the same non-activating app-brightness panel.
 Send $window 0x0111 113;Wait-SettingsFile 'keepPanelOpen=1'
 $foreground=[PreferencesTest]::GetForegroundWindow()
 Send $window 0x0312 2;Wait-SettingsFile 'brightness=95';Start-Sleep -Milliseconds 220
 if(-not [PreferencesTest]::IsWindowVisible($window)){throw 'Keyboard feedback panel did not appear'}
 if((Read-Text ([PreferencesTest]::GetDlgItem($window,112))) -ne 'App brightness' -or (Read-Text ([PreferencesTest]::GetDlgItem($window,110))) -notmatch 'Screen Dimmer'){throw 'Keyboard feedback must clearly identify software/app brightness'}
 if([PreferencesTest]::GetForegroundWindow() -ne $foreground){throw 'Keyboard feedback stole foreground focus'}
 Capture 'slider-keyboard-feedback.png' $window $true
 Start-Sleep -Milliseconds 2300
 if([PreferencesTest]::IsWindowVisible($window)){throw 'Keyboard feedback did not dismiss automatically'}
 Send $window 0x0111 113;Wait-SettingsFile 'keepPanelOpen=0'
 Send $keyFeedback 0x00F5;Wait-SettingsFile 'brightnessKeyFeedback=0'
 Send $window 0x0312 3;Wait-SettingsFile 'brightness=100';Start-Sleep -Milliseconds 150
 if([PreferencesTest]::IsWindowVisible($window)){throw 'Disabled keyboard feedback still opened the panel'}
 Send $keyFeedback 0x00F5;Wait-SettingsFile 'brightnessKeyFeedback=1'
 Send $window 0x0312 2;Wait-SettingsFile 'brightness=95';Start-Sleep -Milliseconds 180
 Send $window 0x0021 # Adopting the passive panel keeps it interactive instead of auto-hiding.
 Send $window 0x0113 32;Start-Sleep -Milliseconds 150
 if(-not [PreferencesTest]::IsWindowVisible($window)){throw 'Interacting with keyboard feedback did not retain the normal panel'}
 Send $window 0x0010;Start-Sleep -Milliseconds 150
 Send $preferences 0x0111 1015;Wait-SettingsFile 'brightness=100'
 Send $preferences 0x0111 1002;Start-Sleep -Milliseconds 130
 $step=[PreferencesTest]::GetDlgItem($preferences,1204)
 Send $step 0x014E 1;Send $preferences 0x0111 (1204 -bor (1 -shl 16)) $step;Wait-SettingsFile 'step=2'
 Send $window 0x0312 2;Wait-SettingsFile 'brightness=98'
 $shortcut=[PreferencesTest]::GetDlgItem($preferences,1205)
 Send $shortcut 0x014E 1;Send $preferences 0x0111 (1205 -bor (1 -shl 16)) $shortcut;Wait-SettingsFile 'shortcut=1'
 $resume=[PreferencesTest]::GetDlgItem($preferences,1202);Send $resume 0x00F5;Wait-SettingsFile 'resume=1'
 $smooth=[PreferencesTest]::GetDlgItem($preferences,1201);Send $smooth 0x00F5;Wait-SettingsFile 'smooth=0'
 $startup=[PreferencesTest]::GetDlgItem($preferences,1203)
 for($attempt=0;$attempt -lt 80 -and -not [PreferencesTest]::IsWindowEnabled($startup);$attempt++){Start-Sleep -Milliseconds 50}
 if(-not [PreferencesTest]::IsWindowEnabled($startup)){throw 'Startup query never completed'}
 Send $startup 0x00F5
 $expected='"'+$binary+'" --background --startup'
 for($attempt=0;$attempt -lt 60 -and (Read-Run) -ne $expected;$attempt++){Start-Sleep -Milliseconds 50}
 if((Read-Run) -ne $expected){throw 'Startup registration did not quote the executable and use background arguments'}
 Capture 'settings-preferences.png'
 if($StoreScreenshots -and ([PreferencesTest]::GetWindowLong($preferences,-16) -band 0x00200000)){throw 'Preferences unnecessarily scrolls at the standard 150% capture size'}
 Send $startup 0x00F5
 for($attempt=0;$attempt -lt 60 -and $null -ne (Read-Run);$attempt++){Start-Sleep -Milliseconds 50}
 if($null -ne (Read-Run)){throw 'Disabling startup did not remove registration'}
 # Short windows retain access to the last preference through scrolling.
 $rect=[PreferencesTest+Rect]::new();[void][PreferencesTest]::GetWindowRect($preferences,[ref]$rect)
 [void][PreferencesTest]::SetWindowPos($preferences,[IntPtr]::Zero,$rect.Left,$rect.Top,$rect.Right-$rect.Left,480,0x0014)
 $beforeScroll=[PreferencesTest+Rect]::new();[void][PreferencesTest]::GetWindowRect([PreferencesTest]::GetDlgItem($preferences,1206),[ref]$beforeScroll)
 Send $preferences 0x0115 3
 $afterScroll=[PreferencesTest+Rect]::new();[void][PreferencesTest]::GetWindowRect([PreferencesTest]::GetDlgItem($preferences,1206),[ref]$afterScroll)
 if($afterScroll.Top -ge $beforeScroll.Top){throw 'Short settings window did not scroll'}
 # UI resources must be released when settings closes, with no growth per open.
 Send $preferences 0x0010
 # Native controls/theme services can allocate process caches on early opens.
 # Warm them before measuring repeated lifetime growth, and retain every sample.
 foreach($iteration in 1..3){Open-Settings;Send $preferences 0x0010}
 $baseline=[PreferencesTest]::GetGuiResources($process.Handle,0)
 $gdiSamples=@($baseline)
 foreach($iteration in 1..8){Open-Settings;Send $preferences 0x0010;$gdiSamples += [PreferencesTest]::GetGuiResources($process.Handle,0)}
 $after=[PreferencesTest]::GetGuiResources($process.Handle,0)
 Write-Output "Closed-settings GDI samples after warmup: $($gdiSamples -join ', ')"
 if($after -gt $baseline+2){throw "Settings leaked GDI objects: $baseline -> $after"}
 # Explicit opt-in resume persists; --paused remains a recovery override.
 Open-Settings;Send $preferences 0x0111 1000
 $level=[PreferencesTest]::GetDlgItem($preferences,1011);Send $level 0x0405 1 99;Send $preferences 0x0114 8 $level;Wait-SettingsFile 'brightness=99'
 Quit-Screen
 Start-Screen $false;Isolate-Overlays
 Open-Settings
 if((Read-Text ([PreferencesTest]::GetDlgItem($window,120))) -ne '43%' -or [PreferencesTest]::FileText($settingsFile) -notmatch 'pauseMinutes=30'){throw 'Quick controls did not survive restart'}
 if((Read-Text ([PreferencesTest]::GetDlgItem($preferences,1013))) -ne 'Pause dimming'){throw 'Opt-in resume was not applied'}
 Send $preferences 0x0111 1001
 if((Read-Text ([PreferencesTest]::GetDlgItem($preferences,1102))) -notmatch 'selected'){throw 'Theme did not survive restart'}
 Quit-Screen
 # Direct settings access works without needing the transient tray flyout.
 $process=Start-Process -FilePath $binary -ArgumentList '--settings','--paused' -WindowStyle Hidden -PassThru
 $preferences=[IntPtr]::Zero
 for($attempt=0;$attempt -lt 100 -and $preferences -eq [IntPtr]::Zero;$attempt++){Start-Sleep -Milliseconds 50;$preferences=[PreferencesTest]::FindWindow('Shardapps.Screen.Settings','Screen Dimmer settings')}
 if($preferences -eq [IntPtr]::Zero){throw 'Direct settings launch failed'}
 $window=[PreferencesTest]::FindWindow('Shardapps.Screen.Control','Screen Dimmer by Shardapps');Quit-Screen
 # Uninstall cleanup removes only the registration for this executable.
 $key=[Microsoft.Win32.Registry]::CurrentUser.CreateSubKey($runPath);$key.SetValue($runName,$expected);$key.Dispose()
 $cleanup=Start-Process -FilePath $binary -ArgumentList '--remove-startup' -WindowStyle Hidden -PassThru
 if(-not $cleanup.WaitForExit(5000) -or $cleanup.ExitCode -ne 0){throw 'Startup cleanup command failed'}
 if($null -ne (Read-Run)){throw 'Uninstall cleanup left its startup entry'}
 $otherCommand='"C:\Other Screen Copy\Screen.exe" --background --startup'
 $key=[Microsoft.Win32.Registry]::CurrentUser.CreateSubKey($runPath);$key.SetValue($runName,$otherCommand);$key.Dispose()
 $cleanup=Start-Process -FilePath $binary -ArgumentList '--remove-startup' -WindowStyle Hidden -PassThru
 if(-not $cleanup.WaitForExit(5000) -or $cleanup.ExitCode -ne 0 -or (Read-Run) -ne $otherCommand){throw 'Cleanup changed a different copy startup entry'}
 $helper=Join-Path (Split-Path -Parent $binary) 'ScreenStartup.exe'
 if(Test-Path -LiteralPath $helper){
  [IO.File]::WriteAllText($settingsFile,"brightness=70`nresume=0`nkeepPanelOpen=1`n")
  $launcher=Start-Process -FilePath $helper -WindowStyle Hidden -PassThru
  if(-not $launcher.WaitForExit(5000) -or $launcher.ExitCode -ne 0){throw 'Startup helper failed'}
  $window=[IntPtr]::Zero
  for($attempt=0;$attempt -lt 100 -and $window -eq [IntPtr]::Zero;$attempt++){Start-Sleep -Milliseconds 50;$window=[PreferencesTest]::FindWindow('Shardapps.Screen.Control','Screen Dimmer by Shardapps')}
  if($window -eq [IntPtr]::Zero){throw 'Startup helper did not launch Screen'}
  [uint32]$appProcessId=0;[void][PreferencesTest]::GetWindowThreadProcessId($window,[ref]$appProcessId);$process=Get-Process -Id $appProcessId
  Start-Sleep -Milliseconds 200
  if([PreferencesTest]::IsWindowVisible($window)){throw 'Startup helper opened the flyout'}
  Quit-Screen
 }
 Write-Output "PASS: display selection, presets, timed pause, seven themes, shortcut/step/resume persistence, startup enable/disable and cleanup, quiet startup helper, short-window scrolling, restart and GDI lifetime ($baseline -> $after)."
}finally{
 if($process -and -not $process.HasExited){Stop-Process -Id $process.Id -Force;$process.WaitForExit()}
 Restore-Run
 if($hadSettings){[IO.File]::WriteAllBytes($settingsFile,$savedBytes)}elseif(Test-Path -LiteralPath $settingsFile){Remove-Item -LiteralPath $settingsFile}
}
