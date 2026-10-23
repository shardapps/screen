# Keyboard brightness keys — beta

Open **Settings → Keyboard** and choose **Test brightness keys**. Press your keyboard's sun/brightness-up and brightness-down keys, using Fn if required. The test lasts up to 15 seconds, reports which keys Windows exposed, and does not change Screen Dimmer's brightness. Your laptop's normal hardware response may still occur during the test. Close settings or choose Stop test to end it early.

**Use keyboard brightness keys (beta)** is off by default. Turn it on after testing. Each press changes app brightness for the selected physical display in the active Windows desktop profile, using the adjustment step from Preferences. Holding a supported key repeats at the Windows keyboard rate, with a three-second safety cap per uninterrupted hold. Release and press again to continue.

**Show mini panel for keyboard changes** is a separate preference, initially on. It applies to supported sun keys and the existing Ctrl+Alt+Up/Down fallback shortcuts. The panel says **App brightness** and **Screen Dimmer**, appears above the taskbar without taking keyboard focus, and fades after two seconds without another adjustment. Hovering keeps it available; clicking it or opening it with Alt+S turns it into the normal interactive panel. Turning feedback off leaves keyboard adjustment working without opening the panel.

Ctrl+Alt+Up/Down continues to adjust **all displays in the current desktop profile**, even when native brightness-key support is off. Alt+S opens the panel; Ctrl+Alt+Shift+F12 restores immediately. The ordinary selected-screen slider and full-screen rendering paths are unchanged.

## Compatibility and scope

This listens to the standard HID Consumer Control collection and its brightness increment/decrement usages (`0x0C:0x6F` / `0x0C:0x70`). Some Fn keys use OEM firmware, ACPI or vendor services and are not delivered as these reports. A successful capability scan means a connected HID collection advertises the usages; the interactive test is still needed to establish actual key delivery. The test's result is session-only and never saved as a hardware certification.

Raw Input observes these controls; it cannot universally suppress Windows' or firmware's hardware-brightness handling. Both effects can occur. If that feels too strong, disable the opt-in and use the fallback shortcuts. Screen does not install a driver, request administrator rights, hook ordinary typing, reprogram keys, replace the Windows brightness slider, or infer keypresses from unrelated automatic hardware-brightness changes.

The listener and system HID parser are loaded on demand and released when disabled/test completion permits. There is no polling thread or idle input timer. Only a held brightness key arms the bounded repeat timer; the feedback timer exists only while the panel is visible. Tests use the same app settings and public shortcut path in an isolated CI session, plus native registration lifetime and portable repeat/bounds checks. Physical Fn delivery, OEM side effects and varied USB/Bluetooth keyboards still require hands-on acceptance testing.

For a read-only capability count, run `screen_windows_tests.exe --inspect-brightness-keys` from a local build. It does not register an input listener, print device names/identifiers, change brightness or save settings.

References: [Microsoft brightness HID conventions](https://learn.microsoft.com/windows-hardware/drivers/hid/display-brightness-control), [Raw Input registration flags](https://learn.microsoft.com/windows/win32/api/winuser/ns-winuser-rawinputdevice), [HID report parsing](https://learn.microsoft.com/windows-hardware/drivers/ddi/hidpi/nf-hidpi-hidp_getusages).
