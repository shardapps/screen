# Release checklist

Developer preview builds are not a substitute for this release gate. Record the tested commit, machine/OS, results, and evidence in the release PR.

## Automated

- [ ] x64/ARM64 compile, x64 core/native smoke tests, Linux sanitizer tests, and CodeQL are green.
- [ ] Both MSIX architectures pass MakeAppx validation and Windows App Certification Kit.
- [ ] Executable, resource, CMake project, and MSIX versions agree; release notes are accurate.
- [ ] No signing material, credentials, private machine paths, or unsupported performance claims are published.

## Windows behavior

- [ ] Start paused, move brightness slider, verify 15% floor, warm tint, smooth on/off, and restore at every setting.
- [ ] Verify click-through input and keyboard focus in browsers, editors, video apps, and borderless windows.
- [ ] Test real shortcut delivery, intentional conflicts, tray activation by mouse/keyboard, close-to-tray, explicit quit, and a second launch.
- [ ] Test Explorer restart, normal shutdown, forced termination, and failed settings writes.
- [ ] Test lock/unlock, sleep/wake, docking/undocking, reconnect, display rotation, and primary display changes.
- [ ] Test at least two displays with different scaling and negative desktop coordinates; verify independent settings and All displays reset.
- [ ] Test physical x64 and ARM64 machines; record GPU/driver and display topology.
- [ ] Verify HDR, protected video, exclusive fullscreen, remote desktop and other topmost apps. Document bypasses clearly.
- [ ] Confirm UAC/secure desktop/sign-in remain outside the overlay's scope.

## Accessibility and performance

- [ ] Keyboard-only navigation, Narrator names/values, slider arrow/page keys, focus visibility, and scroll reachability.
- [ ] 100%, 150%, 200%, and mixed DPI; short work areas, text scaling, high contrast, and system theme behavior.
- [ ] Actual screenshots and visual inspection confirm no clipping, overlap, blank panels, or hidden recovery controls.
- [ ] Repeat measurements for foreground, paused tray, active dimming and rapid slider use; include compositor/GPU costs and multi-monitor 4K setups.

## Distribution

- [ ] Real Shardapps Partner Center identity and publisher values are used, with signing material kept private.
- [ ] Install, launch, update, uninstall and preference cleanup behavior are verified for both packages.
- [ ] Review Store listing copy, categories, age rating, privacy URL, contact channel, screenshots and certification notes.
- [ ] Release PRs are reviewed and merged by a maintainer; signed release/submission receives explicit approval.
- [ ] Update README/CHANGELOG with real download links and actual Store status only after publication.
