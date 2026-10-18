# Microsoft Store packaging and submission

Screen includes an MSIX manifest for a Windows 11 desktop application. The committed identity is an explicit preview placeholder. A successfully generated unsigned package is not Store certification or a trusted local installer.

For a normal local installation, use the setup EXE described in [installation help](INSTALLATION.md). CI separates setup/portable previews from `Screen-msix-validation-*` artifacts; unsigned MSIX is for packaging work only.

## Build a validation package

```powershell
./scripts/package.ps1 -Architecture x64 `
  -Executable build/x64/Release/Screen.exe `
  -IdentityName Shardapps.Screen.Preview `
  -Publisher 'CN=Shardapps-Preview'
```

For ARM64, pass `-Architecture arm64` and its ARM64 executable. The script requires Windows SDK MakeAppx, creates a fresh staging folder under ignored `build/`, copies the executable/license/assets, updates manifest attributes through an XML DOM, and calls MakeAppx with schema validation enabled. It refuses to overwrite an existing output. Clean only the specific ignored staging/output folders you no longer need.

## Use the real Shardapps identity

Reserve an app in the Shardapps Partner Center account. Copy its exact package identity name and publisher certificate subject from Product identity. Do not infer them from the brand name. Run the script with these explicit values and a monotonically increasing four-part Store version whose first component is at least 1 and whose final component is zero, such as `-Version 1.0.0.0`. The default `0.2.0.0` is for developer preview validation, not Store submission. Update the executable/project versions together for the release.

The manifest targets `Windows.Desktop` starting at Windows 11 build 22000. It declares `runFullTrust` for its Win32 entry point, not elevation. Explain the desktop-wide, click-through overlay behavior and lack of driver/kernel access in certification notes.

See Microsoft's [manual packaging guidance](https://learn.microsoft.com/en-us/windows/msix/desktop/desktop-to-uwp-manual-conversion) and [Partner Center packaging guidance](https://learn.microsoft.com/en-us/windows/apps/publish/publish-your-app/msix/app-package-requirements). Partner Center requirements can change; review current guidance before submission.

## Signing and review

- Store packages must use the reserved identity. Partner Center handles the Store distribution signing process; follow the current submission path for the account.
- Local MSIX installation requires a trusted certificate whose subject matches the manifest Publisher. Do not disable Windows trust checks or commit a private certificate/key.
- Portable EXEs are unsigned previews until Shardapps configures trusted code signing. A Store submission does not automatically sign an independently distributed portable EXE.
- Run Windows App Certification Kit, test install/update/uninstall, inspect capabilities, and complete [release QA](RELEASE_CHECKLIST.md) before submitting.
- Publish the reviewed privacy policy to a stable public URL and include it in the listing. Screenshots must show the actual app; the README hero is labeled artwork, not a certification screenshot.

The initial implementation does not include startup tasks, elevation, a service, drivers, or automatic updating. MSIX/Store distribution can provide managed updates after publication.
