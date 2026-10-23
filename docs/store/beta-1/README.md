# Public beta gallery — October 24, 2026

Seven replacement images introduce the current quick-controls panel, followed by native settings captures. The product remains Screen Dimmer by Shardapps, with ShardCoders as its Store publisher.

| Order | File | Content and provenance |
| --- | --- | --- |
| 1 | [Mini panel](01-slider.png) | Built-in imagegen enlarged presentation based on the actual Midnight panel at 55%. |
| 2 | [Pearl and Ocean](02-themes.png) | Built-in imagegen enlarged presentations of two actual theme variants at 55%. |
| 3 | [Displays](03-displays.png) | Unmodified native capture: screen selection, editable presets, flexible pause. |
| 4 | [Appearance](04-appearance.png) | Unmodified native capture: all six themes plus System. |
| 5 | [Preferences](05-preferences.png) | Unmodified native capture: startup/resume, motion and pinned-panel preference. |
| 6 | [Desktops](06-desktops.png) | Unmodified native capture: Windows virtual desktop profiles. |
| 7 | [Keyboard](07-keyboard.png) | Unmodified native capture: off-by-default modes and independent feedback; this image shows an example compatibility selection. |

The first two are **enlarged interface presentations**, not pixel-identical desktop screenshots. Their labels, values and control order were compared with the originals in [raw/](raw/). The custom 43% preset is an example; factory defaults are 25/50/75/100%. Rendering uses an overlay in the capture fixture, as the panel footer states. Configuration examples are not advertised as defaults.

Capture source: `95e2328c5d8bcd9f32f3b5fde7dbe363ba318863`, version 1.3.1, [Windows CI 40010674140](https://github.com/shardapps/screen/actions/runs/40010674140). The isolated Windows desktop rendered settings at 150% into 1440 × 1040 native captures; compact panels were captured at actual monitor DPI. No user's desktop, documents or private profile names appear. x64 behavior, resources, startup, package/installer, memory, sanitizers and CodeQL checks passed. ARM64 was build/package validation only.

`brand-banner.png` is generated README/release artwork and `crescent-art.png` is its decorative source. Neither is an app screenshot. The app icon is deterministic geometry from `scripts/generate_assets.py`; generated marketing art is excluded from application runtime and setup/portable payloads.

[PROMPTS.md](PROMPTS.md) records all built-in imagegen prompts and input roles. [ASSETS.json](ASSETS.json) records dimensions, hashes and file sizes. `listing.en-US.json` in the parent directory contains captions and the Store copy. Store submission 4: `1152921505702217242`; certification status is recorded in the public release notes after submission. The previous gallery is retained in `../gallery-v2/` for reference/recovery.
