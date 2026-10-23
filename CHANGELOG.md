# Changelog

## 1.3.1 â€” Public beta

- Publish the user-tested quick controls, editable presets, pinned mini panel, flexible pauses, refined themes and optional keyboard compatibility from 1.3.0.
- Skip redundant native text and checkbox changes during brightness updates; short labels use stack storage instead of allocating a comparison string.
- Refine the crescent app icon while retaining tiny native assets.
- Keep marketing artwork out of setup and portable downloads while retaining local Markdown help.
- Refresh the Store gallery, description and project documentation for the public beta.


## 1.3.0 â€” Beta polish (unreleased)

- Add four saved quick brightness levels to the mini panel. Save the current level into any slot from Displays settings; presets act on the selected screen/current desktop.
- Add pin/unpin and pause/resume buttons to the 330 Ã— 184 flyout, with a visible input-mode label and an ordered keyboard focus path. Pinning keeps manual controls open across app switches; passive keyboard feedback still dismisses automatically.
- Offer 5/15/30/60-minute pauses and a visible countdown. The background pause remains one-shot; countdown refresh stops when both control windows are hidden or minimized.
- Refine all six theme palettes, add a framed settings surface, animate navigation and hover/focus feedback, and keep native controls and reduced-motion/high-contrast support.
- Fix the first manual flyout after hidden startup, preserve custom controls across restart, and extend lifecycle/UI regressions.
- Compare shared and Windows-following idle use against the pinned 1.2.1 source. This pass does not publish a public beta or Store update.

## 1.2.1 â€” Compatibility fix (unreleased)

- Add a separate, off-by-default Windows brightness compatibility mode for systems that report brightness changes without delivering HID sun-key reports.
- Follow the Windows percentage on the selected display/desktop, with the existing 8% safety floor and independent App brightness mini-panel feedback.
- Preserve saved dimming on initial notification; ignore duplicate levels, batch saves, and release/rebaseline notifications on lock/sleep/resume without polling or an added thread.
- Keep timed pause effective; Restore and emergency recovery disable compatibility following so automatic changes cannot immediately reapply it.
- State explicitly that compatibility also follows Windows-slider and automatic changes; it does not intercept Fn keys or suppress hardware handling.
- Keep replacement release gated on hands-on acceptance. Beta 1 downloads were withdrawn after a reported key-input failure.

## 1.2.0 â€” Beta 1

- Add opt-in support for standard HID keyboard brightness keys, with a 15-second compatibility test and existing Ctrl+Alt+Up/Down fallback.
- Show a non-activating, auto-dismissing mini panel labeled App brightness for keyboard changes, with an independent feedback switch.
- Use the selected monitor and current desktop profile for brightness-key adjustments; preserve each device's own hardware handling.
- Bound held-key repeat, batch settings writes until release, and release the input listener when disabled or a temporary test ends.
- Add registration/resource, settings/persistence, repeat/bounds, focus-preservation and feedback-dismissal regression checks; compare memory with keyboard listening on and off.

## 1.1.0 â€” Alpha 2

- Add opt-in brightness profiles for named Windows virtual desktops, with independent physical-monitor overrides and a shared fallback.
- Add a Desktops settings page to edit detected profiles without switching the Windows desktop.
- Keep desktop discovery event-driven and release its observer when it is no longer needed.
- Fix the display picker's blank white first paint, refine all six palettes and clarify the two startup preferences.
- Add short time-based theme/tab transitions and animation-only high-resolution pacing for brightness and the flyout.
- Compare paused memory/CPU with the Store alpha in Windows CI; add no framework or permanent frame loop.

## 1.0.0 â€” Alpha 1

- Prepare the first Microsoft Store alpha as Screen Dimmer by Shardapps.
- Use the reserved Store product identity, x64/ARM64 packages and opt-in startup task.
- Add Store listing copy, real screenshots, a 300 px tile and separate promotional artwork.
- Preserve existing local preferences and the familiar compact controls.


## 0.4.0 â€” 2026-10-19

- Add on-demand Displays, Appearance and Preferences settings, plus a direct flyout screen picker.
- Add six palettes (Pearl, Midnight, Graphite, Ocean, Forest, Sunset), System following Windows, and high-contrast priority.
- Add screen identification, brightness presets, a 15-minute pause, configurable adjustment steps and opening shortcut.
- Add opt-in quiet Windows startup with separate saved-dimming resume; matching-path uninstall cleanup and MSIX StartupTask support.
- Share the buffered native slider renderer, release settings resources on close, and keep transitions short with no idle UI timer.
- Retry transient atomic preference-file replacement conflicts on the worker; add settings/startup/resource-lifetime regression checks to CI.

## 0.3.0 â€” desktop dimming for full-screen apps

- Use Windows' full-screen color transform for uniform brightness across all displays, avoiding the window-order limitation of a black overlay over full-screen app surfaces.
- Retain the 8% floor, smooth transitions, warm tint, pause and immediate restore; the desktop effect includes Screen's own panel.
- Keep the overlay renderer for different per-display brightness levels, a busy/unavailable color API, or the explicit `--overlay` compatibility option. The flyout reports Desktop or Overlay while active.
- Preserve existing color state and restore on pause, lock and quit. Test forced-exit cleanup and coexistence with another color client.
- Add portable color-matrix tests and native desktop-effect/installed-app regression checks. No elevation, driver, zoom, screen capture or hardware brightness changes are required.

## 0.2.1 â€” slider polish

- Remove stale thumb trails and native trackbar fragments during dragging with complete buffered slider frames.
- Replace the dotted focus rectangle with a small solid ring around the thumb when using the keyboard; retain native keyboard and accessibility support.
- Align the rounded flyout region with its actual bounds.

## 0.2.0 â€” compact controls and reliable dimming

- Replace the large window with a 330 Ã— 136 flyout anchored above the bottom-right taskbar.
- Add Alt+S to open/hide it, with Ctrl+Alt+S fallback, Escape/outside-click dismissal, dark/light/high-contrast colors and short reduced-motion-aware transitions.
- Move per-display selection, tint, pause, restore and quit into the tray/context menu.
- Extend dimming to an 8% floor while retaining immediate emergency restore and transparent startup.
- Preserve fade velocity through rapid slider changes; avoid repainting unchanged full-screen overlay content or resetting timers every frame.
- Repair foreground/topmost ordering, lost window handles, display changes and resume events. A lightweight active-only 4 Hz order audit covers silent promotions that emit no usable WinEvent.
- Add regression tests for actual topmost promotions, repeated slider retargeting, display/resume recovery, compact panel positioning and dismissal.

## Preview distribution fix

- Add per-user x64/ARM64 setup EXEs with Start menu shortcuts and Windows uninstall support.
- Test installation, installed app behavior, reinstall and removal on x64.
- Separate unsigned MSIX validation artifacts from user downloads; add install instructions and checksums.

## 0.1.0 â€” developer preview

- Native Windows 11 overlay dimming with per-display brightness and optional warm tint.
- Dedicated overlay thread, coalesced state updates, and time-based smooth transitions.
- System tray controls, global shortcuts, and immediate emergency restore.
- Transparent startup, bounded opacity, display change handling, session-lock hiding, and local settings.
- Core tests, Windows smoke tests, benchmark script, x64/ARM64 CI builds, and unsigned MSIX validation.
- Open-source contribution, security, privacy, architecture, and Store submission documentation.

Microsoft Store publication, signing, and final hardware/accessibility release validation are pending.
