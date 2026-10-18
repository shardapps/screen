# Build Screen

## Requirements

- Windows 11 for the app; x64 or ARM64.
- Visual Studio 2022 (17.14+) or Visual Studio 2026 with **Desktop development with C++**, C++ x64/x86 tools, and Windows SDK 10.0.26100 or newer. Install the ARM64 C++ tools to cross-build ARM64.
- CMake 3.25+ for manual configuration; 3.31+ is recommended. Visual Studio 2026 presets require a CMake version that supports its generator.
- Python 3.12+ only to regenerate artwork. There are no Python dependencies or runtime NuGet/npm packages.
- Inno Setup 6.3+ only to build conventional setup EXEs. See its [license](https://jrsoftware.org/files/is/license.txt). The app does not need Inno Setup installed to run.

## Visual Studio 2026 presets

From a developer PowerShell with CMake on PATH:

```powershell
cmake --preset windows-x64
cmake --build --preset release-x64
ctest --preset test-x64
./scripts/smoke-test.ps1 -Executable build/x64/Release/Screen.exe
./build/x64/Release/Screen.exe
```

For ARM64, use `windows-arm64` and `release-arm64`. An x64 host can compile ARM64 but cannot execute its test binary natively. Run the application and tests on an actual Windows on Arm device before release.

## Visual Studio 2022 / CI commands

```powershell
cmake -S . -B build/vs2022 -G "Visual Studio 17 2022" -A x64
cmake --build build/vs2022 --config Release --parallel
ctest --test-dir build/vs2022 -C Release --output-on-failure
./scripts/smoke-test.ps1 -Executable build/vs2022/Release/Screen.exe
```

Replace `x64` with `ARM64` and use a separate build directory for a cross-build. Never mix generators or architectures in one CMake build directory. If CMake is not on PATH, use the version bundled in Visual Studio's `Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin` directory.

## Tests and diagnostics

Core tests check opacity boundaries, pause, brightness clamping, animation endpoints/monotonicity, and monitor override isolation. The Windows smoke test launches the real executable, checks overlay styles, hit-testing, and startup alpha, moves test surfaces offscreen, verifies actual bounded nonzero opacity and emergency restore, then checks close-to-tray and shutdown. It preserves the existing settings file and refuses to run alongside an existing instance. It does not validate actual keyboard delivery, lock/unlock, physical display reconnection, or HDR.

The portable core also builds on Linux/macOS with CMake and a C++20 compiler. Only the Windows executable controls a screen.

```sh
cmake -S . -B build/core -DCMAKE_BUILD_TYPE=Debug
cmake --build build/core
ctest --test-dir build/core --output-on-failure
```

Run `python scripts/generate_assets.py` after changing the deterministic icon generator. The PNG/ICO files are committed and CI verifies reproducibility. See [performance](PERFORMANCE.md) for measurement commands and [Store packaging](STORE.md) for MSIX.

## Build and test a normal setup EXE

```powershell
./scripts/installer.ps1 -Architecture x64 -Executable build/x64/Release/Screen.exe
./scripts/test-installer.ps1 -Installer dist/Screen-Setup-0.1.0-x64.exe
```

The installer helper discovers ISCC.exe in standard locations or accepts `-InnoCompiler`. It checks the executable's architecture and refuses to overwrite an existing setup output. The test uses a unique folder under `build/`, installs for the current user, verifies the Start menu and uninstall entry, runs the real app smoke test, reinstalls, and uninstalls. It refuses to run if Screen is already installed or running, and preserves preferences through the app smoke test. Use a clean test profile on Windows on Arm to test the ARM64 installer.

## Layout

| Path | Purpose |
| --- | --- |
| `src/main.cpp` | Native controls, tray, shortcuts, DPI handling, recovery |
| `src/overlay.*` | Worker thread, monitor surfaces, animation, session/display events |
| `src/core.hpp` | Portable brightness and interpolation logic |
| `src/settings.*` | Local, bounded settings and atomic file replacement |
| `resources/` | Executable icon, version resource, process manifest |
| `packaging/` | MSIX manifest and generated brand assets |
| `scripts/` | Repeatable asset, smoke-test, benchmark, packaging commands |
| `tests/` | Portable behavior tests |
| `docs/` | Architecture, validation, Store, launch, visual assets |
| `build/`, `dist/` | Ignored local outputs |

All development occurs on branches and is submitted through focused PRs. No Store credentials or signing keys belong in this repository.
