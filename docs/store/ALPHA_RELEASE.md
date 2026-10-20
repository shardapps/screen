# Alpha 1 release evidence

The product name is **Screen Dimmer by Shardapps**. The Store publisher is **ShardCoders**, product `9PNHDHKMHDLG`, package version `1.0.0.0`.

## Microsoft Store submission

Submitted on **October 20, 2026**. Partner Center reports **In certification** for Submission 1 (`1152921505702173182`): submission complete, pre-processing in progress, certification and publishing next. It confirms the product will start publishing automatically after it passes certification. This records submission, not approval or public availability.

- Both x64 and ARM64 MSIX packages passed Microsoft's package validation.
- Pricing is free, with worldwide availability and a discoverable public listing.
- The English (United States) listing includes the description, use cases, eight features, seven keywords, four captioned native screenshots and the 300 px Store icon.
- Properties, privacy policy and age ratings are complete. IARC generated Everyone / 3+ ratings; its terms and age-of-majority declaration were accepted with the publisher's explicit confirmation.
- Reviewer instructions explain desktop color processing, per-monitor overlay limits, optional startup, emergency restore and the absence of personal-data collection. Partner Center requires a privacy policy based on package capabilities, so the public policy is supplied.
- [Store product page](https://apps.microsoft.com/detail/9PNHDHKMHDLG) may remain unavailable until publication.

## Reproducible validation

- App/source commit: `481efea2d64e75e1369c21e62e70da8e7bbe9a13`.
- [Windows CI run 39298092333](https://github.com/shardapps/screen/actions/runs/39298092333): x64 and ARM64 compile with warnings as errors; core tests, Linux ASan/UBSan, native overlay and full-desktop color behavior, settings/themes/startup, repeated UI resource lifetime, setup install/reinstall/uninstall and MakeAppx validation passed.
- [CodeQL run 39298092843](https://github.com/shardapps/screen/actions/runs/39298092843): passed.
- Four screenshots in `screenshots/` are unmodified native window captures from that CI run, 1440 by 900 pixels each. The isolated runner uses a 1920 by 1080 desktop; the user's desktop and preferences are not involved. Visually checked for blank panels, clipped content and selection states.
- Store package metadata and PE architecture were independently inspected after download. Both packages contain Screen.exe and ScreenStartup.exe, the exact reserved identity, ShardCoders publisher display name and version 1.0.0.0.

| Architecture | MSIX bytes | SHA-256 |
| --- | ---: | --- |
| x64 | 309296 | `3b675f6f8d5ba056c62327c860ed26e82724f4f34a8f6424724f5d9495fa996f` |
| ARM64 | 281778 | `ef90b2b8788c7bafba5213b2c83591b9a17900fc4feb92b443f7ea26f3bf4a20` |

## Review and practical limits

Release review covered package identity/version consistency, preservation of settings/startup identifiers, normal-user permissions, explicit startup consent, native settings lifetime and direct settings launch. No new runtime dependency, network client, secret, elevation, driver or service is introduced. Store package signing is left to Microsoft's distribution process.

This is alpha validation, not a claim that every item in the broader release checklist has passed. Physical ARM64 behavior, broad GPU/HDR/protected-video/exclusive-fullscreen combinations, real sign-in StartupTask delivery and the full manual accessibility matrix remain unverified for this release. Windows App Certification Kit has not completed on these unsigned Store packages; no local certificate trust or Windows security policy was changed. Microsoft certification is still required. Listing copy calls out software brightness and coverage limits.

`alpha-promo.png` is generated marketing artwork, not an app screenshot. Its dimensions do not match Store's optional hero slot, so it is not used for that slot. `alpha-art-prompt.md` records the built-in ImageGen prompt. The Store tile is the deterministic 300 px app icon.
