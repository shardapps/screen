# Architecture and safety

Screen uses C++20 with Win32, GDI, common controls, DWM, the Windows Magnification color API, and session notifications. It statically links the C runtime; no redistributable installer or managed/browser runtime is required. Windows still supplies the system DLLs. This baseline intentionally avoids adding a Windows App SDK runtime solely for a small dimming utility.

## Thread ownership

The main thread owns the settings model, native controls, tray icon, registered hotkeys, and the process message loop. It sends complete settings snapshots to `OverlayEngine`.

The worker owns a hidden notification window and all overlay windows. Submission uses a mutex and one atomic pending-message flag. Consecutive slider updates replace the pending snapshot, so a slow worker does not accumulate a queue of intermediate slider states. The worker reads/writes shared submission state under the same mutex. A promise synchronizes startup; shutdown posts a close message and joins the worker before the app exits.

Settings persistence runs on that worker after a committed control change. Thumb-tracking updates remain in memory until release. The worker flushes a temporary settings file and replaces the destination with `MoveFileExW`; failures are reported to the UI. There is no periodic autosave thread.

## Full-desktop color processing

When all attached displays have the same requested level, the worker uses [MagSetFullscreenColorEffect](https://learn.microsoft.com/en-us/windows/win32/api/magnification/nf-magnification-magsetfullscreencoloreffect). The RGB gain is `1 − opacity/255`; optional warm offsets reproduce the overlay tint. This applies to the composited desktop instead of competing in window Z order, addressing full-screen app layers that outrank normal overlays. It does not call zoom, input-transform or cursor APIs. No captured frames or per-frame screen-sized buffers are allocated by Screen. Screen's own flyout is included in the color effect.

`DesktopDimmer` lazily initializes the Windows runtime, records its original color matrix, and composes the dimming after it. It caches its applied matrix, checks ownership before changing/restoring it, and releases the runtime when transparent. Another color client can make writes unavailable; Screen falls back to overlays without replacing that client's transform. A new user adjustment retries the desktop backend. Active-only audits check color state rather than window order in desktop mode.

Different per-display levels cannot be represented by a single full-desktop matrix. Those settings, API failure, and the explicit `--overlay` compatibility option use the existing surfaces below. The active footer identifies Desktop or Overlay. The default command line supports `--background` and `--overlay` together.

## Compatibility display surfaces

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

Opacity uses exact critically damped integration with a monotonic clock and retained velocity. Retargeting first advances the current motion, then changes its target; it does not repeatedly restart a zero-slope ease-in. While moving, a high-resolution one-shot timer wakes the existing render thread at the desktop composition cadence, capped at 144 Hz. Retargeting does not postpone a scheduled frame. A native 15 ms window timer is the fallback if the high-resolution timer cannot be used. All frame scheduling stops when settled. Alpha is applied only when its rounded byte changes. Full surface painting occurs on exposure or a warm-color change, not every slider update. The UI likewise avoids resetting its own thumb or tray tooltip on every drag sample.

Timing follows Windows scheduling and desktop composition; no particular refresh rate is promised. The app never increases global timer resolution or spins waiting for a frame. Warm color changes are immediate. Both opacity and panel transitions respect the user preference and Windows' client-area animation setting.

At zero opacity the compatibility surface is hidden and the desktop color runtime is released. At steady nonzero opacity there is no application animation timer. DWM still incurs processing/composition costs that must be measured separately.

## Compact controls

The UI is a 330 × 136 DPI-scaled tool window, anchored inside the work area of the tray icon's monitor (cursor monitor fallback). It stays above the bottom-right taskbar, without an Alt+Tab/taskbar entry. Alt+S opens/hides it; Ctrl+Alt+S is the fallback when reserved. Native slider keyboard/MSAA/UIA behavior is preserved while custom drawing supplies a quiet rounded track. The footer opens display selection and the settings icon opens an on-demand native settings window. Context/tray controls remain available.

The panel and settings share six named palettes plus System, with high-contrast priority. Opening takes about 160 ms with a small fade/slide; dismissal takes about 90 ms. The flyout uses the same one-shot frame scheduler on its existing UI thread. Timers run only during a transition. Escape or losing focus dismisses the panel, while dimming and tray access remain active.

## Settings and startup

The settings window uses native controls and GDI drawing. Its fonts, brushes and child windows are created on demand and released on close. Short windows scroll; DPI changes rebuild the scaled layout. There is no retained web view, image cache, or idle animation loop. Page highlights and theme palettes use short time-based transitions and respect Windows' animation preference. High contrast takes effect immediately. Sliders share one buffered renderer; native combo boxes have a complete themed first-paint/print path. Native input and accessibility behavior remain available.

Settings snapshots are coalesced on the rendering worker; disk I/O stays off the UI thread. High-frequency slider updates contain only the resolved active display levels; the full desktop-profile map is copied only for persistence. Atomic replacement retries only transient sharing/access failures for up to 155 ms. The selected Windows display name is persisted alongside brightness overrides; reconnecting hardware may require reselecting a display. The 15-minute pause uses a single deadline timer and is cancelled by manual brightness/pause/restore actions. See [virtual desktop profiles](VIRTUAL_DESKTOPS.md) for discovery, fallback and validation limits.

Startup is opt-in. Unpackaged builds register the exact quoted executable in HKCU Run with --background --startup. Uninstall removes it only when it matches this executable. MSIX builds declare a disabled StartupTask and query/change it through C++/WinRT on a short-lived background worker, with bounded asynchronous waits. A small packaged ScreenStartup.exe forwards the quiet arguments to Screen.exe and exits. Windows user/policy decisions take priority. Resume dimming is a separate preference, so quiet startup remains paused by default.

## Recovery

- A fresh process starts paused by default. Resuming saved dimming is an explicit preference; --paused always overrides it.
- Dimming is capped at approximately 92% opacity; corrupt settings cannot create a fully opaque black screen.
- Emergency restore pauses and resets all brightness overrides to 100%, bypassing animation.
- Rendering/window-creation failure destroys every current surface and notifies the UI.
- Session lock releases the desktop color effect, hides overlays immediately and cancels their fade timer. Unlock resumes the current preference.
- Explicit quit restores the prior color state, releases the color runtime, joins the worker, destroys its surfaces, removes the tray icon, and unregisters hotkeys. Windows removes process windows and this client's desktop effect on forced termination; the native regression test exercises that cleanup.
- Explorer restart re-registers the tray icon. If no tray icon can be created, the control window remains reachable.

There is no elevation, `uiAccess`, service, driver, exclusive input hook, or secure-desktop access. `runFullTrust` in the MSIX manifest means a normal desktop process. See [Microsoft's packaging guidance](https://learn.microsoft.com/en-us/windows/msix/desktop/desktop-to-uwp-manual-conversion).

## Limits to validate

No software-only overlay can promise coverage over every Windows surface or hardware configuration. See the README for protected content, HDR, fullscreen, secure-desktop, remote-session, and power-saving limits. The [release checklist](RELEASE_CHECKLIST.md) records manual coverage still required for a public release.
