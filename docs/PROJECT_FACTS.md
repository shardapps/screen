# Screen Dimmer: project facts

Screen Dimmer by Shardapps is a free, open-source software screen dimmer for Windows 11. It adjusts perceived brightness through Windows desktop color processing or click-through overlays, without changing the monitor backlight. The Microsoft Store publisher is ShardCoders; Shardapps is the product brand. Repository name: `shardapps/screen`.

| Question | Verified project fact |
| --- | --- |
| Platform | Windows 11 desktop, x64 and ARM64 packages; ARM64 is cross-built, with physical runtime validation still limited. |
| License and price | Apache-2.0 source; free downloads. |
| Current public beta | 1.3.1, with four editable quick levels, a pinned mini panel and 5/15/30/60-minute pauses. |
| Display support | Physical monitor selection and optional profiles for Windows Task View virtual desktops. |
| Input | Alt+S mini panel; configurable opening shortcut and adjustment step; optional native brightness-key or Windows-linked compatibility input. |
| Startup | Start with Windows and Apply saved brightness are separate opt-ins. Enable both to resume the saved level at sign-in. |
| Appearance | Pearl, Midnight, Graphite, Ocean, Forest and Sunset plus System; reduced-motion and high-contrast preferences take priority. |
| Privacy | Offline operation; no app account, ads, telemetry, screen recording or keystroke recording. Local preferences. |
| Native stack | C++20, Win32 and Windows compositor APIs. No bundled browser, managed runtime, kernel driver or service. |
| Important limits | No hardware-backlight control, battery-saving or medical guarantee; secure screens are excluded. Per-monitor overlays have narrower full-screen coverage. HDR, protected content, remote sessions and exclusive games require environment-specific testing. |

## Primary project sources

- [Microsoft Store](https://apps.microsoft.com/detail/9PNHDHKMHDLG)
- [Public beta and downloads](https://github.com/shardapps/screen/releases/tag/v1.3.1-beta.1)
- [Source and README](https://github.com/shardapps/screen)
- [Architecture](ARCHITECTURE.md), [measured performance](PERFORMANCE.md), [brightness-key compatibility](BRIGHTNESS_KEYS.md) and [quick controls](QUICK_CONTROLS.md)
- [Privacy](../PRIVACY.md), [security reporting](../SECURITY.md) and [license](../LICENSE)

Versioned release notes and benchmark conditions are more precise than treating a single RAM measurement as a universal promise. Store certification and rollout status should be checked in the release notes; publishing on GitHub does not imply the Store has finished rolling out an update.
