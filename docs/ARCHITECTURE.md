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

`WM_DISPLAYCHANGE` rebuilds surfaces against the current topology. Windows display device names identify per-display overrides; these names are not permanent physical monitor identities. Removed entries stay local and are bounded before use. No monitor serial numbers are queried.

## Animation and idle work

For brightness `b`, opacity is `round((100 − clamp(b, 15, 100)) × 255 / 100)`. Pause overrides opacity to zero. Warm tint replaces black with RGB `(66,30,8)`; its displayed luminance is not numerically equivalent to a black overlay at the same setting.

Transitions interpolate from the last applied alpha to the target with `t²(3−2t)` over 200 ms using a monotonic clock. A 15 ms Windows timer runs only while a surface is changing. Timing follows Windows scheduling and desktop composition; it does not promise a particular refresh rate. The app never increases the global timer resolution or spins waiting for a frame. Warm color changes are immediate; the alpha transition is eased.

At zero opacity the surface is hidden. At steady nonzero opacity it remains visible, without an application animation timer. DWM still incurs composition costs that must be measured separately.

## Recovery

- A fresh process always starts paused, retaining brightness preferences without automatically applying them.
- Dimming is capped at 85% opacity; corrupt settings cannot create a fully opaque black screen.
- Emergency restore pauses and resets all brightness overrides to 100%, bypassing animation.
- Rendering/window-creation failure destroys every current surface and notifies the UI.
- Session lock hides overlays immediately and cancels their fade timer. Unlock resumes the current preference.
- Explicit quit joins the worker, destroys its surfaces, removes the tray icon, and unregisters hotkeys. Windows removes process windows if the process crashes or is terminated.
- Explorer restart re-registers the tray icon. If no tray icon can be created, the control window remains reachable.

There is no elevation, `uiAccess`, service, driver, exclusive input hook, or secure-desktop access. `runFullTrust` in the MSIX manifest means a normal desktop process. See [Microsoft's packaging guidance](https://learn.microsoft.com/en-us/windows/msix/desktop/desktop-to-uwp-manual-conversion).

## Limits to validate

No software-only overlay can promise coverage over every Windows surface or hardware configuration. See the README for protected content, HDR, fullscreen, secure-desktop, remote-session, and power-saving limits. The [release checklist](RELEASE_CHECKLIST.md) records manual coverage still required for a public release.
