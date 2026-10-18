# Performance measurements

## v0.3.0 desktop backend sample

Measured October 19, 2026: Release x64, MSVC 19.51, Windows 11 build 26200, one display. Two separate processes started paused; the active process then received an All displays slider change to 99%. After a two-second settling interval, process CPU deltas were sampled over ten seconds. Preferences were restored after the measurement.

| Metric | Paused tray | Active desktop color effect |
| --- | ---: | ---: |
| Working set | 13.54 MiB | 13.79 MiB |
| Private committed memory | 2.21 MiB | 2.24 MiB |
| Process CPU time delta | 0.0 ms | 15.625 ms |

The executable was 421,888 bytes (412 KiB). This single sample confirms no large process-memory increase from the new path on this machine; it is not a universal budget or a measurement of Windows compositor/GPU allocations. Active steady state retains the small four-times-per-second color-state audit, with no animation timer or screen-copy loop. Keep the historical overlay sample below separate when assessing regressions.

## v0.2.0 development baseline

Measured October 18, 2026: Release x64, MSVC 19.51, Windows 11 build 26200, one connected display, after a two-second warmup, over ten-second samples. The active sample applies a full-screen overlay at 99% brightness; deeper dimming has the same surface dimensions.

| Metric | Paused tray | Active overlay |
| --- | ---: | ---: |
| Working set | 12.96 MiB | 13.09 MiB |
| Private committed memory | 2.13 MiB | 2.10 MiB |
| Process CPU time delta | 0.0 ms | 0.0 ms |

The development executable was 414,720 bytes (405 KiB). These are separate process samples; allocator and OS behavior can make one private-memory sample slightly lower than another.

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
- Avoid busy loops, timer-resolution changes, and permanent per-frame screen copies. Keep the active-only 4 Hz coverage audit small; it is needed for silent order changes and must stop when paused/at 100%.
- Investigate a reproducible regression above 20% in comparable private commit or transition CPU before merging.

Report memory and frame cadence under active multi-monitor dimming before Store release. The local paused baseline alone does not establish those properties.
