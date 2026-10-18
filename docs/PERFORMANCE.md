# Performance measurements

## Baseline

Measured October 18, 2026: Release x64, MSVC 19.51, Windows 11 build 26200, one connected display, paused in the tray, after a two-second warmup, over a ten-second idle sample.

| Metric | Observed |
| --- | ---: |
| Screen.exe | 398,336 bytes (389 KiB) |
| Working set | 13.26 MiB |
| Private committed memory | 2.12 MiB |
| Process CPU time delta | 0.0 ms |

This sample is illustrative, not a performance SLA. Working set includes resident shared pages; private commit is a different measurement. Neither includes all memory charged to DWM or the GPU. An active overlay, additional displays, higher resolutions, HDR, driver changes, UI activity, and OS differences can change results. A CPU time delta below the accounting granularity does not prove literally zero work.

## Reproduce

Close any running Screen instance, build Release, and run:

```powershell
./scripts/benchmark.ps1 -Executable build/x64/Release/Screen.exe -Seconds 10
```

The script launches a paused tray instance and emits JSON for binary size, working set, private commit, process CPU time, duration, OS, and architecture. It stops only the process it launched. It never applies dimming.

For a release comparison, take at least five samples on the same OS/machine after warmup, report the median and range, and record display count, resolution, scaling, refresh rate, SDR/HDR, power mode, compiler, and commit. Record foreground, paused tray, active dimming, and rapid slider movement separately. Use Task Manager's GPU view or Windows Performance Recorder to account for DWM and GPU work.

## Engineering budgets

Targets to investigate, not claimed universal results:

- Keep the portable Release executable below 1 MiB without sacrificing hardening or accessibility.
- Keep application idle CPU near the measurement floor after a transition settles.
- Avoid periodic timers, busy loops, timer-resolution changes, and permanent per-frame screen copies.
- Investigate a reproducible regression above 20% in comparable private commit or transition CPU before merging.

Report memory and frame cadence under active multi-monitor dimming before Store release. The local paused baseline alone does not establish those properties.
