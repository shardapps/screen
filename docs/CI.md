# Continuous integration

The `ci-cd-pipeline-builder` skill's detector and generator were run against the repository. Its built-in signals cover manifests such as package.json, pyproject.toml and go.mod, and returned no language/build commands for this CMake project. The generated baseline had no jobs. The committed workflow is therefore tailored to the actual `CMakeLists.txt`, tests, and packaging script rather than shipping that empty baseline.

## Windows CI

- Runs on pushes to main and feature/fix/docs/CI branches, PRs targeting main, and manual dispatch.
- Builds x64 and ARM64 separately on `windows-2025` with the installed Windows SDK. `scripts/configure.ps1` detects Visual Studio 2022 or 2026 through vswhere and uses its matching generator/bundled CMake instead of assuming the runner's Visual Studio version.
- Verifies deterministic generated artwork, compiles C++20 with warnings-as-errors, runs portable core tests and native-window smoke tests on x64, builds per-user Inno Setup EXEs, and tests x64 installation, Start menu/uninstall registration, app behavior, reinstall and removal.
- User preview artifacts contain the setup EXE, portable ZIP, instructions and SHA-256 checksums. Unsigned MSIX validation packages are uploaded under separate `Screen-msix-validation-*` artifact names so they cannot be confused with normal installers.
- ARM64 is cross-compiled on x64; its executable and tests require a physical or compatible Windows on Arm runner for runtime validation.
- A Linux job tests the portable core with address/undefined-behavior sanitizers.
- Cancels obsolete runs for a branch, limits job time, uses read-only default permissions, and retains preview artifacts for 14 days.

There are no downloaded application dependencies or package lockfiles. The setup compiler is the runner's preinstalled Inno Setup 6.3+; `scripts/installer.ps1` locates it and checks the input PE architecture. Inno Setup is a build-time tool, not an application runtime dependency. Builds are small, so dependency caching is intentionally omitted; caching the few source files or preinstalled Windows SDK would add overhead and stale-toolchain risk. Add a content-keyed dependency cache only if real dependencies are introduced.

## Security and updates

CodeQL analyzes C/C++ on PRs targeting main, main pushes, and a weekly schedule. Dependabot proposes GitHub Actions updates monthly. No production secrets are required by any committed workflow. PR artifact packages have a clearly separate preview identity.

## Deployment gates

These workflows do not publish a GitHub release, sign binaries, upload to Partner Center, or submit to the Store. There is no production deployment job or embedded credential. Before adding one, create a protected production environment, require manual review, limit trusted branch/ref inputs, store signing/submission credentials in environment secrets, and document exact rollout and rollback behavior. See the [release checklist](RELEASE_CHECKLIST.md).

After the first green run, a maintainer should configure main-branch rules to require review and the stable CI job names. Workflow files alone do not enforce branch protection.
