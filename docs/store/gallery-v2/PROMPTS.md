# Store gallery refresh

Created with the built-in ImageGen tool, October 20, 2026. The background and brand card are generated marketing artwork. Product UI inserts come from native Windows captures; generated compositions are checked against those originals.

## Crescent background — `crescent-art.png`

Use case: ads-marketing. Microsoft Store promotional hero for a Windows screen dimmer utility, landscape 16:9. Sophisticated premium minimalist abstract artwork about bringing a bright screen into a calm evening. A sculptural, softly glowing lavender crescent formed from layered translucent frosted-glass ribbons, gently transitioning from luminous pearl on the left to ink-blue darkness on the right. Large graceful curves, subtle surface texture, exceptionally clean edges and restrained studio lighting. Polished contemporary desktop software branding, calm and precise, not a space scene or stock wallpaper. Wide cinematic negative space, crescent near center, balanced at small thumbnail size, deep charcoal navy background, delicate violet and cool lavender glow. No text, letters, logos, watermarks, fabricated UI, devices, icons, stars or clutter.

## Brand card — `brand-card.png`

Edit target: supplied lavender crescent artwork. Create a premium Microsoft Store landscape brand card. Preserve the translucent crescent material and navy/lavender palette; recompose with the crescent in the right 45% and a quiet deep navy field on the left. Exact left-aligned modern sans-serif text: eyebrow “SCREEN DIMMER”; large headline on two lines “A softer screen.” and “A calmer space.”; subtitle “Native brightness control for Windows 11”; footer “by Shardapps”. Cream-white typography, small lavender accents, careful margins, breathing room, restrained studio lighting. No fake interface, devices, badges, ratings, health claims, performance statistics or additional words. This is promotional art, not a screenshot.

## Capture provenance

`scripts/settings-test.ps1 -StoreScreenshots` renders the unchanged app on a disposable GitHub Actions Windows desktop. Settings use native 150% DPI; the flyout uses the monitor's native DPI at its actual compact size. The test restores its own settings and startup registration. It does not operate the user's desktop.

The raw captures remain available alongside the final marketing compositions for comparison. The listing update changes imagery only; it does not require a new app binary.
