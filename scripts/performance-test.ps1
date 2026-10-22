[CmdletBinding()]
param(
 [Parameter(Mandatory)][string]$Executable,
 [Parameter(Mandatory)][string]$BaselineExecutable,
 [string]$OutputPath='build/performance-comparison.json',
 [ValidateRange(2,10)][int]$Seconds=3,
 [ValidateRange(3,7)][int]$Samples=3
)
$ErrorActionPreference='Stop'
Add-Type @'
using System;using System.Runtime.InteropServices;
public static class PerformanceTest {
 [DllImport("user32.dll",CharSet=CharSet.Unicode)]public static extern IntPtr FindWindow(string cls,string title);
 [DllImport("user32.dll")]public static extern IntPtr SendMessage(IntPtr hwnd,uint msg,IntPtr wp,IntPtr lp);
 [DllImport("user32.dll")]public static extern uint GetWindowThreadProcessId(IntPtr hwnd,out uint pid);
}
'@
if([PerformanceTest]::FindWindow('Shardapps.Screen.Control','Screen Dimmer by Shardapps') -ne [IntPtr]::Zero){throw 'Close Screen before isolated performance comparisons.'}
$settingsFile=Join-Path $env:LOCALAPPDATA 'Shardapps/Screen/settings.ini'
$hadSettings=Test-Path -LiteralPath $settingsFile
$savedBytes=if($hadSettings){[IO.File]::ReadAllBytes($settingsFile)}else{$null}
$process=$null
$rows=[Collections.Generic.List[object]]::new()
function Measure-Screen([string]$path,[string]$mode,[bool]$profiles){
 $settings="brightness=70`nresume=0`nsmooth=1`nseparateDesktops=$([int]$profiles)`n"
 if($profiles){
  foreach($index in 1..3){$id='{'+('{0:X8}' -f $index)+'-0000-0000-0000-000000000001}';$settings+="desktop:$($id):brightness=45`n"}
 }
 [IO.File]::WriteAllText($settingsFile,$settings)
 $script:process=Start-Process -FilePath (Resolve-Path -LiteralPath $path).Path -ArgumentList '--background','--paused' -WindowStyle Hidden -PassThru
 Start-Sleep -Seconds 2
 if($process.HasExited){throw 'Benchmark copy exited; another instance may be running.'}
 $window=[PerformanceTest]::FindWindow('Shardapps.Screen.Control','Screen Dimmer by Shardapps')
 $ownerId=0u;[void][PerformanceTest]::GetWindowThreadProcessId($window,[ref]$ownerId)
 if($ownerId -ne $process.Id){throw 'Benchmark window is not owned by this process.'}
 $process.Refresh();$cpu=$process.TotalProcessorTime.TotalMilliseconds
 Start-Sleep -Seconds $Seconds
 $process.Refresh()
 $rows.Add([ordered]@{mode=$mode;privateMiB=[Math]::Round($process.PrivateMemorySize64/1MB,3);workingSetMiB=[Math]::Round($process.WorkingSet64/1MB,3);cpuMilliseconds=[Math]::Round($process.TotalProcessorTime.TotalMilliseconds-$cpu,3);handles=$process.HandleCount})
 [void][PerformanceTest]::SendMessage($window,0x0111,[IntPtr]107,[IntPtr]::Zero)
 if(-not $process.WaitForExit(5000) -or $process.ExitCode -ne 0){throw 'Benchmark copy did not exit cleanly.'}
}
function Median($values){$sorted=@($values | Sort-Object);$n=$sorted.Count;if($n%2){return $sorted[[int][Math]::Floor($n/2)]};return ($sorted[$n/2-1]+$sorted[$n/2])/2}
try{
 [void][IO.Directory]::CreateDirectory((Split-Path -Parent $settingsFile))
 # Interleave samples to reduce run-order bias on the same hosted machine.
 foreach($sample in 1..$Samples){Measure-Screen $BaselineExecutable 'baseline-shared' $false;Measure-Screen $Executable 'current-shared' $false;Measure-Screen $Executable 'current-profiles' $true}
 $summary=@(foreach($mode in @('baseline-shared','current-shared','current-profiles')){
  $group=@($rows | Where-Object { $_.mode -eq $mode })
  [ordered]@{mode=$mode;medianPrivateMiB=(Median $group.privateMiB);minPrivateMiB=($group.privateMiB | Measure-Object -Minimum).Minimum;maxPrivateMiB=($group.privateMiB | Measure-Object -Maximum).Maximum;medianWorkingSetMiB=(Median $group.workingSetMiB);medianCpuMilliseconds=(Median $group.cpuMilliseconds)}
 })
 $baseline=$summary | Where-Object mode -eq 'baseline-shared'
 $current=$summary | Where-Object mode -eq 'current-shared'
 $report=[ordered]@{sampleSeconds=$Seconds;samples=$Samples;currentBytes=(Get-Item -LiteralPath $Executable).Length;baselineBytes=(Get-Item -LiteralPath $BaselineExecutable).Length;os=[Environment]::OSVersion.VersionString;summary=$summary;raw=$rows.ToArray();caveat='Same-host paused tray samples. Excludes compositor/GPU allocations, active dimming and real virtual-desktop switching. Profile mode loads three saved profiles; detection depends on the hosted session.'}
 [void][IO.Directory]::CreateDirectory((Split-Path -Parent ([IO.Path]::GetFullPath($OutputPath))))
 $report | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $OutputPath -Encoding utf8
 $summary | Format-Table | Out-String | Write-Output
 if($current.medianPrivateMiB -gt $baseline.medianPrivateMiB*1.20){throw 'Shared tray private memory regressed by more than 20%; inspect the comparison artifact.'}
 if($report.currentBytes -gt 1MB){throw 'Native executable exceeds the 1 MiB engineering budget.'}
 if($current.medianCpuMilliseconds -gt 50){throw 'Unexpected paused shared-tray CPU; inspect samples before merging.'}
}finally{
 if($process -and -not $process.HasExited){Stop-Process -Id $process.Id -Force;$process.WaitForExit()}
 if($hadSettings){[IO.File]::WriteAllBytes($settingsFile,$savedBytes)}elseif(Test-Path -LiteralPath $settingsFile){Remove-Item -LiteralPath $settingsFile}
}
