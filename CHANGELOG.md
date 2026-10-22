# Changelog

## 1.1.0 — Alpha 2

- Add opt-in brightness profiles for named Windows virtual desktops, with independent physical-monitor overrides and a shared fallback.
- Add a Desktops settings page to edit detected profiles without switching the Windows desktop.
- Keep desktop discovery event-driven and release its observer when it is no longer needed.
- Fix the display picker's blank white first paint, refine all six palettes and clarify the two startup preferences.
- Add short time-based theme/tab transitions and animation-only high-resolution pacing for brightness and the flyout.
- Compare paused memory/CPU with the Store alpha in Windows CI; add no framework or permanent frame loop.

## 1.0.0 — Alpha 1

- Prepare the first Microsoft Store alpha as Screen Dimmer by Shardapps.
- Use the reserved Store product identity, x64/ARM64 packages and opt-in startup task.
- Add Store listing copy, real screenshots, a 300 px tile and separate promotional artwork.
- Preserve existing local preferences and the familiar compact controls.


## 0.4.0 — 2026-10-19

- Add on-demand Displays, Appearance and Preferences settings, plus a direct flyout screen picker.
- Add six palettes (Pearl, Midnight, Graphite, Ocean, Forest, Sunset), System following Windows, and high-contrast priority.
- Add screen identification, brightness presets, a 15-minute pause, configurable adjustment steps and opening shortcut.
- Add opt-in quiet Windows startup with separate saved-dimming resume; matching-path uninstall cleanup and MSIX StartupTask support.
- Share the buffered native slider renderer, release settings resources on close, and keep transitions short with no idle UI timer.
- Retry transient atomic preference-file replacement conflicts on the worker; add settings/startup/resource-lifetime regression checks to CI.

## 0.3.0 — desktop dimming for full-screen apps

- Use Windows' full-screen color transform for uniform brightness across all displays, avoiding the window-order limitation of a black overlay over full-screen app surfaces.
- Retain the 8% floor, smooth transitions, warm tint, pause and immediate restore; the desktop effect includes Screen's own panel.
- Keep the overlay renderer for different per-display brightness levels, a busy/unavailable color API, or the explicit `--overlay` compatibility option. The flyout reports Desktop or Overlay while active.
- Preserve existing color state and restore on pause, lock and quit. Test forced-exit cleanup and coexistence with another color client.
- Add portable color-matrix tests and native desktop-effect/installed-app regression checks. No elevation, driver, zoom, screen capture or hardware brightness changes are required.

## 0.2.1 — slider polish

- Remove stale thumb trails and native trackbar fragments during dragging with complete buffered slider frames.
- Replace the dotted focus rectangle with a small solid ring around the thumb when using the keyboard; retain native keyboard and accessibility support.
- Align the rounded flyout region with its actual bounds.

## 0.2.0 — compact controls and reliable dimming

- Replace the large window with a 330 × 136 flyout anchored above the bottom-right taskbar.
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

## 0.1.0 — developer preview

- Native Windows 11 overlay dimming with per-display brightness and optional warm tint.
- Dedicated overlay thread, coalesced state updates, and time-based smooth transitions.
- System tray controls, global shortcuts, and immediate emergency restore.
- Transparent startup, bounded opacity, display change handling, session-lock hiding, and local settings.
- Core tests, Windows smoke tests, benchmark script, x64/ARM64 CI builds, and unsigned MSIX validation.
- Open-source contribution, security, privacy, architecture, and Store submission documentation.

Microsoft Store publication, signing, and final hardware/accessibility release validation are pending.
