# Alpha 2 native captures

Unmodified Windows captures from [CI run 39845380675](https://github.com/shardapps/screen/actions/runs/39845380675), app source `b355313`. These show the new native picker painting, four-page navigation, theme palettes and clearer saved-brightness preference. The release metadata update does not change these pages' appearance.

The three settings images render natively at 150%, 1440 × 1040, on the isolated CI desktop. The compact slider is its actual 330 × 136 window capture, retained as a reference rather than enlarged and presented as a Store screenshot. No user's screen or preferences were captured. The Preferences image shows a test configuration, not the default startup/animation settings.

The first two Store gallery slots retain the explicitly labeled promotional slider compositions from [gallery-v2](../gallery-v2/README.md); slots 3–5 use these refreshed native settings captures. The hosted runner does not expose normal Task View desktops, so its unavailable-detection page is not used as a marketing screenshot. Real desktop identification was checked separately with the read-only diagnostic.

All three settings images and the slider were inspected for legibility, complete rendering and clipped controls. Package, installer, native behavior, settings/resource, portable sanitizer and CodeQL checks passed for the capture source. Store submission/publication status is tracked separately from the creation of these files.
