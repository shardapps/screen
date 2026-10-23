# Quick controls and motion

Open the mini panel with Alt+S. The four percentage buttons apply a saved level to the selected display in the current desktop profile. Their defaults are 25%, 50%, 75% and 100%. In Settings â†’ Displays, choose a screen, adjust its slider, then choose **Save current level to a quick presetâ€¦** and the slot to replace. Slots are shared across screens/desktops; applying them respects the current selection. Duplicate levels are allowed, and the usual 8% floor remains.

The pin icon keeps a manually opened panel visible when you switch apps. Its accessible name changes to **Unpin mini panel**. Settings â†’ Preferences offers the same switch. Escape, Close and Alt+S can still dismiss a pinned panel; opening Settings also hides the panel. Pinning does not open it during quiet startup. Passive keyboard feedback remains brief until you interact with it, even if pinning was saved.

Pause/resume is available directly in the flyout. Settings â†’ Displays offers a pause length of 5, 15, 30 or 60 minutes. Choose a duration, then **Pause for â€¦ minutes**. The current brightness values are retained; the existing one-shot pause deadline resumes dimming. Changing the duration affects the next pause, not an already running timer. Moving a slider, choosing a quick level or explicitly resuming cancels a timed pause. The runtime deadline is not restored after the app exits.

A remaining-time label updates while the panel or Settings is visible. Its update timer stops when both are hidden/minimized, while the pause deadline still expires in the background. Windows brightness compatibility does not override an active timed pause. Restore/emergency recovery disables compatibility following as before.

The source label under App brightness identifies software dimming, native keyboard control or Windows-linked compatibility. The percentage remains the app's level, not a hardware-backlight reading. Windows-linked compatibility can also respond to Windows-slider and automatic changes, as described in [brightness controls](BRIGHTNESS_KEYS.md).

Motion is shared by all themes: brief hover/focus tint, an expanding slider thumb, short page movement, theme-color transitions and the existing flyout/renderer fades. Smooth transitions off, Windows reduced-motion settings and high contrast take priority. There is no permanent animation loop, blur texture, bundled browser or new runtime. Native keyboard focus, accessible button labels and the emergency shortcut remain available.

These controls ship in the 1.3.1 public beta after hands-on 1.3.0 acceptance. Hosted screenshots and regression checks supplement local core/lifecycle tests; broad hardware and human motion-quality acceptance remain separate from a build passing.
