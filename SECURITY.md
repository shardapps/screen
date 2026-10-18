# Security policy

Screen is currently a developer preview. Security fixes target the latest code on `main`; there is no supported stable release yet. Use current builds and review the preview's limitations before deployment.

## Report a vulnerability

Use [GitHub private vulnerability reporting](https://github.com/shardapps/screen/security/advisories/new) when available. Include affected versions/commits, reproduction steps, impact, and a minimal proof of concept. Do not disclose exploits or sensitive information in a public issue.

If private reporting is unavailable, open an issue titled **Request a private security contact**, without vulnerability details. A maintainer can arrange a private channel. Response times depend on maintainer availability; no response-time guarantee is made.

## Security design

The app runs at the user's existing privilege level, without elevation, a driver, kernel access, an input hook, screen capture, or a network client. Display recovery is prioritized: transparent startup, bounded opacity, restore controls, and destruction of overlays on process exit.

The MSIX package declares `runFullTrust` because it hosts a Win32 desktop app. This is not an administrator permission. Unsigned preview MSIX files are validation artifacts, not trusted installers.

Settings are local, treated as untrusted input, and brightness is clamped before use. The app is not a privacy screen or security boundary and does not prevent other software from capturing screen contents.

Please allow maintainers time to investigate and coordinate a fix before publishing a detailed exploit. Supported fixes should be accompanied by an advisory where appropriate.
