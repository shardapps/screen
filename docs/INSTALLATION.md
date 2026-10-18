# Install or run Screen on Windows 11

## Normal setup

Download the `Screen-preview-x64` CI artifact for Intel/AMD Windows, or `Screen-preview-arm64` for Windows on Arm. Extract it and open `Screen-Setup-0.3.0-x64.exe` or its ARM64 equivalent.

Setup installs under `%LOCALAPPDATA%\Programs\Shardapps\Screen`, creates **Shardapps → Screen by Shardapps** in the Start menu, and registers a per-user uninstall entry. A desktop shortcut is optional. It needs no administrator rights and does not add certificates or change trust settings. Close Screen using its tray menu before reinstalling, upgrading, or uninstalling it.

This remains an unsigned developer preview. Windows may show SmartScreen or unknown-publisher warnings. If Windows or an organization's application-control policy blocks unsigned executables, use the organization's approved process or wait for a signed/Store distribution. Setup does not bypass those policies.

## Portable app

Extract `Screen-portable-x64.zip` or the ARM64 ZIP completely into a folder, then run `Screen.exe`. A portable ZIP is not an installer and creates no Start menu or uninstall entry.

Both forms start paused. Move the slider to apply dimming. **Alt+S** opens/hides the compact panel above the bottom-right taskbar; **Ctrl+Alt+S** is the fallback when Alt+S is reserved. Click outside or press Escape to dismiss it. Right-click the tray icon or panel for display/tint settings, pause, restore and **Quit**. Emergency restore is **Ctrl + Alt + Shift + F12**.

## Why the MSIX fails to install

The `Screen-msix-validation-*` artifact contains an **unsigned** MSIX with a preview package identity. It proves the package layout/schema and is reserved for packaging development. It is not a trusted double-click installer. This was bundled with the original preview downloads and could easily be mistaken for the normal installer; it now has its own clearly labeled artifact.

Normal MSIX installation needs a valid package signature trusted by Windows. Microsoft Store distribution supplies the Store signing path after submission/certification. See [Microsoft's signing guidance](https://learn.microsoft.com/en-us/windows/msix/package/signing-package-overview) and our [Store guide](STORE.md). Use the conventional setup EXE or portable ZIP to try this preview without certificate configuration.

## Uninstall and preferences

For a setup installation, quit Screen, then use **Windows Settings → Apps → Installed apps → Screen by Shardapps → Uninstall**. Portable users can quit and delete their extracted folder.

Both preserve preferences at `%LOCALAPPDATA%\Shardapps\Screen\settings.ini`. Delete that file with Screen closed if you also want to remove preferences. Setup does not enable automatic startup or change hardware brightness.
