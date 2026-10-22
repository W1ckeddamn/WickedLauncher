# Wicked Launcher

Wicked Launcher is a customized, high-performance fork of Prism Launcher designed for enhanced flexibility, offline functionality, and advanced resource management.

[![Build Status](https://github.com/W1ckeddamn/WickedLauncher/actions/workflows/build.yml/badge.svg?branch=develop)](https://github.com/W1ckeddamn/WickedLauncher/actions)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
[![Latest Release](https://img.shields.io/github/v/release/W1ckeddamn/WickedLauncher)](https://github.com/W1ckeddamn/WickedLauncher/releases)

---

## Features

### Enhanced Offline Authentication
- **Custom Player Identity:** Configure custom player names, arbitrary UUIDs (or generate RFC 4122 v4 UUIDs), player model variants (`classic` / `slim`), and local texture files (skin and cape).
- **Embedded Local Skin Server:** Features an integrated in-process HTTP server listening on `127.0.0.1` implementing the `authlib-injector` specification. Skins and capes are served to the game client locally without requiring third-party authentication services.
- **Direct Launch:** Launch instances offline without Microsoft account enforcement.

### Schematics Management
- Dedicated **Schematics** management tab per instance (bound to the `schematics/` directory).
- Native support for:
  - `.schematic` (MCEdit / WorldEdit legacy format)
  - `.schem` (Sponge / WorldEdit modern format)
  - `.litematic` (Litematica format)
- Automatic NBT metadata parsing: dimensions (width, height, length), author, format version, and block palette statistics.

### Custom Player Models (CPM & YSM)
- **Custom Player Models:** Dedicated management interface for `.cpmmodel` files with metadata inspection.
- **Yes Steve Model:** Management interface for `.ysm`, `.zip`, and `.7z` model packages.

### Robust Crash Handling & Diagnostics
- **Windows:** Custom structured exception filter using `StackWalk64` and `dbghelp` to produce human-readable call stacks and automatic minidumps (`.dmp`) upon crash.
- **Linux & macOS:** POSIX signal handlers (`SIGSEGV`, `SIGABRT`, `SIGBUS`, `SIGFPE`) capturing backtrace frames directly to `crash.log`.
- Thread-safe, non-reentrant diagnostics logging with high-resolution timestamps.

### Download Mirrors & Mod Platform Enhancements
- Built-in support for download mirrors (BMCLAPI and MCIM) with automatic fallback mechanisms.
- CDN fallback for restricted CurseForge mods.
- Automated OptiFine resolution and download integration.

### Optimized Package Footprint
- Production packages configured with full compiler optimizations and Link-Time Optimization (LTO).
- Binaries and shared libraries are stripped (`strip --strip-unneeded`) on Linux and Windows MinGW, and debugging symbols (`.pdb`, `.ilk`, `.lib`) are pruned from MSVC redistributables, reducing artifact sizes by 60–75%.

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

- C++20 / C++23 compliant compiler:
  - GCC 13+
  - Clang 16+
  - MSVC 2022 (v143, 17.8+)
- CMake 3.22 or later
- Ninja build system
- Qt 6.6 or later (Core, Gui, Widgets, Network, Concurrent, Svg, Xml, and DBus on Linux)
- Java Development Kit (JDK 8 or later, required for packaging launcher wrappers)

### Linux & macOS

```bash
git clone --recursive https://github.com/W1ckeddamn/WickedLauncher.git
cd WickedLauncher

cmake --preset gcc-release -DLauncher_BUILD_PLATFORM="official"
cmake --build --preset gcc-release
```

### Windows (MSVC)

```cmd
git clone --recursive https://github.com/W1ckeddamn/WickedLauncher.git
cd WickedLauncher

cmake --preset msvc-release -DLauncher_BUILD_PLATFORM="official"
cmake --build --preset msvc-release
```

---

## License & Attribution

- Wicked Launcher is licensed under the [GNU General Public License v3.0](LICENSE).
- Artwork, logos, and branding assets are licensed under [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/).
- This project is an independent fork and is not affiliated with or endorsed by Prism Launcher, MultiMC, or Mojang Studios.
