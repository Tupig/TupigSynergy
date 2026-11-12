# Build Guide / 编译指南

> **Language / 语言**: [English](#english) | [中文](#中文)

---

## English

### Ubuntu CI APT bootstrap

The Linux CI job keeps its existing container images. On Ubuntu only, its pre-checkout Git bootstrap
changes active official archive/security `/ubuntu` and ports `/ubuntu-ports` source URIs from HTTP to
HTTPS, preserving hosts, package selection, signing keys, disabled stanzas and other source text.
A single public CA bundle from the Ubuntu runner is mounted read-only at
`/run/synergy-ci/host-ca-certificates.crt`; it is not a private key or a custom trust root. The bootstrap
requires a readable, nonempty regular CA file before changing sources, and passes it temporarily to
APT with `Acquire::https::CAInfo`. Peer, hostname and repository-signature verification stay enabled.
It installs `ca-certificates` alongside Git tools; later dependency installation uses the container's
normal system CA and the persisted HTTPS sources. Debian and other distribution commands are unchanged.

APT bootstrap phases and errors remain visible, and failures stop checkout. The 20-minute job limit
is unchanged; no retry loop or HTTP fallback is added. Offline fixtures establish rewrite and command
behavior only: runner CA availability, TLS acquisition, actual package installation and any ARM
provisioning timeout need separate native CI evidence. A reviewed rollback of this CI/docs change
restores the earlier transport and its possible HTTP failures. Local manifest release builds and
the documented CI dependency exception retain their existing dependency policy.


### Debian-family CI dependency diagnostics

The shared Debian-family dependency step emits `CI-DEPS` start/end markers for each existing APT
operation and package-availability probe. End markers include elapsed seconds and the actual exit
status; normal `apt-get` progress and errors stay visible. Probe metadata stays suppressed, while
its phase and result remain visible. Required failures stop the step; the existing optional OpenGL
install and unavailable-package paths retain their nonfatal behavior.

The 20-minute Linux job budget, package requests, sources, CA/TLS/signature checks and other
platform commands remain unchanged. An external timeout can leave a start marker without an end;
this identifies the active phase without inventing an exit status. These diagnostics establish
operation visibility, not a remedy or proof of a network/latency cause. Local manifest build policy
is unchanged.

### Ubuntu 24.04 CI Qt5 dependency selection

The selection below is the sole package-request exception to the diagnostic baseline above.
Ubuntu 24.04 CI already selects Qt5 5.15.13 because its Qt6 6.4.2 is below the project's
6.7.0 floor. Only literal `ID=ubuntu` and `VERSION_ID=24.04` in `/etc/os-release` select
the smaller dependency set; standard unquoted, single-quoted and double-quoted values are
accepted. Missing, unreadable, malformed or duplicate relevant metadata retains the full set,
as do other systems and versions. Metadata is read as data and never executed.

Before APT, one `CI-DEPS selection=ubuntu24-qt5` or `CI-DEPS selection=full` marker records
the choice. The exact Ubuntu 24.04 branch omits only the four direct Qt6 development/tool
requests and both Qt6 translation-package probe/install pairs. Qt5's default Recommends and
plugins, common dependencies, doxygen/help2man, Mesa/OpenGL/GTK/X11, libei/libportal probes
and optional OpenGL behavior remain intact. No new translation, test or man-page skip is added;
their existing Qt5 policy remains visible as Not Run where applicable.

The 20-minute budget, APT diagnostics, sources and trust checks remain intact. The observed
71.2 MB Qt6/LLVM15 download pool is not a net download-size or elapsed-time saving guarantee.
DEB package testing may pull Qt6 back through `qt6-svg-plugins` Recommends; packaging policy is
unchanged. Actual Qt selection, download volume, timing, build and package acceptance still
require native CI evidence; this dependency reduction alone establishes none of them.

### Prerequisites

| Component | Minimum Version | Notes |
|-----------|----------------|-------|
| **CMake** | 3.25+ | Presets use schema version 6, which needs 3.25 |
| **Qt** | 6.7.0+ | Core, Widgets, Network, DBus (Linux) |
| **OpenSSL** | 3.0+ | TLS/crypto support |
| **libportal** | 0.8.0+ | Linux/BSD only (Wayland portal) |
| **libei** | 1.0+ | Linux/BSD only (input emulation) |

libportal older than 0.8.0 does not ship `libportal/inputcapture.h`. When libei is present and libportal is missing or too old, the build still compiles: Wayland portal capture is left out and libei uses its socket backend.

### Architectures

Build artifacts are **64-bit only**. `vcpkg.json` sets `"supports": "x64 | arm64"`. There is no 32-bit (x86) target.

| Platform | Architectures |
|----------|----------------|
| Windows | x64 (`vcvarsall.bat x64`, triplet `x64-windows-static`) and arm64 in CI |
| Linux | x86_64 and aarch64 |
| macOS | arm64 locally (`arm64-osx`); Intel x86_64 in CI |

x86_64 and x64 name the same 64-bit Intel/AMD architecture. They are not 32-bit.

A push to `main` and a pull request compile and test Windows, macOS, and the free Linux subset in the same run; automatic validation skips Flatpak. Push builds use a snapshot version and pull requests use a development version. The Windows GUI filename remains `synergy_<X.Y.Z>.exe`, using the base version from `Version.cmake`. These runs upload unsigned Actions artifacts without signing, notarization, release publication, or external notifications. Manual snapshots support every platform, including Windows, and do not sign or publish. A `package-type=release` run (and a published Release) builds and publishes **Windows and macOS only**; Linux is validated by the free push/PR subset but is not part of the release packages. The GitHub Release is published by the daily 08:23 schedule, a manual `package-type=release` run, or a published Release.

The macOS and Linux vcpkg overlays use their platform system runtime (`VCPKG_CRT_LINKAGE=dynamic`) while retaining static Qt/OpenSSL libraries (`VCPKG_LIBRARY_LINKAGE=static`). This differs from the Windows release triplet, which uses `/MT`, and from CI's shared Qt distribution.

Those builds run on GitHub-hosted runners. Qt and OpenSSL are downloaded there, not onto the machine that cloned the repository. A clone does not fetch them. `vendor/` stays untracked.

### Source responsibilities

The entry points remain `setup.bat` and `scripts/build.{bat,sh}`. Runtime code stays under
`src/lib`, applications and packaging assets under `src/apps` and `deploy`, and tests mirror
the owning library under `src/unittests`.

- `server/config/` owns configuration parsing, action syntax, link cells, serialization and read context;
  `server/Config.h` remains the public model interface.
- `ServerInput.cpp`, `ServerSwitching.cpp` and `ServerClipboard.cpp` separate server input dispatch,
  screen transitions and clipboard/file transfer from client lifecycle management.
- `gui/mainwindow/` owns connection and configuration actions; `gui/config/` types use `synergy::gui`.
- Platform screen implementations are split into event, input and display/session responsibilities
  within their existing `platform/<os>/` directory. Key lookup tables remain separate from dispatch code.
- `synergy/protocol/ProtocolTypes.h` remains the include entry point; control, input and data message
  declarations live in domain headers. Their wire names, versions and serialized field order are unchanged.

CMake lists every translation unit explicitly. A source split must preserve platform guards and compile
on Windows, macOS and Linux; a local build alone does not establish cross-platform acceptance.

### Default Build Options

The following components are enabled by default:

- ✅ TuPig Synergy GUI (`synergy_<X.Y.Z>.exe` on Windows, `synergy` on Linux, `TuPig Synergy.app` on macOS)
- ✅ TuPig Synergy Core (`synergy-core`, unversioned)
- ✅ Daemon for Windows UAC handling (`synergy-daemon`, Windows only, unversioned)
- ✅ Build-time Unit Tests (Qt Test + CTest)

### CMake Configuration Options

| Option | Description | Default |
|--------|-------------|---------|
| `BUILD_GUI` | Build the Qt GUI | `ON` |
| `BUILD_TESTS` | Build unit tests | `ON` |
| `BUILD_INSTALLER` | Build platform installers | `ON` |
| `BUILD_X11_SUPPORT` | Build X11 backend (Linux/BSD) | `ON` |
| `BUILD_OSX_BUNDLE` | Build macOS `.app` bundle | `ON` |
| `SKIP_BUILD_TESTS` | Skip tests during build | `ON` |
| `SYNERGY_ENABLE_INTERACTIVE_TESTS` | Enable tests that inject input or change the system clipboard | `OFF` |
| `ENABLE_COVERAGE` | Enable code coverage reports | `OFF` |
| `CLEAN_TRS` | Remove obsolete translation strings | `OFF` |
| `SYNERGY_CORE_FLAVOR` | Build as "TuPig Synergy Core"; seeds headless defaults (GUI/tests/installer off) | `OFF` |
| `APPLE_CODESIGN_DEV` | Apple Developer code-sign identity (cache variable, not an `option()`) | unset |
| `CMAKE_COMPILE_WARNING_AS_ERROR` | Fail the build on a compiler warning | `ON` |

**Build:**

```bash
# Windows: scripts\build.bat release
# Linux or macOS (Apple Silicon):
./scripts/build.sh release
```

A bare `cmake -S` without the vcpkg toolchain from `CMakePresets.json` will not find Qt.

---

### 🪟 Windows (MSVC + vcpkg)

Dependencies are resolved by vcpkg in manifest mode. The repository-local vcpkg is bootstrapped for
you, so there is no `VCPKG_ROOT` to configure.

```bat
REM One-time: install the host toolchain (run as Administrator)
setup.bat

REM Build (bootstraps vcpkg, configures, compiles)
scripts\build.bat release
```

`scripts\build.bat` locates Visual Studio through `vswhere`, activates the MSVC x64 environment and
then configures with the preset for that Visual Studio. Visual Studio 2022 uses
`windows-msvc-release`. Visual Studio 2026 uses `windows-msvc-2026-release`, because vcpkg builds
the dependencies with the same compiler and a VS 2022 link cannot resolve the newer static STL.
That preset sets `SYNERGY_VERSION_RELEASE=ON`, so the version string is `X.Y.Z` from
`cmake/Version.cmake` (currently 1.21.2), not `X.Y.Z-dev+<sha>`.

Output:

| File | Role |
|------|------|
| `build\bin\Release\synergy_<X.Y.Z>.exe` | GUI. `X.Y.Z` is `SYNERGY_VERSION_MAJOR.MINOR.PATCH` plus an optional stage. |
| `build\bin\Release\synergy-core.exe` | Core. The GUI looks this name up; do not version it. |
| `build\bin\Release\synergy-daemon.exe` | Windows service helper. Also unversioned. The portable 7Z omits it. |

Check the binaries. `X.Y.Z` below is the current `Version.cmake` value, 1.21.2:

```bat
build\bin\Release\synergy_1.21.2.exe --version
build\bin\Release\synergy-core.exe --version
build\bin\Release\synergy-daemon.exe --version
build\bin\Release\synergy-core.exe server
```

`synergy-core --version` prints the product version and `protocol v1.8`. The GUI prints `TuPig Synergy: 1.21.2`. The daemon prints `1.21.2` only. Flags are listed in [configuration.md](configuration.md#command-line).

#### Windows code signing

Release CI signs `synergy_<X.Y.Z>.exe`, `synergy-core.exe`, `synergy-daemon.exe`, and the
MSI through [SignPath Foundation](https://signpath.org/). The private key stays on
SignPath's HSM. This repository never stores a certificate, a password, or a token.
The portable 7Z is built from those signed executables; the archive itself is not an
Authenticode file. A local Release build stays unsigned, because the Foundation key
cannot be used on a developer machine.

Signing runs only when all three of these GitHub Actions values are set (Settings,
Secrets and variables, Actions):

| Name | Kind | Meaning |
|---|---|---|
| `SIGNPATH_API_TOKEN` | secret | API token for a user who may submit the signing policy. The only secret. |
| `SIGNPATH_ORGANIZATION_ID` | variable | Organization id assigned after the Foundation approves the project. |
| `SIGNPATH_PROJECT_SLUG` | variable | Project slug for this repository. |

`SIGNPATH_POLICY_SLUG` is an optional variable. When it is empty, the workflow uses
`release-signing`.

Apply at <https://signpath.org/> for the public repository
`https://github.com/Tupig/Tupig_synergy`. After approval:

1. Install the SignPath GitHub App on this repository and link the predefined
   Trusted Build System GitHub.com to the project.
2. Create signing policy `release-signing` for the `main` branch and release tags.
3. Add two artifact configurations, using these slugs and the XML in this repository:
   `windows-executables` from `.signpath/windows-executables.xml`, and `windows-msi`
   from `.signpath/windows-msi.xml`.
4. Store the API token as the secret above, and store the organization id and project
   slug as the two variables. Do not commit any of those values.

If any required value is missing, the job logs that signing was skipped and uploads unsigned packages. If they
are set and `signtool verify` fails after SignPath returns the files, the job fails.

---

### 🍎 macOS (Apple Silicon locally, Intel in CI)

Host tools are a full Xcode installation selected as the active developer directory,
CMake 3.25+, Ninja, pkgconf, Autoconf with autoconf-archive, Automake, and GNU Libtool.
Qt submodule builds require Xcode's version tools, which Command Line Tools alone
do not provide. Select the installed Xcode in its Locations settings and confirm
`xcodebuild -version` succeeds before building. Qt, OpenSSL, and their library
dependencies come from the repository vcpkg. Homebrew supplies these host build
tools; do not point CMake at a Homebrew Qt.

CI uses the maintained Homebrew `openssl@3` line and passes its self-discovered prefix as `OPENSSL_ROOT_DIR`. This is the same major version as the release manifest, but the minor version and link model can differ; local release dependencies remain governed by `vcpkg.json`. OpenSSL 4 source compatibility remains tracked in [#307](https://github.com/Tupig/TupigSynergy/issues/307).

```bash
xcodebuild -version
brew install cmake ninja pkgconf autoconf autoconf-archive automake libtool
./scripts/build.sh release
```

`./scripts/build.sh` selects preset `macos-release` (Ninja, triplet `arm64-osx`). That
preset is Apple Silicon. Intel x86_64 is the Actions job `macos-x64`
(`CMAKE_OSX_ARCHITECTURES=x86_64` on `macos-15-intel`), which produces a separate DMG
(`mac_x64`), not a universal binary. Local builds default to a requested deployment
target of macOS 14, matching the macOS overlay triplets' dependency build target;
this is not a compatibility acceptance claim. Override the application target with
`-DCMAKE_OSX_DEPLOYMENT_TARGET=<version>` when the selected dependencies need a newer
OS. Both CI artifacts explicitly target macOS 15 because their provider archives
require it. Configure checks selected Qt/OpenSSL slices and static archive members;
packaging checks every bundled Mach-O slice and the app's `LSMinimumSystemVersion`.
Providers newer than the requested floor are rejected, never relabeled as compatible.
An existing build cache retains an explicitly selected older target. To move that
cache to the current default, run `cmake --preset macos-release -DCMAKE_OSX_DEPLOYMENT_TARGET=14`
before using the build entry point again. Older dependency caches are rebuilt when
the triplet deployment target changes; keep the download and binary caches.

Output: `build/bin/TuPig Synergy.app` and `build/bin/synergy-core`.

#### macOS code signing (development)

```bash
security find-identity -v -p codesigning login.keychain-db
cmake --preset macos-release -DAPPLE_CODESIGN_DEV="Apple Development: Name (TEAMID)"
codesign -d -r- "build/bin/TuPig Synergy.app"
```

Local development uses an Apple Development certificate. Distribution signing uses the
CI secret `APPLE_CODESIGN_ID` when it is set.

---

### 🐧 Linux (Ubuntu / Debian / Fedora / Arch)

Install a C++ compiler, CMake 3.25+, Ninja, and the system libraries vcpkg does not
build (X11 and the Wayland portal stack). Do not install a distro Qt or OpenSSL for
this build, and do not set `CMAKE_PREFIX_PATH` to one: `./scripts/build.sh` configures
with the vcpkg toolchain, which builds Qt and OpenSSL from `vcpkg.json`.

#### Ubuntu / Debian

```bash
sudo apt update && sudo apt install -y \
  cmake ninja-build g++ pkg-config \
  libx11-dev libxi-dev libxtst-dev \
  libxinerama-dev libxrandr-dev \
  libxkbcommon-dev libglib2.0-dev \
  libportal-dev libei-dev
```

#### Fedora / RHEL

```bash
sudo dnf install -y \
  cmake ninja-build gcc-c++ pkgconf-pkg-config \
  libX11-devel libXi-devel libXtst-devel \
  libXinerama-devel libXrandr-devel \
  libxkbcommon-devel glib2-devel \
  libportal-devel libei-devel
```

#### Arch Linux

```bash
sudo pacman -S \
  cmake ninja gcc pkgconf \
  libx11 libxi libxtst libxinerama \
  libxrandr libxkbcommon glib2 \
  libportal libei
```

#### Build

```bash
./scripts/build.sh release
```

Output: `build/bin/synergy` (GUI, unversioned, because the `.desktop` file launches that
name) and `build/bin/synergy-core`. If vcpkg stops on a missing system library, install
the package it names and run the script again. Downloads already in
`vendor/vcpkg/downloads/` are reused.

---

### 📦 Packaging & Installation

| Target | Command | Output |
|--------|---------|--------|
| **Install locally** | `cmake --install build --prefix /usr/local` | System-wide |
| **Staged install** | `DESTDIR=/tmp/pkg cmake --install build` | Package staging |
| **Binary package** | `cmake --build build --target package` | Platform native |
| **Source package** | `cmake --build build --target package_source` | Source tarball |

**Supported Package Formats:**

| Platform | Formats |
|----------|---------|
| **All** | TGZ, TBZ2, TXZ, TZST |
| **Linux** | DEB or RPM (auto-selected from `/etc/os-release`); Flatpak via CI `build-flatpak`; Arch via `deploy/linux/arch/PKGBUILD.in` |
| **macOS** | DMG (DragNDrop) |
| **Windows** | 7Z (portable), MSI (WiX, see note) |

> **Note**: AppImage is not implemented — no `appimage` reference exists in
> `deploy/` or `.github/`. A Flatpak manifest exists under
> `deploy/linux/flatpak/` and is consumed by the CI `build-flatpak`
> job; the local `package` target does not produce a Flatpak bundle.
> MSI generation uses the WiX toolset installed on the machine (CI pins
> 5.0.2; CPack requests the WiX v4 schema).

---

### 🔧 Advanced Configuration

#### Sanitizer builds

```bash
cmake --preset linux-asan-build && cmake --build --preset linux-asan-build
cmake --preset linux-tsan-build && cmake --build --preset linux-tsan-build
cmake --preset linux-coverage-build && cmake --build --preset linux-coverage-build
```

Windows ASan is preset `windows-msvc-asan`. It uses the dynamic-CRT triplet `x64-windows` because MSVC ASan requires `/MD`. There is no MemorySanitizer preset.

#### ccache Acceleration

```bash
cmake -S . -B build \
  -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
```

---

### 🧪 Testing

Tests are registered under `src/unittests`, so `ctest --test-dir build` reports that no tests were found. `BUILD_TESTS` defaults to `ON` (the test binaries are compiled). `SKIP_BUILD_TESTS` defaults to `ON`, which only skips running them at the end of the build.

`SYNERGY_ENABLE_INTERACTIVE_TESTS` defaults to `OFF`. CTest keeps the available interactive targets registered and built, but reports them as `Disabled` / `Not Run`; count them separately from passing tests. The exact targets are `OSXKeyStateTests` (real key injection into the focused application), `OSXClipboardTests` and `MSWindowsClipboardTests` (replace or clear the system clipboard), and `XWindowsClipboardTests` when X11 is enabled (changes X11 clipboard selection ownership). Pure Windows conversion, DropData, key-state and policy tests remain enabled. `SKIP_BUILD_TESTS` does not opt into interactive targets.

Only a disposable test environment should explicitly configure `-DSYNERGY_ENABLE_INTERACTIVE_TESTS=ON`; the shared native CI Configure command does this. The option does not derive its value from `CI` or `GITHUB_ACTIONS`, and an explicit `OFF` remains off. Opting in permits these side effects; it does not grant macOS permissions or establish secure-desktop, hardware or multi-machine acceptance. Directly launching a test executable bypasses CTest's disabled property. Review new tests for host side effects before registering them.

```bash
# Windows (Visual Studio preset)
ctest --test-dir build/src/unittests -C Release --output-on-failure

# Linux / macOS (Ninja)
ctest --test-dir build/src/unittests --output-on-failure

ctest --test-dir build/src/unittests -C Release -R "ClipboardTests" --output-on-failure
```

Pass `-DSKIP_BUILD_TESTS=OFF` to run that ctest invocation automatically after the build. Coverage uses preset `linux-coverage-build`.

---

### Source encoding

CI job `lint-encoding` runs `python3 scripts/ci/check-encoding.py` after the version plan and before compile or packaging. The same command works from a checkout. It walks the configured roots, skips excluded directory names, and checks files in parallel. Each file is read in chunks. A NUL byte in the first 4 KiB, with no UTF-8/UTF-16/UTF-32 BOM, is reported as `binary` and the rest of that file is not read.

The default whitelist is `utf-8`, which means UTF-8 without a BOM. An ASCII file is reported as `ascii` and still passes, because ASCII is UTF-8. To allow a BOM, add `utf-8-bom` (aliases `utf-8-sig` and `utf-8-with-bom`). To allow only ASCII, set the whitelist to `ascii` alone. `binary` and `unreadable` are never accepted.

`on_violation` is `fail` by default: each bad file is printed and the process exits 1, so the build jobs do not start. Set it to `warn`, or pass `--warn`, to print the same lines and exit 0. `--fail` forces the non-zero exit even when the file says `warn`.

| Key | Default | Meaning |
|-----|---------|---------|
| `roots` | `src`, `cmake`, `scripts`, `deploy`, `.github`, `translations` | Directories scanned from the repository root |
| `extensions` | C/C++, Qt, CMake, scripts, YAML, JSON, XML | Suffixes that count as source |
| `names` | `CMakeLists.txt` | Extra file names, regardless of suffix |
| `allowed` | `utf-8` | Whitelist. Also accepts `ascii` and `utf-8-bom` |
| `exclude_dirs` | `vendor`, `build`, `.git`, `dist`, `node_modules`, `downloads`, `vcpkg_installed`, `generated`, `autogen`, `CMakeFiles` | Directory names skipped at any depth |
| `on_violation` | `fail` | `fail` exits 1. `warn` prints and exits 0 |

A failing line looks like this. The path and the detected encoding are on the same line:

```text
encoding FAIL src/foo.cpp detected=utf-8-bom allowed=utf-8
encoding FAIL count=1 checked=669 allowed=utf-8 mode=fail
```

A clean run is one line: `encoding OK checked=669 violations=0 allowed=utf-8 mode=fail`. The detail lines list every bad file. The last line is the total.

---

### ❓ Troubleshooting Build Issues

| Issue | Solution |
|-------|----------|
| `Qt6 not found` | Re-run `scripts/build.bat` or `scripts/build.sh`. Do not set `Qt6_DIR` or `CMAKE_PREFIX_PATH` to a distro Qt. |
| `OpenSSL not found` | Same. OpenSSL is a vcpkg manifest dependency, not a system package you point CMake at. |
| `X11 libs missing` | Install `libx11-dev`, `libxi-dev`, `libxtst-dev`, and the rest of the Linux package list above. |
| `Wayland protocols` | Install `libwayland-dev` and `wayland-protocols` when the log asks for them. |
| vcpkg download stalls | Run the build script again. Keep `vendor/vcpkg/downloads/`; partial files are rejected by their SHA-512. |
| `encoding FAIL ... detected=` | The file is not in the whitelist. Save it as UTF-8 without a BOM, or add that encoding to `allowed`. `ascii` already passes under `utf-8`. `detected=binary` means a NUL byte; resave the file as text. |
| `_FORTIFY_SOURCE` redefined | The toolchain already defined it (Flatpak uses level 3). The build keeps that definition and does not add a second one. |
| MSVC CRT merge module not found | A dynamic-CRT release installs `vcruntime140.dll` and `msvcp140.dll` beside the binaries when the merge module is absent. |

---

### Complete package dependencies

All shipped MSI/portable, app/DMG and native Linux packages include application runtime dependencies and their license notices. Deployment follows the actual linked dependency graph, including GUI/core helpers and dynamically loaded Qt plugins. Missing libraries, unresolved loader paths, wrong architectures or missing license sources stop packaging or verification. Static Qt builds retain only their actual dynamic dependencies and include licenses for statically linked libraries as well.

The prebuilt Windows/macOS CI Qt provider carries an exact-version source-notice supplement for `qtbase` and `qtsvg`. The collector checks official Qt source archive SHA256 values and records selected SDK provider hashes, complete license texts, attribution files and provenance. CI records the original manifest digest before configuration; configuration and packaging reject later metadata or notice changes. Source archives stay in the CI cache. Local manifest builds continue using vcpkg notice material.

Windows packages keep application DLLs and required VC redistributables beside the executables, with Qt plugins selected through package-local `qt.conf`. The portable archive omits the service daemon; the MSI keeps it. Distribute complete packages rather than a GUI executable detached from its runtime files. The current Windows CI Qt deployment selects its Schannel TLS plugin, whose TLS provider is the Windows OS; the application's directly linked OpenSSL runtime is still included. The optional Qt OpenSSL plugin is not forced into the package. PE dependency checks and startup on current CI runners do not establish minimum-Windows-version compatibility.

macOS deploys the GUI and `synergy-core`, fixes third-party load commands inside the app, removes build-machine RPATHs, and signs after dependency fixups and resource installation. The verifier copies the DMG's app into a temporary directory, audits all Mach-O files and plugins, starts GUI `QApplication`/Cocoa through `--help`, and connects a real core client to a private TLS loopback server with a disposable identity and trust store. It requires a TLS handshake, client certificate and the core's peer-fingerprint acceptance before stopping without a Synergy greeting. Actual loaded images must remain inside the package or system directories. This is package TLS/startup evidence, not main-window, TCC permission or input-sharing acceptance. Translation catalogues live only in `Contents/Resources/translations`.

The prebuilt Qt 6 OpenSSL plugin resolves OpenSSL ABI 3 dynamically, so import-table scans alone cannot close its dependencies. macOS packaging explicitly copies that runtime and its provider notices into the app when needed; CI installs `openssl@3` for this purpose as part of its prebuilt-Qt dependency exception. Homebrew discovery is restricted to GitHub Actions; local manifest builds use their selected SDK and fail if it lacks a required runtime. The installed app does not depend on Homebrew. A separately statically linked core OpenSSL does not satisfy this plugin runtime. CI runner success also does not establish compatibility with the minimum macOS deployment target; current provider objects may require a newer OS.

Linux packages keep application executables, libraries and plugins under an app-private `libexec/synergy` tree and provide launch wrappers. The verifier extracts native packages without installing them or fetching dependencies, checks ELF closure, and exercises the bundled Qt platform plugin with build search paths removed.

Linux CI retains packaged license documentation and restores missing notices from the exact installed RPM when needed. Before configure, `scripts/ci/prepare-runtime-notices.py` verifies installed package owners, source versions, ELF64 architecture and actual SONAMEs against finite source profiles. Pinned SHA256 values use published upstream/distro evidence where available; libproxy and the exact official GCC revision are locally computed original-byte pins with recorded source identities, not signature verification. The exact keyutils pin downloads from its reviewed fixed Buildroot endpoint after the official kernel snapshot returned 403; HTTP errors do not trigger a generic mirror fallback. Complete authored notices and delegated license texts are retained, including libportal's incorporated GPLv3 and libxcb's exact protocol-input notices. GCC uses a bounded streaming profile (128 MiB download, 180,000 members, 1 GiB expanded, 2 MiB per notice, 32 MiB output and 180 seconds); other sources retain their smaller bounds. Unknown identities/layouts/hashes stop with bounded public package diagnostics. Maintainers review new finite pins and full regression checks before native revalidation. The unchanged final collector copies only notices used by the actual staged dependency closure; unused prepared maps grant no exemption. Ubuntu 24.04's static libportal build first verifies its fixed Git commit and reuses exact 0.9.1 source notices plus full GPLv3 in the existing extra-notice directory, without claiming an installed dynamic provider. This CI supplement does not change local vcpkg manifest dependency or notice sources.

The host boundary is the supported OS kernel/ABI and system APIs; on macOS this includes `/usr/lib` and `/System/Library`, and on Linux glibc/loader plus POSIX shell/coreutils. Display services, fonts and GPU drivers remain on the host. Qt, OpenSSL and application plugins are included rather than delegated to those host boundaries. Native DEB/RPM/Arch metadata may name the minimal OS runtime prerequisites.

Run package checks with `scripts\ci\verify-package-windows.bat build <x64-or-arm64>`, `bash scripts/ci/verify-package-macos.sh --expected-arch <x64-or-arm64> build`, or `bash scripts/ci/verify-package-linux.sh --expected-arch <x64-or-arm64> build <distribution-family>`. Declare the approved build target explicitly; aliases x86_64/amd64 and aarch64 are accepted. macOS universal apps declare `x64,arm64`; GUI/core must match, while additional supported dependency slices retain per-slice closure checks. Whole extracted payload bytes, archive members and native slices are audited before launch; `V64_NATIVE_*` records retain actual traversal hashes and counts. Report each result against its exact build commit. A green CI run with prebuilt/shared dependencies does not prove the local vcpkg static release path. The Flatpak delivery target is an offline OSTree repository `.tar.zst` containing the app, runtime and extensions for each supported architecture, with hashes and bilingual offline instructions. This requires separate exact-commit verification; a standalone app file or an unfinished offline pipeline must not be reported as a complete delivery.

---

## 中文

### Ubuntu CI 的 APT 引导

Linux CI 保留现有容器镜像。仅 Ubuntu 的 checkout 前 Git 引导将活跃的官方 archive/security
`/ubuntu` 与 ports `/ubuntu-ports` 源 URI 从 HTTP 改为 HTTPS，保持主机、包选择、签名密钥、
禁用段落及其他源文本。Ubuntu runner 的单个公共 CA 文件只读挂载到
`/run/synergy-ci/host-ca-certificates.crt`，不包含私钥，也不新增自定义信任根。引导在改源之前
要求 CA 为可读、非空的普通文件，每次 APT 调用临时传入 `Acquire::https::CAInfo`，保持服务器、
主机名和仓库签名校验。安装 Git 工具时明确安装 `ca-certificates`；后续依赖安装使用容器正常
系统 CA 和已保留的 HTTPS 源。Debian 及其他发行版的命令保持不变。

APT 引导阶段与错误保持可见，失败即停止 checkout。任务仍为 20 分钟，不新增重试循环或 HTTP
回退。离线夹具只证明改写和命令行为；runner CA 可用性、TLS 下载、实际包安装及 ARM 引导超时
仍需独立原生 CI 证据。经审核回滚本次 CI/文档改动会恢复此前传输及其潜在 HTTP 故障。本地
manifest 发布构建和已记录的 CI 依赖例外仍遵循原有依赖政策。


### Debian 系 CI 依赖诊断

公共 Debian 系依赖步骤为现有每个 APT 操作和包可用性探测输出 `CI-DEPS` 开始/结束标记。
结束标记包含耗时秒数和真实退出状态，保留 `apt-get` 的正常进度与错误；探测元数据仍被抑制，
但阶段及结果可见。必需操作失败即终止步骤；原可选 OpenGL 安装失败及包不可用路径继续保持
非致命语义。

Linux 作业的 20 分钟预算、包请求、源、CA/TLS/签名检查及其他平台命令保持不变。外部超时可能
只留下开始而没有结束标记，这能定位活跃阶段，不伪造退出状态。诊断仅建立操作可见性，
不代表已修复或证实网络/耗时根因；本地 manifest 构建政策不变。

### Ubuntu 24.04 CI 的 Qt5 依赖选择

以下选择是上文诊断基线中“包请求不变”的唯一例外。
Ubuntu 24.04 CI 原本就选择 Qt5 5.15.13，因为系统 Qt6 6.4.2 低于项目的 6.7.0 下限。
仅当 `/etc/os-release` 中字面值精确为 `ID=ubuntu`、`VERSION_ID=24.04` 时选择较小依赖集合，
支持标准无引号、单引号和双引号值。相关元数据缺失、不可读、格式错误或重复，以及其他系统
和版本，均保留完整集合。元数据只作为数据读取，绝不执行。

APT 前仅输出一次 `CI-DEPS selection=ubuntu24-qt5` 或 `CI-DEPS selection=full` 标记。
精确 Ubuntu 24.04 分支只省去四个直接 Qt6 开发/工具包请求及两个 Qt6 翻译包的完整探测/安装对；
保留 Qt5 默认 Recommends 和插件、公共依赖、doxygen/help2man、Mesa/OpenGL/GTK/X11、
libei/libportal 探测及可选 OpenGL 行为。不新增翻译、测试或 man 页面跳过；原 Qt5 策略中
适用的项目仍须如实报告为 Not Run。

20 分钟预算、APT 诊断、软件源和信任检查保持不变。已观察到的 71.2 MB Qt6/LLVM15 下载池
不代表净下载量或耗时节省保证。DEB 包测试可能经 `qt6-svg-plugins` 的 Recommends 再拉入
Qt6，打包策略不变。真实 Qt 选择、下载量、耗时、构建及包验收仍需原生 CI 证据，不能仅凭
这次依赖缩减宣称通过。

### 编译环境要求

| 组件 | 最低版本 | 说明 |
|------|----------|------|
| **CMake** | 3.25+ | 预设使用 schema 第 6 版，因此需要 3.25 |
| **Qt** | 6.7.0+ | Core, Widgets, Network, DBus (Linux) |
| **OpenSSL** | 3.0+ | TLS/加密支持 |
| **libportal** | 0.8.0+ | 仅 Linux/BSD (Wayland Portal) |
| **libei** | 1.0+ | 仅 Linux/BSD (输入仿真) |

低于 0.8.0 的 libportal 没有 `libportal/inputcapture.h`。装了 libei、但没有 libportal 或版本过旧时，仍然可以编译：Wayland Portal 捕获不参与构建，libei 走 socket 后端。

### 架构

构建产物**只有 64 位**。`vcpkg.json` 的 `"supports"` 是 `x64 | arm64`。没有 32 位（x86）目标。

| 平台 | 架构 |
|------|------|
| Windows | x64（`vcvarsall.bat x64`，triplet `x64-windows-static`）；CI 另有 arm64 |
| Linux | x86_64 与 aarch64 |
| macOS | 本地 arm64（`arm64-osx`）；CI 另有 Intel x86_64 |

x86_64 与 x64 都是 64 位 Intel/AMD，不是 32 位。

推送到 `main` 和 Pull Request 会在同一次运行里编译并测试 Windows、macOS 和精简 Linux 矩阵；自动验证跳过 Flatpak。推送使用 snapshot 版本，PR 使用开发版本；Windows GUI 文件名仍取 `Version.cmake` 的基础版本，保持 `synergy_<X.Y.Z>.exe`。这些运行上传未签名 Actions 产物，不执行签名、公证、正式发布或外部通知。手动 snapshot 支持包括 Windows 在内的所有平台，同样不签名、不发布。`package-type=release`（以及已发布的 Release）只构建并发布 **Windows 与 macOS**；Linux 仅由推送/PR 的免费子集验证，不进入发布包。GitHub Release 由每天 08:23 的定时任务、手动的 `package-type=release`，或一次已发布的 Release 来发布。

macOS 与 Linux 的 vcpkg overlay 使用平台系统运行库（`VCPKG_CRT_LINKAGE=dynamic`），Qt/OpenSSL 仍保持静态库（`VCPKG_LIBRARY_LINKAGE=static`）。这与 Windows 发布 triplet 的 `/MT`、以及 CI 的共享 Qt 分发分别不同。

这些构建跑在 GitHub 托管的运行器上。Qt 和 OpenSSL 下载到运行器，不会下载到克隆仓库的那台机器。只克隆仓库不会去取它们。`vendor/` 不入库。

### 源码职责划分

入口保持为 `setup.bat` 和 `scripts/build.{bat,sh}`。运行时代码放在 `src/lib`，应用与打包资源分别放在
`src/apps` 和 `deploy`，测试在 `src/unittests` 按所属库分组。

- `server/config/` 分别负责配置解析、动作语法、连接单元、序列化与读取上下文；`server/Config.h` 仍是公开模型接口。
- `ServerInput.cpp`、`ServerSwitching.cpp` 和 `ServerClipboard.cpp` 将输入派发、屏幕切换、剪贴板与文件传输
  从客户端生命周期管理中分离。
- `gui/mainwindow/` 负责连接与配置操作；`gui/config/` 的类型统一放在 `synergy::gui` 命名空间。
- 平台屏幕实现仍位于 `platform/<os>/`，按事件、输入、显示或会话职责拆分；按键查找表与事件派发代码分离。
- `synergy/protocol/ProtocolTypes.h` 保持为包含入口，控制、输入与数据消息的声明按领域拆分。
  协议名称、版本和序列化字段顺序保持不变。

CMake 显式列出每个编译单元。拆分源码时必须保留平台条件并验证 Windows、macOS 与 Linux 构建；
本机编译通过不能代替跨平台验收。

### 默认启用组件

- ✅ TuPig Synergy GUI（Windows 为 `synergy_<X.Y.Z>.exe`，Linux 为 `synergy`，macOS 为 `TuPig Synergy.app`）
- ✅ TuPig Synergy 核心（`synergy-core`，不带版本号）
- ✅ Windows UAC 守护进程（`synergy-daemon`，仅 Windows，不带版本号）
- ✅ 编译时单元测试 (Qt Test + CTest)

### CMake 配置选项

| 选项 | 说明 | 默认值 |
|------|------|--------|
| `BUILD_GUI` | 编译 Qt GUI | `ON` |
| `BUILD_TESTS` | 编译单元测试 | `ON` |
| `BUILD_INSTALLER` | 编译平台安装包 | `ON` |
| `BUILD_X11_SUPPORT` | 编译 X11 后端 (Linux/BSD) | `ON` |
| `BUILD_OSX_BUNDLE` | 编译 macOS `.app` 包 | `ON` |
| `SKIP_BUILD_TESTS` | 跳过编译时测试 | `ON` |
| `SYNERGY_ENABLE_INTERACTIVE_TESTS` | 启用注入真实输入或改变系统剪贴板的测试 | `OFF` |
| `ENABLE_COVERAGE` | 启用代码覆盖率报告 | `OFF` |
| `CLEAN_TRS` | 清理翻译文件中过时字符串 | `OFF` |
| `SYNERGY_CORE_FLAVOR` | 以 “TuPig Synergy Core” 构建；同时将 GUI/测试/安装包默认置为关闭（无界面构建） | `OFF` |
| `APPLE_CODESIGN_DEV` | Apple 开发者代码签名身份（缓存变量，非 `option()`） | 未设置 |
| `CMAKE_COMPILE_WARNING_AS_ERROR` | 编译器警告使构建失败 | `ON` |

**构建：**

```bash
# Windows：scripts\build.bat release
# Linux 或 macOS（Apple Silicon）：
./scripts/build.sh release
```

不带 `CMakePresets.json` 里 vcpkg 工具链的 `cmake -S` 找不到 Qt。

---

### 🪟 Windows (MSVC + vcpkg)

依赖由 vcpkg 清单模式解析，仓库内的 vcpkg 会自动引导，因此无需配置 `VCPKG_ROOT`。

```bat
REM 一次性：安装宿主工具链（以管理员身份运行）
setup.bat

REM 构建（自动引导 vcpkg、配置、编译）
scripts\build.bat release
```

`scripts\build.bat` 会通过 `vswhere` 定位 Visual Studio，激活 MSVC x64 环境，然后选用与该
Visual Studio 对应的预设。Visual Studio 2022 使用 `windows-msvc-release`。Visual Studio 2026
使用 `windows-msvc-2026-release`，因为 vcpkg 用同一套编译器构建依赖，而 VS 2022 的链接器
解不开较新的静态 STL 符号。
该预设打开 `SYNERGY_VERSION_RELEASE`，版本字符串是 `cmake/Version.cmake` 里的 `X.Y.Z`
（当前 1.21.2），不是 `X.Y.Z-dev+<sha>`。

产物：

| 文件 | 作用 |
|------|------|
| `build\bin\Release\synergy_<X.Y.Z>.exe` | GUI。`X.Y.Z` 来自 `SYNERGY_VERSION_MAJOR.MINOR.PATCH`，若有 stage 再追加。 |
| `build\bin\Release\synergy-core.exe` | 核心。GUI 按这个固定名字查找，不要加版本号。 |
| `build\bin\Release\synergy-daemon.exe` | Windows 服务辅助进程，同样不带版本号。便携 7Z 故意不含它。 |

检查产物。下面的 `1.21.2` 是当前 `Version.cmake` 的值：

```bat
build\bin\Release\synergy_1.21.2.exe --version
build\bin\Release\synergy-core.exe --version
build\bin\Release\synergy-daemon.exe --version
build\bin\Release\synergy-core.exe server
```

`synergy-core --version` 同时打印产品版本和 `protocol v1.8`。GUI 打印 `TuPig Synergy: 1.21.2`。daemon 只打印 `1.21.2`。参数说明见 [configuration.md](configuration.md#命令行)。

#### Windows 代码签名

Release CI 通过 [SignPath Foundation](https://signpath.org/) 签名 `synergy_<X.Y.Z>.exe`、`synergy-core.exe`、`synergy-daemon.exe` 和 MSI。私钥留在 SignPath 的 HSM 上。本仓库不存放证书、密码或令牌。便携 7Z 由这些已签名的可执行文件打成，压缩包本身不是 Authenticode 文件。本地 Release 构建不签名，因为 Foundation 的私钥不能拿到开发机上用。

只有下面三项 GitHub Actions 值都设置了才签名（Settings、Secrets and variables、Actions）：

| 名称 | 种类 | 含义 |
|---|---|---|
| `SIGNPATH_API_TOKEN` | secret | 可提交该签名策略的用户 API 令牌。唯一的 secret。 |
| `SIGNPATH_ORGANIZATION_ID` | variable | Foundation 批准项目后给出的组织 id。 |
| `SIGNPATH_PROJECT_SLUG` | variable | 本仓库对应的项目 slug。 |

`SIGNPATH_POLICY_SLUG` 是可选变量。为空时工作流使用 `release-signing`。

到 <https://signpath.org/> 为公开仓库 `https://github.com/Tupig/Tupig_synergy` 申请。批准之后：

1. 在本仓库安装 SignPath GitHub App，并把预置的 Trusted Build System GitHub.com 关联到该项目。
2. 创建签名策略 `release-signing`，范围是 `main` 分支和 Release 标签。
3. 按仓库里的 XML 添加两份构件配置，slug 必须一致：`windows-executables` 对应 `.signpath/windows-executables.xml`，`windows-msi` 对应 `.signpath/windows-msi.xml`。
4. 把 API 令牌存成上面的 secret，把组织 id 和项目 slug 存成那两个变量。这三项都不要提交进仓库。

缺任何一项必填值时，作业记下签名已跳过，并上传未签名包。三项都在、SignPath 返回文件之后 `signtool verify` 失败，则作业失败。

---

### 🍎 macOS（本地 Apple Silicon，Intel 由 CI 构建）

宿主工具是已选为活动开发者目录的完整 Xcode、CMake 3.25+、Ninja、pkgconf、Autoconf 及 autoconf-archive、Automake 和 GNU Libtool。Qt 子模块构建需要 Xcode 版本工具，只有 Command Line Tools 并不足够。先在 Xcode 的 Locations 设置中选择已安装的 Xcode，并确认 `xcodebuild -version` 成功。Qt、OpenSSL 及其库依赖来自仓库内 vcpkg；Homebrew 只提供这些宿主构建工具，不要把 CMake 指到 Homebrew 的 Qt。

CI 使用仍受维护的 Homebrew `openssl@3` 主线，并将自动发现的前缀作为 `OPENSSL_ROOT_DIR`。它与发布清单同为主版本 3，但小版本和链接模型可能不同；本地发布依赖仍以 `vcpkg.json` 为准。OpenSSL 4 源码兼容性继续由 [#307](https://github.com/Tupig/TupigSynergy/issues/307) 跟踪。

```bash
xcodebuild -version
brew install cmake ninja pkgconf autoconf autoconf-archive automake libtool
./scripts/build.sh release
```

`./scripts/build.sh` 选择预设 `macos-release`（Ninja，triplet `arm64-osx`），这是 Apple Silicon。Intel x86_64 是 Actions 作业 `macos-x64`（`macos-15-intel` 上 `CMAKE_OSX_ARCHITECTURES=x86_64`），单独产出 `mac_x64` 的 DMG，不是一个通用二进制。本地构建默认请求 macOS 14，与 macOS overlay triplet 的依赖构建目标一致；这不是兼容性验收承诺。若选定依赖需要较新系统，可用 `-DCMAKE_OSX_DEPLOYMENT_TARGET=<版本>` 显式调整应用目标。两种 CI 产物因提供方归档的要求而明确以 macOS 15 为目标。配置阶段检查选定 Qt/OpenSSL 架构及静态归档成员；打包验证检查每个 Mach-O 架构和 app 的 `LSMinimumSystemVersion`。依赖要求高于请求目标时会拒绝构建，不能将其重新标注为兼容。已有构建缓存会保留显式选定的旧目标；若要转为当前默认，先运行 `cmake --preset macos-release -DCMAKE_OSX_DEPLOYMENT_TARGET=14`，再使用构建入口。triplet 部署目标变化后会重建旧依赖缓存；请保留下载及二进制缓存。

产物：`build/bin/TuPig Synergy.app` 与 `build/bin/synergy-core`。

#### macOS 代码签名（开发用）

```bash
security find-identity -v -p codesigning login.keychain-db
cmake --preset macos-release -DAPPLE_CODESIGN_DEV="Apple Development: Name (TEAMID)"
codesign -d -r- "build/bin/TuPig Synergy.app"
```

本地开发用 Apple Development 证书。分发签名在设置了 CI secret `APPLE_CODESIGN_ID` 时由 CI 完成。

---

### 🐧 Linux (Ubuntu / Debian / Fedora / Arch)

安装 C++ 编译器、CMake 3.25+、Ninja，以及 vcpkg 不负责构建的系统库（X11 与 Wayland portal）。不要为这次构建安装发行版 Qt 或 OpenSSL，也不要把 `CMAKE_PREFIX_PATH` 指过去：`./scripts/build.sh` 使用 vcpkg 工具链，按 `vcpkg.json` 从源码构建 Qt 与 OpenSSL。

#### Ubuntu / Debian

```bash
sudo apt update && sudo apt install -y \
  cmake ninja-build g++ pkg-config \
  libx11-dev libxi-dev libxtst-dev \
  libxinerama-dev libxrandr-dev \
  libxkbcommon-dev libglib2.0-dev \
  libportal-dev libei-dev
```

#### Fedora / RHEL

```bash
sudo dnf install -y \
  cmake ninja-build gcc-c++ pkgconf-pkg-config \
  libX11-devel libXi-devel libXtst-devel \
  libXinerama-devel libXrandr-devel \
  libxkbcommon-devel glib2-devel \
  libportal-devel libei-devel
```

#### Arch Linux

```bash
sudo pacman -S \
  cmake ninja gcc pkgconf \
  libx11 libxi libxtst libxinerama \
  libxrandr libxkbcommon glib2 \
  libportal libei
```

#### 编译

```bash
./scripts/build.sh release
```

产物：`build/bin/synergy`（GUI，不带版本号，因为 `.desktop` 的 `Exec=` 按这个名字启动）和 `build/bin/synergy-core`。vcpkg 若因缺少系统库停下，安装它点名的包后再运行脚本。`vendor/vcpkg/downloads/` 里已有的下载会被复用。

---

### 📦 打包与安装

| 目标 | 命令 | 输出 |
|------|------|------|
| **本地安装** | `cmake --install build --prefix /usr/local` | 系统级 |
| **暂存安装** | `DESTDIR=/tmp/pkg cmake --install build` | 打包暂存 |
| **二进制包** | `cmake --build build --target package` | 平台原生包 |
| **源码包** | `cmake --build build --target package_source` | 源码压缩包 |

**支持的包格式：**

| 平台 | 格式 |
|------|------|
| **所有平台** | TGZ, TBZ2, TXZ, TZST |
| **Linux** | DEB 或 RPM（依 `/etc/os-release` 自动二选一）；Flatpak 经 CI `build-flatpak`；Arch 经 `deploy/linux/arch/PKGBUILD.in` |
| **macOS** | DMG (DragNDrop) |
| **Windows** | 7Z（便携）、MSI (WiX，见说明) |

> **说明**：AppImage **未实现** —— `deploy/`、`.github/` 中不存在任何 `appimage` 引用。Flatpak 清单位于 `deploy/linux/flatpak/`，由 CI 的 `build-flatpak` job 消费；本地 `package` 目标不产出 Flatpak 包。MSI 使用机器上已安装的 WiX（CI 固定 5.0.2；CPack 请求 WiX v4 schema）。

---

### 🔧 进阶配置

#### Sanitizer 构建

```bash
cmake --preset linux-asan-build && cmake --build --preset linux-asan-build
cmake --preset linux-tsan-build && cmake --build --preset linux-tsan-build
cmake --preset linux-coverage-build && cmake --build --preset linux-coverage-build
```

Windows ASan 使用预设 `windows-msvc-asan`。它改用动态 CRT triplet `x64-windows`，因为 MSVC ASan 需要 `/MD`。没有 MemorySanitizer 预设。

#### ccache 加速编译

```bash
cmake -S . -B build \
  -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
```

---

### 🧪 测试

测试注册在 `src/unittests`，所以 `ctest --test-dir build` 会报找不到测试。`BUILD_TESTS` 默认 `ON`（会编译测试程序）。`SKIP_BUILD_TESTS` 默认 `ON`，只是不在构建结束时自动跑它们。

`SYNERGY_ENABLE_INTERACTIVE_TESTS` 默认 `OFF`。CTest 仍注册并编译当前平台存在的交互目标，但将其报告为 `Disabled` / `Not Run`，须与通过的测试分开计数。精确范围为 `OSXKeyStateTests`（向当前焦点应用注入真实按键）、`OSXClipboardTests` 与 `MSWindowsClipboardTests`（替换或清空系统剪贴板），以及启用 X11 时的 `XWindowsClipboardTests`（改变 X11 剪贴板 selection 所有权）。纯 Windows 转换、DropData、键状态和策略测试保持启用。`SKIP_BUILD_TESTS` 不会启用交互目标。

仅在一次性测试环境中显式配置 `-DSYNERGY_ENABLE_INTERACTIVE_TESTS=ON`；原生 CI 的公共 Configure 命令已这样处理。选项不会根据 `CI` 或 `GITHUB_ACTIONS` 自动开启，显式 `OFF` 保持关闭。开启即允许上述副作用，不会授予 macOS 权限，也不代表安全桌面、硬件或多机验收通过。直接启动测试程序会绕过 CTest 的禁用属性。注册新测试前须检查宿主副作用。

```bash
# Windows（Visual Studio 预设）
ctest --test-dir build/src/unittests -C Release --output-on-failure

# Linux / macOS（Ninja）
ctest --test-dir build/src/unittests --output-on-failure

ctest --test-dir build/src/unittests -C Release -R "ClipboardTests" --output-on-failure
```

加上 `-DSKIP_BUILD_TESTS=OFF` 会在构建结束后自动跑上述 ctest。覆盖率使用预设 `linux-coverage-build`。

---

### 源码编码

CI 作业 `lint-encoding` 在版本计划之后、编译和打包之前运行 `python3 scripts/ci/check-encoding.py`。在检出的仓库里执行同一条命令即可。它按配置的根目录往下走，跳过指定的目录名，并并行检查文件。每个文件分块读取。前 4 KiB 里出现 NUL、且没有 UTF-8/UTF-16/UTF-32 BOM 时，记为 `binary`，不再读这个文件的剩余部分。

默认白名单是 `utf-8`，即不带 BOM 的 UTF-8。ASCII 文件会被标成 `ascii`，仍然通过，因为 ASCII 属于 UTF-8。要允许 BOM，把 `utf-8-bom` 加进白名单（别名是 `utf-8-sig` 和 `utf-8-with-bom`）。若只允许 ASCII，白名单里就只留 `ascii`。`binary` 和 `unreadable` 不会被接受。

`on_violation` 默认是 `fail`：每个不合规文件打一行，进程以 1 退出，编译作业不会开始。设成 `warn`，或加上 `--warn`，会打出同样的行并以 0 退出。配置写成 `warn` 时，`--fail` 仍强制以非零退出。

| 键 | 默认 | 含义 |
|----|------|------|
| `roots` | `src`、`cmake`、`scripts`、`deploy`、`.github`、`translations` | 从仓库根开始扫描的目录 |
| `extensions` | C/C++、Qt、CMake、脚本、YAML、JSON、XML | 算作源码的后缀 |
| `names` | `CMakeLists.txt` | 不论后缀都纳入的文件名 |
| `allowed` | `utf-8` | 白名单。也可写 `ascii` 和 `utf-8-bom` |
| `exclude_dirs` | `vendor`、`build`、`.git`、`dist`、`node_modules`、`downloads`、`vcpkg_installed`、`generated`、`autogen`、`CMakeFiles` | 任意层级都跳过的目录名 |
| `on_violation` | `fail` | `fail` 以 1 退出。`warn` 只打印并以 0 退出 |

失败时路径和检测到的编码在同一行：

```text
encoding FAIL src/foo.cpp detected=utf-8-bom allowed=utf-8
encoding FAIL count=1 checked=669 allowed=utf-8 mode=fail
```

全部通过时只有一行：`encoding OK checked=669 violations=0 allowed=utf-8 mode=fail`。明细行列出每个不合规文件，最后一行是汇总。

---

### ❓ 常见编译问题排查

| 问题 | 解决方案 |
|------|----------|
| `Qt6 not found` | 重新运行 `scripts\build.bat` 或 `scripts/build.sh`。不要把 `Qt6_DIR` 或 `CMAKE_PREFIX_PATH` 指到发行版 Qt。 |
| `OpenSSL not found` | 同上。OpenSSL 是 vcpkg 清单依赖，不是要另外指给 CMake 的系统包。 |
| `X11 libs missing`（X11 库缺失） | 安装 `libx11-dev`、`libxi-dev`、`libxtst-dev` 以及上面 Linux 软件包列表里的其余项。 |
| `Wayland protocols`（Wayland 协议） | 日志点名时再安装 `libwayland-dev` 与 `wayland-protocols`。 |
| vcpkg 下载中断 | 重新运行构建脚本。保留 `vendor/vcpkg/downloads/`；不完整文件会因 SHA-512 校验失败而被拒绝。 |
| `encoding FAIL ... detected=` | 该文件不在白名单里。另存为不带 BOM 的 UTF-8，或把检测到的编码加进 `allowed`。`utf-8` 已经接受 `ascii`。`detected=binary` 表示文件里有 NUL，需要另存为文本。 |
| `_FORTIFY_SOURCE` 被重复定义 | 工具链已经定义过（Flatpak 用的是 3）。构建保留那一个定义，不再加第二个。 |
| 找不到 MSVC CRT 合并模块 | 动态 CRT 的发布构建在没有合并模块时，把 `vcruntime140.dll` 和 `msvcp140.dll` 装到可执行文件旁边。 |

---

### 完整打包运行依赖

所有交付的 MSI/便携包、app/DMG 和 Linux 原生包均须携带应用运行依赖及其许可声明。部署依据实际链接的依赖关系，包括 GUI/core 辅助程序和动态加载的 Qt 插件。缺库、加载路径无法解析、架构错误或许可来源缺失都应阻止打包或验证通过。静态 Qt 构建只保留实际需要的动态依赖，同时携带静态链接库的许可。

Windows/macOS CI 的预编译 Qt 提供方另附 `qtbase`、`qtsvg` 对应精确版本的源码许可材料。收集器核对 Qt 官方源码归档 SHA256，并记录选定 SDK 提供方哈希、完整许可文本、归属声明及来源记录。CI 在配置前记录原始清单摘要；配置和打包阶段都拒绝之后的清单或许可内容改动。源码归档留在 CI 缓存，本地 manifest 构建继续使用 vcpkg 的许可材料。

Windows 包把应用 DLL 和必需的 VC 可再分发运行库放在可执行文件旁，通过包内 `qt.conf` 定位 Qt 插件。便携包省略服务 daemon，MSI 保留。交付完整软件包，不把 GUI 可执行文件与运行依赖拆开分发。 当前 Windows CI 的 Qt 部署选择 Schannel TLS 插件，其 TLS 提供方属于 Windows 系统；应用直接链接的 OpenSSL 运行库仍随包携带。不强行加入可选的 Qt OpenSSL 插件。PE 依赖检查及当前 CI 系统启动通过，不能证明最低 Windows 版本兼容性。

macOS 同时部署 GUI 与 `synergy-core`，将第三方加载路径改为 app 内部位置，去除构建机 RPATH，并在依赖修复及资源安装后签名。验证器把 DMG 中的 app 复制到临时目录，检查所有 Mach-O 文件和插件，通过 `--help` 启动 GUI 的 `QApplication`/Cocoa，并让真实 core 客户端使用一次性身份和信任库连接私有 TLS 回环服务。必须完成 TLS 握手、客户端证书检查以及 core 对服务端指纹的接受，随后停止且不发送 Synergy 问候消息。实际加载的库必须位于包内或系统目录。这是包 TLS/启动证据，不是主窗口、TCC 权限或输入共享功能验收。翻译文件只放在 `Contents/Resources/translations`。

预编译 Qt 6 的 OpenSSL 插件动态解析 OpenSSL ABI 3，因此仅扫描导入表无法覆盖全部依赖。macOS 打包会在需要时显式把该运行库及提供方许可文本复制进 app；CI 为此安装 `openssl@3`，这是预编译 Qt 依赖例外的一部分。Homebrew 查找仅限 GitHub Actions；本地 manifest 构建使用选定 SDK，缺少必需运行库时失败。安装后的 app 不依赖 Homebrew。core 另行静态链接的 OpenSSL 无法代替此插件运行库。CI runner 通过也不能证明最低 macOS 部署版本兼容性；当前提供方对象可能要求较新的系统。

Linux 包将应用可执行文件、库和插件放在应用私有的 `libexec/synergy` 目录，提供启动包装脚本。验证器只解包原生软件包，不安装它们或联网下载依赖，检查 ELF 依赖闭包，并在移除构建搜索路径后加载包内 Qt 平台插件。

Linux CI 保留发行包的许可文档，必要时仅从已安装 RPM 的精确版本恢复缺失声明。配置前，`scripts/ci/prepare-runtime-notices.py` 根据有限来源档案核对已安装包所有者、源版本、ELF64 架构与实际 SONAME。有发布证据时使用上游/发行版 SHA256；libproxy 和固定官方 GCC 修订使用精确原始字节的本地计算摘要并保留来源身份，不冒称验签。keyutils因官方kernel快照返回403，固定从已审查的Buildroot端点下载同一SHA256字节；不引入HTTP错误通用镜像回退。完整保留作者声明和被委托许可，包括 libportal 并入的 GPLv3 以及 libxcb 精确协议输入声明。GCC 使用有界流式档案（下载128 MiB、180000成员、展开1 GiB、单声明2 MiB、输出32 MiB、180秒）；其他来源保留较小上限。未知身份、布局或哈希失败时提供有界公开包元数据诊断，维护者须审查新固定值、完整回归后再做原生验证。最终收集器逻辑不变，只复制实际暂存依赖闭包使用的声明，未用准备映射不提供豁免。Ubuntu24.04 静态 libportal 先核对固定 Git commit，再把精确0.9.1源码声明及完整GPLv3放入原有额外声明目录，不虚构已安装动态提供方身份。本项CI补充不改变本地vcpkg manifest依赖或许可来源。

宿主边界是受支持的操作系统内核/ABI 和系统 API；macOS 包括 `/usr/lib` 与 `/System/Library`，Linux 包括 glibc/加载器及 POSIX shell/coreutils。显示服务、字体及 GPU 驱动由宿主提供。Qt、OpenSSL 和应用插件应随包携带，不能算作这些宿主边界。DEB/RPM/Arch 元数据可以声明最小操作系统运行前提。

检查入口为 `scripts\ci\verify-package-windows.bat build <x64-or-arm64>`、`bash scripts/ci/verify-package-macos.sh --expected-arch <x64-or-arm64> build` 或 `bash scripts/ci/verify-package-linux.sh --expected-arch <x64-or-arm64> build <发行版类型>`。须显式声明已批准的构建目标，支持x86_64/amd64及aarch64别名。macOS通用app声明`x64,arm64`，GUI/core须精确匹配，依赖额外的受支持64位切片仍逐片检查闭包。启动前检查完整解包载荷字节、归档成员和原生切片，`V64_NATIVE_*`记录实际遍历摘要及计数。每项结果都应关联准确的构建提交。使用预编译/共享依赖的 CI 绿灯不证明本机 vcpkg 静态发布路径已通过。Flatpak 的交付目标是为每种受支持架构提供包含应用、runtime 和 extensions 的离线 OSTree 仓库 `.tar.zst`，附带哈希及双语离线说明。它须按准确提交独立验证，单个应用文件或尚未验收的离线流水线不能宣称交付完整。

---

## Build Presets & Scripts / 构建预设与脚本

### One-command build / 一键构建

The repository is self-contained. Third-party dependencies (Qt, OpenSSL and their transitive
dependencies) are managed by vcpkg in manifest mode, and the repository-local vcpkg is bootstrapped
automatically — no `VCPKG_ROOT`, no global install, no manual environment variables.

本仓库自包含。第三方依赖（Qt、OpenSSL 及其传递依赖）由 vcpkg 清单模式管理，仓库内的 vcpkg 自动引导
—— 无需 `VCPKG_ROOT`、无需全局安装、无需手工设置环境变量。

| Platform / 平台 | Command / 命令 |
|---|---|
| Windows | `scripts\build.bat release` |
| Linux / macOS | `./scripts/build.sh release` |

The `*-release` presets set `SYNERGY_VERSION_RELEASE=ON`, so the version string is `X.Y.Z` from `cmake/Version.cmake`. Configuring without that preset (or without `-DSYNERGY_VERSION_RELEASE=ON`) produces `X.Y.Z-dev+<sha>`.

这些 `*-release` 预设都会设置 `SYNERGY_VERSION_RELEASE=ON`，版本字符串是 `cmake/Version.cmake` 里的 `X.Y.Z`。不用该预设、也不传 `-DSYNERGY_VERSION_RELEASE=ON` 时，版本是 `X.Y.Z-dev+<sha>`。

Both scripts accept only `release`: every vcpkg overlay triplet in this repository sets
`VCPKG_BUILD_TYPE release`, so a Debug configuration cannot link the dependencies. For
instrumented builds use the diagnostics presets (`linux-asan-build` / `linux-tsan-build` /
`linux-coverage-build`).

Both scripts call `scripts/bootstrap-vcpkg.{bat,sh}`, which clones the repository-local vcpkg into
`vendor/vcpkg`, checks out the baseline pinned by `builtin-baseline` in `vcpkg.json`, and bootstraps
the vcpkg tool. The baseline is the single source of truth and is read from `vcpkg.json`, not
duplicated in the scripts.

两个脚本都会调用 `scripts/bootstrap-vcpkg.{bat,sh}`：克隆仓库内 vcpkg 到 `vendor/vcpkg`，检出
`vcpkg.json` 中 `builtin-baseline` 固定的版本，并引导 vcpkg 工具。baseline 是唯一真源，由脚本从
`vcpkg.json` 读取，不在脚本里复制一份。

两个脚本都只接受 `release`：本仓库所有 vcpkg overlay triplet 均设置
`VCPKG_BUILD_TYPE release`，Debug 配置无法链接依赖。需要插桩构建请使用诊断预设
（`linux-asan-build` / `linux-tsan-build` / `linux-coverage-build`）。

> **Windows prerequisite / Windows 前置条件**: a C++ toolchain (Visual Studio 2022 or 2026 Build Tools with
> the "Desktop development with C++" workload), CMake 3.25+ and Git. Run `setup.bat` once to install
> them. `scripts\build.bat` locates Visual Studio through `vswhere` and activates the MSVC environment
> itself — do not hardcode install paths or pre-set `VCPKG_ROOT`.
>
> **Windows 前置条件**：C++ 工具链（含 "Desktop development with C++" 工作负载的 Visual Studio 2022
> 或 2026 Build Tools）、CMake 3.25+ 与 Git。首次运行 `setup.bat` 安装。`scripts\build.bat` 会通过 `vswhere`
> 自行定位 Visual Studio 并激活 MSVC 环境 —— 不要硬编码安装路径，也不要预设 `VCPKG_ROOT`。

### Presets in `CMakePresets.json` / 预设清单

| Preset | Purpose / 用途 |
|---|---|
| `windows-msvc`, `windows-msvc-release`, `windows-msvc-debug` | Windows x64, Visual Studio 2022, static triplet |
| `windows-msvc-2026`, `windows-msvc-2026-release` | Windows x64, Visual Studio 2026, static triplet. `scripts\build.bat` selects this when VS 2026 is the newest install. |
| `linux`, `linux-release` | Linux x64, Ninja, static triplet |
| `macos`, `macos-release` | macOS arm64, Ninja, static triplet |
| `linux-asan-build`, `linux-tsan-build`, `linux-coverage-build` | Linux diagnostics (`RelWithDebInfo` + `-O1`) / Linux 诊断构建 |

```bash
# List presets / 列出预设
cmake --list-presets

# Configure and build directly (the scripts do both for you)
# 直接配置与构建（脚本已代为完成这两步）
cmake --preset windows-msvc-release
cmake --build --preset windows-msvc-release
```

> There is no `CMakeUserPresets.json` in this repository — it is git-ignored and was removed.
>
> 本仓库不包含 `CMakeUserPresets.json` —— 该文件已被忽略并移除。
