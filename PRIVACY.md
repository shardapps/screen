# Privacy policy — Screen by Shardapps

Effective date: October 18, 2026. Applies to the source and v0.2.1 developer preview in this repository.

Screen does not collect, transmit, sell, or share personal data. It has no account system, telemetry, advertising, analytics SDK, network client, screenshot capture, or keystroke recording.

## Local data

Screen saves brightness, per-display brightness overrides, warm tint, and smooth-transition preferences in a local settings file. Portable builds use `%LOCALAPPDATA%\Shardapps\Screen\settings.ini`; Windows may virtualize this location in an MSIX installation. Per-display overrides contain Windows display device names, not monitor serial numbers. Each launch starts paused.

The app reads display geometry to position overlays and receives Windows display/session and window-change notifications. It inspects local window handles, process IDs, bounds, visibility and stacking order to maintain coverage; it does not read or log other apps' titles or contents. It registers the documented global shortcuts without installing a keyboard hook or injecting code into other apps. This state is processed locally.

Close Screen and delete its settings file to reset or remove preferences. Removing a portable executable does not automatically remove its settings. Packaged uninstallation follows Windows' app-data handling.

## Third-party services and reports

GitHub hosts the repository, issues, workflow artifacts, and downloads under GitHub's own terms and privacy policy. A future Microsoft Store distribution would also be subject to Microsoft's terms and applicable diagnostics. These services are separate from Screen's application behavior.

If you choose to submit an issue, screenshot, diagnostic result, or benchmark, it is shared through the service you use. Remove personal information before posting. Screen does not submit reports automatically.

## Changes and questions

Material changes to application data handling must update this policy before release. For questions, open a [repository discussion or issue](https://github.com/shardapps/screen/issues). Do not include personal or sensitive information in a public report.
