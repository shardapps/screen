# Store gallery refresh — October 20, 2026

Five replacement gallery images, led by the compact slider instead of settings pages. The native settings captures have larger, readable controls and show every page without clipped content.

| Order | File | Source |
| --- | --- | --- |
| 1 | [Midnight slider](01-slider.png) | ImageGen promotional composition based on the real flyout |
| 2 | [Pearl and Ocean sliders](02-themes.png) | ImageGen promotional composition based on two real flyouts |
| 3 | [Displays](03-displays.png) | Unmodified native Windows capture, 150% rendering |
| 4 | [Appearance](04-appearance.png) | Unmodified native Windows capture, 150% rendering |
| 5 | [Preferences](05-preferences.png) | Unmodified native Windows capture, 150% rendering |

The first two images are enlarged promotional presentations, not pixel-identical screenshots. Captions explicitly identify that distinction. They preserve the product's controls and labels; the originals are in [raw/](raw/). The Preferences capture shows an example configuration, not the default startup or resume state.

The gallery artwork is 1672 × 941; native settings captures are 1440 × 1040. All five are PNG files above the Store's recommended 1366 × 768 dimensions. None is used as the optional hero slot, which requires different exact dimensions.

## Reproduction and review

- Capture source: `78c96bd`, with no app runtime changes.
- [Windows CI 39385749573](https://github.com/shardapps/screen/actions/runs/39385749573): x64 and ARM64 builds, native settings/themes/startup/resource checks, desktop/overlay tests, installers, package validation and Linux sanitizers passed.
- Actual flyouts: Midnight, Pearl and Ocean, brightness 55%, default Alt+S shortcut.
- Native settings render at 150% on the isolated runner's supported 1920 × 1080 desktop. A higher desktop resolution was unsupported, so the final capture fits the complete UI within 1080p.
- Inspected all five finals for legibility, accurate labels, selection state, clipping and empty panels.
- No new app permissions, dependencies, services or background work.

[PROMPTS.md](PROMPTS.md) and [COMPOSITION_PROMPTS.md](COMPOSITION_PROMPTS.md) record the built-in ImageGen prompt set. [brand-card.png](brand-card.png) is additional promotional artwork; [crescent-art.png](crescent-art.png) is its source background. Neither is represented as an app screenshot.

## Store update

The original app is live. Submission 2 (`1152921505702177743`) was created for this imagery refresh. [PR #13](https://github.com/shardapps/screen/pull/13) records the final submission outcome; Partner Center remains the authority for certification and publication status.

The former gallery remains in `../screenshots/` for reference. To revert, start a listing update and upload those four originals with their earlier captions. This update does not change the app package.
