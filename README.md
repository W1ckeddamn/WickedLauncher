<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="program_info/io.github.w1ckeddamn.WickedLauncher.logo-darkmode.svg" type="image/svg+xml">
    <img src="program_info/io.github.w1ckeddamn.WickedLauncher.logo.svg" alt="Wicked Launcher" width="460">
  </picture>
</p>

<p align="center">
  A customized, high-performance fork of Prism Launcher with enhanced offline play, resource management, and diagnostics.
</p>

<p align="center">
  <a href="https://github.com/W1ckeddamn/WickedLauncher/actions/workflows/build.yml">
    <img src="https://github.com/W1ckeddamn/WickedLauncher/actions/workflows/build.yml/badge.svg?branch=develop" alt="Build Status">
  </a>
  <a href="https://github.com/W1ckeddamn/WickedLauncher/releases">
    <img src="https://img.shields.io/github/v/release/W1ckeddamn/WickedLauncher" alt="Latest Release">
  </a>
  <a href="LICENSE">
    <img src="https://img.shields.io/badge/License-GPLv3-blue.svg" alt="License: GPL v3">
  </a>
  <img src="https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20macOS-4c4c4c" alt="Platforms">
</p>

---

## Table of Contents

- [Features](#features)
- [Downloads](#downloads)
- [Building from Source](#building-from-source)
- [Project Structure](#project-structure)
- [Branding](#branding)
- [Contributing](#contributing)
- [License & Attribution](#license--attribution)

---

## Features

### Enhanced Offline Authentication
- **Custom Player Identity:** Configure custom player names, arbitrary UUIDs (or generate RFC 4122 v4 UUIDs), player model variants (`classic` / `slim`), and local texture files (skin and cape).
- **Embedded Local Skin Server:** An integrated in-process HTTP server listening on `127.0.0.1` implements the `authlib-injector` specification. Skins and capes are served to the game client locally without third-party authentication services.
- **Direct Launch:** Launch instances offline without Microsoft account enforcement.

### Schematics Management
- Dedicated **Schematics** management tab per instance, bound to the `schematics/` directory.
- Native support for:
  - `.schematic` — MCEdit / WorldEdit legacy format
  - `.schem` — Sponge / WorldEdit modern format
  - `.litematic` — Litematica format
- Automatic NBT metadata parsing: dimensions (width, height, length), author, format version, and block palette statistics.

### Custom Player Models (CPM & YSM)
- **Custom Player Models:** Dedicated management interface for `.cpmmodel` files with metadata inspection.
- **Yes Steve Model:** Management interface for `.ysm`, `.zip`, and `.7z` model packages.

### Robust Crash Handling & Diagnostics
- **Windows:** Custom structured exception filter using `StackWalk64` and `dbghelp` to produce human-readable call stacks and automatic minidumps (`.dmp`) on crash.
- **Linux & macOS:** POSIX signal handlers (`SIGSEGV`, `SIGABRT`, `SIGBUS`, `SIGFPE`) capturing backtrace frames directly to `crash.log`.
- Thread-safe, non-reentrant diagnostics logging with high-resolution timestamps.

### Download Mirrors & Mod Platform Enhancements
- Built-in support for download mirrors (BMCLAPI and MCIM) with automatic fallback.
- CDN fallback for restricted CurseForge mods.
- Automated OptiFine resolution and download integration.

### Optimized Package Footprint
- Production packages use full compiler optimizations and Link-Time Optimization (LTO).
- Binaries and shared libraries are stripped (`strip --strip-unneeded`) on Linux and Windows MinGW; debugging symbols (`.pdb`, `.ilk`, `.lib`) are pruned from MSVC redistributables, reducing artifact sizes by 60–75%.

---

## Downloads

Precompiled binaries for Linux, Windows, and macOS are available on the [Releases](https://github.com/W1ckeddamn/WickedLauncher/releases) page and via [GitHub Actions](https://github.com/W1ckeddamn/WickedLauncher/actions).

| Operating System | Package Type |
| :--- | :--- |
| **Windows (x86_64, ARM64)** | Setup Installer (`.exe`), Portable (`.zip`) |
| **Linux (x86_64, AArch64)** | AppImage (`.AppImage`), Portable Tarball (`.tar.gz`) |
| **macOS (Universal)** | Apple Disk Image (`.dmg`), Archive (`.zip`) |

---

## Building from Source

### Prerequisites

- C++23 compliant compiler:
  - GCC 13+
  - Clang 16+
  - MSVC 2022 (v143, 17.8+)
- CMake 3.28 or later
- Ninja build system
- Qt 6.8 or later (Core, Gui, Widgets, Network, Concurrent, Svg, Xml, and DBus on Linux)
- Java Development Kit (JDK 8 or later, required for packaging launcher wrappers)

### Linux

```bash
git clone --recursive https://github.com/W1ckeddamn/WickedLauncher.git
cd WickedLauncher

cmake --preset linux -DLauncher_BUILD_PLATFORM="official"
cmake --build --preset linux
```

### macOS

```bash
git clone --recursive https://github.com/W1ckeddamn/WickedLauncher.git
cd WickedLauncher

cmake --preset macos -DLauncher_BUILD_PLATFORM="official"
cmake --build --preset macos
```

For a universal macOS build, use the `macos_universal` preset instead.

### Windows (MSVC)

```cmd
git clone --recursive https://github.com/W1ckeddamn/WickedLauncher.git
cd WickedLauncher

cmake --preset windows_msvc -DLauncher_BUILD_PLATFORM="official"
cmake --build --preset windows_msvc
```

### Windows (MinGW-w64)

```cmd
cmake --preset windows_mingw -DLauncher_BUILD_PLATFORM="official"
cmake --build --preset windows_mingw
```

---

## Project Structure

| Path | Purpose |
| :--- | :--- |
| `launcher/` | Launcher application source code |
| `libraries/` | Bundled third-party libraries and submodules |
| `program_info/` | Branding, icons, desktop entry, AppStream metadata, and packaging metadata |
| `cmake/` | CMake modules, presets, and build helpers |
| `tests/` | Automated tests |
| `tools/` | Development and packaging scripts |
| `nix/` | Nix packaging and development environment |

---

## Branding

Wicked Launcher uses a faceted **W crystal** mark with dedicated light and dark variants. Branding assets live in [`program_info/`](program_info/README.md):

- Square application icon: [`io.github.w1ckeddamn.WickedLauncher.svg`](program_info/io.github.w1ckeddamn.WickedLauncher.svg)
- macOS application icon: [`io.github.w1ckeddamn.WickedLauncher.bigsur.svg`](program_info/io.github.w1ckeddamn.WickedLauncher.bigsur.svg)
- Horizontal wordmark: [`logo.svg`](program_info/io.github.w1ckeddamn.WickedLauncher.logo.svg) / [`logo-darkmode.svg`](program_info/io.github.w1ckeddamn.WickedLauncher.logo-darkmode.svg)
- Social banner: [`Social.svg`](program_info/io.github.w1ckeddamn.WickedLauncher.Social.svg)
- Generated binaries: `wickedlauncher.ico`, `wickedlauncher.icns`, and `io.github.w1ckeddamn.WickedLauncher_256.png`

The application ID is `io.github.w1ckeddamn.WickedLauncher`. The legacy `prismlauncher://` URL scheme stays registered because it is the Microsoft login redirect URI and is used by older import links.

---

## Contributing

Contributions are welcome. Please read [CONTRIBUTING.md](CONTRIBUTING.md) and [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) before opening a pull request.

When changing branding or icons, update the SVG sources in `program_info/` and regenerate the binary icons as described in [`program_info/README.md`](program_info/README.md).

---

## License & Attribution

- Wicked Launcher is licensed under the [GNU General Public License v3.0](LICENSE).
- Artwork, logos, and branding assets are licensed under [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/).
- This project is an independent fork of [Prism Launcher](https://github.com/PrismLauncher/PrismLauncher) and is not affiliated with or endorsed by Prism Launcher, MultiMC, or Mojang Studios.
