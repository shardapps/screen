# Windows virtual desktop brightness

Open Settings → Desktops and enable **Different brightness for each Windows desktop**. The current Task View desktop is identified by name. Select a detected desktop in the profile picker and set its default brightness; editing an inactive profile does not move your windows or change the active desktop's brightness.

Switch desktops normally with Windows. Screen applies that desktop's profile automatically. Use Displays while a desktop is active to adjust physical monitors separately within its profile. Themes, warmth, pause, startup and shortcuts remain shared preferences.

New desktops inherit the shared brightness and monitor settings, rather than copying the last desktop visited. Turning the feature off restores the shared settings and retains saved profiles. **Reset this profile to shared brightness** removes the selected desktop override. Emergency restore pauses dimming everywhere and resets the active desktop's display levels; other saved desktop profiles remain available.

Profiles are keyed by the Windows desktop GUID, so renaming a desktop keeps its settings. Deleted desktop profiles are retained until reset; up to 64 profiles are stored. A newly created desktop receives a new identity. A Windows session that replaces desktop identities can therefore fall back to shared brightness. Startup still requires both **Start with Windows** and saved-dimming resume if you want automatic dimming at sign-in.

## Native implementation and limits

Detection reads Explorer's desktop IDs, names and current-desktop metadata. This shell registry layout is not a Windows API contract and is treated as optional. Registry notifications use [RegNotifyChangeKeyValue](https://learn.microsoft.com/windows/win32/api/winreg/nf-winreg-regnotifychangekeyvalue); no polling timer or shell registry writes are added. Existing foreground events provide a second refresh opportunity. If metadata is absent, the public [IVirtualDesktopManager](https://learn.microsoft.com/windows/win32/api/shobjidl_core/nn-shobjidl_core-ivirtualdesktopmanager) can identify the foreground window's desktop. If identification fails, the UI reports it and shared brightness applies.

Only the public desktop manager moves Screen's own flyout or overlay windows when necessary; it never changes another app's desktop, creates desktops, injects code or uses private shell COM interfaces. Window movement is best effort on the current Windows build. Registry/thread-pool wait handles and COM objects are released on shutdown, and the discovery observer is not retained in the ordinary shared-brightness tray mode after settings close. Existing overlay/full-screen/HDR limitations still apply.

## Validation

Portable tests cover desktop isolation, shared inheritance, monitor overrides, inactive-profile editing, resets and invalid IDs. Native tests cover bounded INI round trips, old preferences, GUID/name metadata, repeated registry notification re-arming, missing identity, absence of idle callback loops and handle cleanup. Registry tests use a private process-specific fixture under Shardapps, never Explorer's real desktop keys.

The settings regression checks opt-in behavior, discovery status, profile controls and persistence when the hosted Windows desktop exposes an identity. Physical Windows 11 Task View switching, desktop deletion/renaming, Explorer restart and multi-monitor transitions remain manual acceptance cases; an isolated registry fixture does not establish those end-to-end behaviors.
