# Contributing to Screen

Thank you for helping make a small, reliable Windows screen dimmer. We welcome bug fixes, accessibility improvements, performance measurements, documentation, and hardware compatibility reports.

## Before changing code

Check existing issues and open a focused issue for a substantial feature. Include the problem, expected behavior, and constraints. Small fixes can go straight to a pull request. Keep runtime dependencies, background work, and binary size to a minimum.

## Workflow

1. Fork or branch from the appropriate base. Use a descriptive branch such as `fix/display-reconnect`.
2. Keep each task in its own pull request. Do not push changes directly to `main`.
3. Build and run the relevant checks in [development](docs/DEVELOPMENT.md). Report what you tested and any missing hardware.
4. Explain the user-visible problem, resulting behavior, validation, and limitations. Include a screenshot for UI changes and measurements for performance claims.
5. Wait for review and green required checks. Maintainers decide when a preview is ready for release.

## Engineering expectations

- Use C++20, RAII, clear ownership, Unicode Windows APIs, and warnings-as-errors.
- Keep all overlay operations on the worker thread and all UI operations on the UI thread. Shared state must be synchronized.
- Preserve click-through input, transparent startup, the brightness floor, emergency restore, and fail-open cleanup.
- Avoid busy polling, timer-resolution changes, injected/input hooks, drivers, elevated privileges, and runtime network calls. Preserve the small active-only coverage audit: silent topmost promotions do not always emit usable window events. Event callbacks must be filtered/coalesced, and the audit must stop while paused or at 100%.
- Prefer existing Windows APIs over adding a library. Explain any dependency's size, license, security, and performance impact.
- Add meaningful tests for changed brightness, persistence, lifecycle, or concurrency behavior. Do not add tests that only repeat implementation details.
- Test keyboard navigation, high DPI, monitor reconnection, and recovery when changing UI or overlay behavior.

## Reports and conduct

Do not upload private screenshots, usernames, serial numbers, tokens, or machine identifiers. Use [private security reporting](SECURITY.md) for vulnerabilities. Be respectful and follow the [code of conduct](CODE_OF_CONDUCT.md).

By submitting a contribution, you agree that it may be distributed under this repository's Apache-2.0 license and confirm you have the right to contribute it. No separate CLA is currently required.
