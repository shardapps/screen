# Performance measurements

## Regression comparison for the next release

The x64 Windows CI job builds the pinned Store alpha source (`0a4ecf9`) and the current source with the same toolchain on the same host. `scripts/performance-test.ps1` interleaves five samples each of baseline shared-brightness tray mode, current shared mode, and current mode with three saved desktop profiles. Each process is forced paused; the script preserves the exact preference file and shuts down only its own copy.

The artifact `Screen-performance-comparison` contains raw samples, median/range private commit, working set, CPU deltas, executable sizes and OS context. CI investigates a shared-tray private-memory regression above 20%, a binary above 1 MiB or a material paused CPU increase. Profile-mode costs are reported separately rather than silently compared with a simpler baseline. These are paused-process measurements, not compositor/GPU memory, an animation smoothness benchmark or proof of real Task View switching.

The beta adds a fourth paused mode with brightness-key listening explicitly enabled. Compare it with ordinary shared mode to assess the optional HID parser/registration cost; the off state does not create that listener. Held-key repeat and mini-panel feedback are inactive during these idle measurements. Real key delivery and active input/rendering costs remain separate acceptance cases.

The compatibility fix adds a fifth paused mode with Windows brightness notifications enabled. It uses the existing UI thread, has no WMI observer or idle polling timer, and receives an initial baseline without applying dimming. The same 20% private-memory and 50 ms idle CPU investigation budgets apply. Physical notifications, active dimming and the mini panel were checked separately with a user; a successful check on one machine does not establish universal compatibility.

Frame timers are created lazily and armed only during brightness or flyout motion. One-shot scheduling follows the reported desktop composition rate, capped at 144 Hz, without changing the global timer resolution. A native window timer remains the fallback. The portable smoothing model retains velocity through rapid retargets; all frame timers stop when settled or motion is disabled.

### Alpha 2 comparison result

[Windows CI run 39845380675](https://github.com/shardapps/screen/actions/runs/39845380675), source `b355313`, measured the following medians on one Windows 2025 x64 runner (OS reported 10.0.26100). Five interleaved samples per mode used a two-second warmup and a three-second paused measurement. Both binaries used the same Release MSVC toolchain. Raw samples are retained in the run's `Screen-performance-comparison` artifact.

| Metric | Store alpha baseline | Updated shared mode | Updated desktop profiles |
| --- | ---: | ---: | ---: |
| Private committed memory, median | 2.297 MiB | 2.281 MiB | 2.664 MiB |
| Private committed memory, range | 2.266–2.309 MiB | 2.270–2.301 MiB | 2.629–2.703 MiB |
| Working set, median | 13.574 MiB | 13.582 MiB | 20.359 MiB |
| Process CPU time over 3 seconds, median | 0 ms measured | 0 ms measured | 0 ms measured |

The updated executable was 518,656 bytes, compared with 482,304 bytes for the baseline. Shared-mode private memory remains within baseline variation. Desktop profiles add registry observation and, where Explorer metadata is unavailable on the hosted desktop, public shell fallback; Windows may load additional shared pages and thread-pool workers. Three saved profiles were loaded, but this runner did not expose ordinary Task View desktops. The local read-only diagnostic separately detected three named desktops on the user's Windows 11 machine.

These short, paused samples do not establish literal zero CPU, active rendering cost, frame delivery, or real desktop-switch latency. Display refresh, HDR and power state were not recorded for this paused comparison; compositor/GPU allocations are excluded. Real multi-monitor/HDR and Task View transitions remain manual acceptance cases.

## v0.4.0 native settings sample

Measured October 19, 2026 with the same Release x64 compiler/OS/display setup as below. Each row starts a separate process, settles for two seconds after the requested state, and measures process CPU over ten seconds. Settings and startup registrations were restored after tests.

| Metric | Paused tray | Desktop at 99% | Settings open | Settings opened, then closed |
| --- | ---: | ---: | ---: | ---: |
| Working set | 13.66 MiB | 13.89 MiB | 19.44 MiB | 19.48 MiB |
| Private committed memory | 2.25 MiB | 2.25 MiB | 2.91 MiB | 2.89 MiB |
| Process CPU time over 10 seconds | 0 ms measured | 0 ms measured | 0 ms measured | 0 ms measured |

The executable was 482,304 bytes (471 KiB). Settings are created on demand; fonts, brushes and controls are released on close. Windows may keep loaded shared UI pages resident afterward, as the final column shows. Eight repeated opens/closes measured 36 → 37 GDI objects, with no per-open growth. These single local samples do not establish a universal memory budget or literal zero CPU, and exclude compositor/GPU costs. CI repeats behavioral and resource-lifetime checks rather than asserting machine-specific memory values.

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
