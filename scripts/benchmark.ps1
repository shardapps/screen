[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Executable,
    [ValidateRange(2, 60)][int]$Seconds = 10
)
$ErrorActionPreference = 'Stop'
$binary = (Resolve-Path -LiteralPath $Executable).Path
$process = Start-Process -FilePath $binary -ArgumentList '--background','--paused' -WindowStyle Hidden -PassThru
try {
    Start-Sleep -Seconds 2
    if ($process.HasExited) { throw 'Screen exited; close any existing instance before benchmarking.' }
    $process.Refresh()
    $cpuBefore = $process.TotalProcessorTime.TotalMilliseconds
    $clock = [Diagnostics.Stopwatch]::StartNew()
    Start-Sleep -Seconds $Seconds
    $clock.Stop()
    $process.Refresh()
    [ordered]@{
        mode = 'tray, paused, transparent overlays'
        sampleSeconds = [math]::Round($clock.Elapsed.TotalSeconds, 3)
        executableBytes = (Get-Item -LiteralPath $binary).Length
        workingSetMiB = [math]::Round($process.WorkingSet64 / 1MB, 2)
        privateCommitMiB = [math]::Round($process.PrivateMemorySize64 / 1MB, 2)
        cpuMilliseconds = [math]::Round($process.TotalProcessorTime.TotalMilliseconds - $cpuBefore, 3)
        os = [Environment]::OSVersion.VersionString
        architecture = [Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString()
        caveat = 'One local sample. Excludes DWM/GPU memory; active overlays, monitor count, resolution and OS affect results.'
    } | ConvertTo-Json
} finally {
    if (-not $process.HasExited) { Stop-Process -Id $process.Id; $process.WaitForExit() }
}
