# Architecture and safety

Screen uses C++20 with Win32, GDI, common controls, DWM, and Windows session notifications. It statically links the C runtime; no redistributable installer or managed/browser runtime is required. Windows still supplies the system DLLs. This baseline intentionally avoids adding a Windows App SDK runtime solely for a small dimming utility.

## Thread ownership

The main thread owns the settings model, native controls, tray icon, registered hotkeys, and the process message loop. It sends complete settings snapshots to `OverlayEngine`.

The worker owns a hidden notification window and all overlay windows. Submission uses a mutex and one atomic pending-message flag. Consecutive slider updates replace the pending snapshot, so a slow worker does not accumulate a queue of intermediate slider states. The worker reads/writes shared submission state under the same mutex. A promise synchronizes startup; shutdown posts a close message and joins the worker before the app exits.

Settings persistence runs on that worker after a committed control change. Thumb-tracking updates remain in memory until release. The worker flushes a temporary settings file and replaces the destination with `MoveFileExW`; failures are reported to the UI. There is no periodic autosave thread.

## Display surfaces

`EnumDisplayMonitors` and `GetMonitorInfoW` supply each display's full bounds, including negative desktop coordinates. One borderless surface per monitor uses:

| Style / behavior | Purpose |
| --- | --- |
| `WS_EX_LAYERED` + `SetLayeredWindowAttributes` | Compositor-backed constant opacity |
| `WS_EX_TRANSPARENT` | Route pointer input through the overlay |
| `WS_EX_NOACTIVATE` + `MA_NOACTIVATE` | Do not take focus |
| `WS_EX_TOOLWINDOW` | Exclude overlays from taskbar and Alt+Tab |
| `HWND_TOPMOST` | Cover ordinary composited app windows |
| `HTTRANSPARENT` | Additional hit-test defense |

See Microsoft's [layered window documentation](https://learn.microsoft.com/en-us/windows/win32/winmsg/window-features#layered-windows) and [opacity API](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setlayeredwindowattributes). Topmost is not an absolute ordering guarantee against every other topmost or protected surface.

`WM_DISPLAYCHANGE` and power-resume notifications rebuild surfaces against the current topology. Windows display device names identify per-display overrides; these names are not permanent physical monitor identities. Removed entries stay local and are bounded before use. No monitor serial numbers are queried.

Out-of-context [WinEvent notifications](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwineventhook) for foreground, top-level visibility/order, topmost location and desktop changes queue one short coverage check. Callbacks skip this process, filter out child/control events and coalesce bursts. The engine checks visible intersecting windows above a surface before reasserting `HWND_TOPMOST`, without activation. The compact control panel is kept above Screen's own overlays so it stays readable.

Some already-visible, nonactivating windows can change order without emitting a usable notification. A 250 ms active-only audit checks for that case and lost overlay handles. It reads order/visibility, without repainting or changing opacity when stable. It stops while paused, locked or at 100% brightness. Protected/shell bands that remain above the overlay after a repair are cached until a relevant external change, preventing a self-generated reorder loop. This remains ordinary user-level window ordering, not a guarantee over every Windows band.

## Animation and idle work

For brightness `b`, opacity is `round((100 − clamp(b, 8, 100)) × 255 / 100)`. Pause overrides opacity to zero. Warm tint replaces black with RGB `(66,30,8)`; its displayed luminance is not numerically equivalent to a black overlay at the same setting.

Opacity uses exact critically damped integration with a monotonic clock and retained velocity. Retargeting first advances the current motion, then changes its target; it does not repeatedly restart a zero-slope ease-in. A 15 ms animation timer is started once and stopped when settled. Alpha is applied only when its rounded byte changes. Full surface painting occurs on exposure or a warm-color change, not every slider update. The UI likewise avoids resetting its own thumb or tray tooltip on every drag sample.

Timing follows Windows scheduling and desktop composition; no particular refresh rate is promised. The app never increases global timer resolution or spins waiting for a frame. Warm color changes are immediate. Both opacity and panel transitions respect the user preference and Windows' client-area animation setting.

At zero opacity the surface is hidden. At steady nonzero opacity it remains visible, without an application animation timer. DWM still incurs composition costs that must be measured separately.

## Compact controls

The UI is a 330 × 136 DPI-scaled tool window, anchored inside the work area of the tray icon's monitor (cursor monitor fallback). It stays above the bottom-right taskbar, without an Alt+Tab/taskbar entry. Alt+S opens/hides it; Ctrl+Alt+S is the fallback when reserved. Native slider keyboard/MSAA/UIA behavior is preserved while custom drawing supplies a quiet rounded track. Display selection, tint, pause, restore and quit live in the context/tray menu.

The panel uses system-aware light/dark/high-contrast colors. Opening takes about 160 ms with a small fade/slide; dismissal takes about 90 ms. These timers run only during the transition. Escape or losing focus dismisses the panel, while dimming and tray access remain active.

## Recovery

- A fresh process always starts paused, retaining brightness preferences without automatically applying them.
- Dimming is capped at approximately 92% opacity; corrupt settings cannot create a fully opaque black screen.
- Emergency restore pauses and resets all brightness overrides to 100%, bypassing animation.
- Rendering/window-creation failure destroys every current surface and notifies the UI.
- Session lock hides overlays immediately and cancels their fade timer. Unlock resumes the current preference.
- Explicit quit joins the worker, destroys its surfaces, removes the tray icon, and unregisters hotkeys. Windows removes process windows if the process crashes or is terminated.
- Explorer restart re-registers the tray icon. If no tray icon can be created, the control window remains reachable.

There is no elevation, `uiAccess`, service, driver, exclusive input hook, or secure-desktop access. `runFullTrust` in the MSIX manifest means a normal desktop process. See [Microsoft's packaging guidance](https://learn.microsoft.com/en-us/windows/msix/desktop/desktop-to-uwp-manual-conversion).

## Limits to validate

No software-only overlay can promise coverage over every Windows surface or hardware configuration. See the README for protected content, HDR, fullscreen, secure-desktop, remote-session, and power-saving limits. The [release checklist](RELEASE_CHECKLIST.md) records manual coverage still required for a public release.
