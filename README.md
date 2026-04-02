![Build Status](https://github.com/westlicht/performer/actions/workflows/ci.yml/badge.svg?branch=master)

# PERFORMER NX

<a href="doc/sequencer.jpg"><img src="doc/sequencer.jpg"/></a>

Firmware for the **PERFORMER NX** Eurorack sequencer.

- Fork software repository: [napolitano/performer-nx](https://github.com/napolitano/performer-nx/)
- Fork hardware repository: [napolitano/performer-nx-hardware](https://github.com/napolitano/performer-nx-hardware/)
- Original project page: [westlicht.github.io/performer](https://westlicht.github.io/performer)
- Original hardware repository: [westlicht/performer-hardware](https://github.com/westlicht/performer-hardware)

## Table of contents

- [About this fork](#about-this-fork)
  - [Scope and philosophy](#scope-and-philosophy)
  - [What this fork is not](#what-this-fork-is-not)
- [At a glance](#at-a-glance)
- [How to use this README](#how-to-use-this-readme)
- [Quick start](#quick-start)
- [Platform support](#platform-support)
- [Development setup](#development-setup)
  - [Linux / macOS](#linux--macos)
  - [Windows (WSL)](#windows-wsl)
- [Build directories](#build-directories)
- [Hardware workflow](#hardware-workflow)
  - [Bootloader target (read first)](#bootloader-target-read-first)
  - [HWCONFIG target](#hwconfig-target)
  - [Flashing with ST-Link V3 and JTAG](#flashing-with-st-link-v3-and-jtag)
    - [Command-line flashing](#command-line-flashing)
    - [STM32CubeProgrammer (GUI)](#stm32cubeprogrammer-gui)
- [On-Device Debugging](#on-device-debugging)
  - [Why ST-Link V3](#why-st-link-v3)
  - [Critical safety notes (read first)](#critical-safety-notes-read-first)
  - [Recommended debugging procedure](#recommended-debugging-procedure)
  - [STM toolset and OpenOCD options](#stm-toolset-and-openocd-options)
  - [Build strategy for reliable on-device debugging](#build-strategy-for-reliable-on-device-debugging)
  - [When to debug on hardware vs simulator](#when-to-debug-on-hardware-vs-simulator)
- [Simulator workflow](#simulator-workflow)
- [Troubleshooting](#troubleshooting)
- [Source tree overview](#source-tree-overview)
- [Third-party libraries](#third-party-libraries)
- [License](#license)

## About this fork

This repository is a continuation fork of the original project.

The project name was changed from **PER|FORMER** to **PERFORMER NX** for better readability.
The `NX` suffix indicates continued development and a "New Experience" direction, while still aiming to avoid breaking existing workflows where possible.

The original firmware has seen limited development activity for several years. This fork exists to keep the project alive and improve it in a practical, user-focused way:

- fix long-standing issues
- improve UX and day-to-day reliability
- add useful features
- preserve existing user workflows and compatibility where possible
- improve maintainability of the code base
- modernize parts of the toolchain where it is necessary and where it makes sense

In short: make the instrument better and smoother without unnecessary disruption.

This distinguishes the fork from more radical directions (for example **PEW|FORMER**), which focus on larger architectural and feature-set changes.

### Scope and philosophy

This fork follows an **evolutionary** approach:

- improve behavior users notice every day (stability, UX, workflow friction)
- keep changes understandable and reviewable
- prefer incremental improvements over large rewrites
- modernize tooling only when it improves reliability, maintainability, or contributor experience

When trade-offs exist, the default is to preserve existing workflows unless there is a strong reason to change them.

### What this fork is not

This fork is **not** trying to redefine the instrument from scratch.

- it is not a "break everything and redesign all assumptions" project
- it is not focused on novelty at the cost of usability
- it is not a compatibility-breaking experiment branch

The goal is practical progress: smoother development, better maintainability, and better day-to-day use.

### What you can expect

If you are new to the project, expect steady and understandable improvements rather than abrupt rewrites. The focus is on:

- compatibility-minded evolution
- clear development workflows
- maintainable code that is easier to understand, test, and extend

For contributors, this means code and tooling changes should be explainable, justified, and useful in real-world workflows.
For users, it means updates should feel like improvements, not surprises.

## At a glance

This repository contains:

- firmware for the STM32 hardware target
- a desktop simulator for fast UI/engine iteration
- unit and integration tests
- helper scripts and tools for building and flashing

Verified top-level setup targets from the repository `Makefile`:

- `make tools_install`
- `make setup_stm32`
- `make setup_sim`

## How to use this README

This guide is written for both first-time contributors and existing maintainers.

If you are new, follow this order:

1. `Quick start`
2. Your platform subsection in `Development setup`
3. `Hardware workflow` and/or `Simulator workflow`
4. `Troubleshooting` if anything fails

If you already know the project, use the table of contents as a quick command/reference index.

Suggested paths:

- **Simulator-first contributors**: `Quick start` -> your platform setup -> `Simulator workflow` -> `Troubleshooting`
- **Hardware-focused contributors**: `Quick start` -> your platform setup -> `Hardware workflow` -> flashing section
- **WSL/Windows users**: read the full `Windows (WSL)` section and the WSLg pitfalls before first build

## Quick start

Clone with submodules:

```bash
git clone --recursive https://github.com/napolitano/performer-nx.git
cd performer-nx
```

> Windows + WSL users: clone inside the Linux filesystem (for example `/home/<user>/projects`), not under `/mnt/c/...`, to avoid major compile-time slowdowns.

Typical first-time setup:

```bash
make tools_install
make setup_stm32
make setup_sim
```

Expected result after setup:

- the `build/stm32/{debug,release}` directories exist and are configured
- the `build/sim/{debug,release}` directories exist and are configured
- the toolchain/OpenOCD tools are available under `tools/`

Then:

- build firmware from `build/stm32/release`
- build and run the simulator from `build/sim/debug`

You can run only the parts you need. For example, simulator-only development does not require hardware flashing.

## Platform support

| Platform | Hardware build | Simulator build | Simulator audio/graphics | Simulator MIDI |
|---|---:|---:|---:|---:|
| Linux | Yes | Yes | Yes | Yes |
| macOS | Yes | Yes | Yes | Yes |
| Windows 11 via WSL2/WSLg | Yes | Yes | Usually yes with WSLg | No |

WSL is good for building firmware and for basic simulator work. Native Linux or macOS remains the better choice if you need full simulator functionality, especially MIDI.

## Development setup

### Linux / macOS

Make sure you have a recent CMake and a standard C/C++ build toolchain installed.

If you only want simulator work at first, you can start with `make setup_sim` and defer hardware tooling until needed.

On Debian/Ubuntu, a reasonable base setup is:

```bash
sudo apt-get update
sudo apt-get install libtool autoconf cmake libusb-1.0.0-dev libftdi-dev pkg-config
```

Install the embedded toolchain and OpenOCD:

```bash
make tools_install
```

Generate build directories:

```bash
make setup_stm32
make setup_sim
```

After this, continue with `Hardware workflow` and/or `Simulator workflow`.

### Windows (WSL)

This section targets **Windows 11 + WSL2 + WSLg**.

> Important: do not clone the repository into a path that contains spaces.

> Important for performance: keep the repository in the Linux filesystem (for example `~/projects/performer-nx`). Building from `/mnt/c/...` is significantly slower.

Recommended directory layout for Windows 11 users:

```text
/home/<user>/projects/performer-nx
```

This avoids the common slow-path where source lives on the Windows filesystem.

#### 1. Install WSL

Run in PowerShell as Administrator:

```powershell
wsl --install
```

If needed:

```powershell
wsl --install -d Ubuntu
wsl -l -v
```

#### 2. Recommended WSL configuration

Open:

```powershell
notepad %USERPROFILE%\.wslconfig
```

Use:

```ini
[wsl2]
networkingMode=mirrored
localhostForwarding=true
```

Then restart WSL:

```powershell
wsl --shutdown
```

#### 3. Install build dependencies inside WSL

```bash
sudo apt update
sudo apt install build-essential gdb cmake git pkg-config openocd linux-tools-common linux-tools-standard-WSL2
sudo apt install libsdl2-dev python3-dev pybind11-dev mesa-utils libgl1-mesa-dri libglx-mesa0 alsa-utils
```

#### 4. Optional graphics hint for WSLg / Mesa

```bash
export GALLIUM_DRIVER=d3d12
echo 'export GALLIUM_DRIVER=d3d12' >> ~/.bashrc
source ~/.bashrc
```

#### 5. First-time setup inside WSL

```bash
make tools_install
make setup_stm32
make setup_sim
```

#### WSL notes

- Graphics and audio usually work through WSLg.
- MIDI is not available in the simulator under WSLg because the ALSA sequencer interface (`/dev/snd/seq`) is not exposed.

### Recommended Windows 11 + WSLg + CLion workflow

1. Install/verify WSL2 + Ubuntu and complete the setup steps above.
2. Clone the repository inside WSL (for example under `~/projects`).
3. Open the project in CLion using the WSL toolchain.
4. Use WSL-based CMake profiles (sim/stm32) for configure/build.
5. Run simulator/debug tasks from those WSL profiles.

Why this matters:

- keeps file I/O fast
- keeps compiler/toolchain paths consistent
- avoids subtle host/guest path and environment mismatches

### Windows 11 / WSLg pitfalls and best practices

#### 1) File-system location matters (a lot)

- Prefer: `/home/<user>/projects/performer-nx`
- Avoid for active builds: `/mnt/c/...`

Reason: cross-filesystem I/O between Windows and Linux layers is slower and can dramatically increase CMake/build times.

#### 2) Use WSL terminal for build commands

Run all setup/build commands inside WSL:

```bash
make tools_install
make setup_stm32
make setup_sim
```

#### 3) Expect no MIDI in WSLg simulator

This is a platform limitation, not a project bug.

#### 4) Restart WSL after network/config changes

```powershell
wsl --shutdown
```

#### 5) Verify WSLg session when graphics/audio are odd

```bash
echo $WAYLAND_DISPLAY
```

If this is empty, restart WSL and launch a fresh session.

## IDE / editor workflow

Main development is currently done in **JetBrains CLion**.

- Recommended on Windows: run CLion with the WSL toolchain and open the project from the WSL path (for example `\\wsl$\Ubuntu\home\<user>\projects\performer-nx`).
- Source code can still be edited with any IDE or text editor.

Using other editors is fine; just keep build/configure tasks in WSL for consistency.

If you use CLion, preferred defaults are:

- toolchain: WSL
- generator: Ninja (if available) or default Makefiles
- build directories: existing `build/sim/debug` and `build/stm32/release` (or matching profile-specific paths)

If you hit line-ending or shebang issues in submodules, this recovery sequence can help:

```bash
git submodule foreach --recursive 'git reset --hard'
git submodule foreach --recursive 'git clean -fdx'
git submodule foreach --recursive 'git config core.autocrlf false'
git submodule foreach --recursive 'git rm --cached -r . || true'
git submodule foreach --recursive 'git reset --hard'
```

Then rerun setup:

```bash
make setup_stm32
make setup_sim
```

## Build directories

Generated build trees live under:

- `build/stm32/debug`
- `build/stm32/release`
- `build/sim/debug`
- `build/sim/release`

In general:

- use `release` for hardware builds
- use `debug` for simulator work and debugging

Recommended defaults:

- hardware: `build/stm32/release`
- simulator: `build/sim/debug`

## Hardware workflow

Enter the STM32 release build directory:

```bash
cd build/stm32/release
```

Build everything:

```bash
make -j
```

Build only one target when iterating:

```bash
make -j sequencer
```

Useful targets:

- `make -j sequencer`
- `make -j sequencer_standalone`
- `make -j bootloader`
- `make -j tester`
- `make -j tester_standalone`

Typical output files for `sequencer` are written below `src/apps/sequencer` in the build tree:

- `sequencer` – ELF with debug symbols
- `sequencer.bin` – raw binary
- `sequencer.hex` – Intel HEX
- `sequencer.srec` – Motorola SREC
- `sequencer.list` – disassembly listing
- `sequencer.map` – linker map
- `sequencer.size` – section sizes
- `UPDATE.DAT` – bootloader update file

`UPDATE.DAT` is the file you copy to the SD card root for bootloader-based firmware update.

Typical update flow:

1. Build `sequencer`
2. Copy `UPDATE.DAT` to SD card root
3. Boot device into bootloader/update mode
4. Apply update on device

### Bootloader target (read first)

Warning: the bootloader is safety-critical. Change it only if it is truly necessary and you understand the consequences.

- A broken bootloader can block normal SD-card update workflows.
- Bootloader changes should be tested carefully with reliable SWD/JTAG access available.
- Prefer changing the sequencer app first; touch bootloader code only when required.

Build the bootloader from the STM32 release tree:

```bash
cd build/stm32/release
make -j bootloader
```

Output artifacts are generated in `build/stm32/release/src/apps/bootloader/` including:

- `bootloader`
- `bootloader.bin`
- `bootloader.hex`
- `bootloader.srec`
- `bootloader.list`
- `bootloader.map`
- `bootloader.size`

### HWCONFIG target

The HWCONFIG app writes hardware configuration values. It is useful for initial hardware setup, board variants, and recovery workflows.

Build options come from `src/apps/hwconfig/CMakeLists.txt` and are exposed as dedicated targets:

- `hwconfig_default`
- `hwconfig_dac8568a`
- `hwconfig_invert-leds`
- `hwconfig_invert-leds_dac8568a`
- `hwconfig_reverse-encoder`
- `hwconfig_reverse-encoder_dac8568a`
- `hwconfig_reverse-encoder_invert-leds`
- `hwconfig_reverse-encoder_invert-leds_dac8568a`

Example:

```bash
cd build/stm32/release
make -j hwconfig_default
```

Pick the HWCONFIG target that matches your hardware variant; flashing the wrong variant can lead to confusing UI/IO behavior.

### Flashing with ST-Link V3 and JTAG

Flashing requires an SWD/JTAG debug probe (for example ST-Link V3) connected to the device debug header.

Use `.hex` files when possible: they already include addresses, reducing the risk of writing to the wrong flash region.

Before flashing:

- verify connector orientation/polarity on the device header
- use ESD-safe handling
- use a stable USB port/cable
- avoid connect/disconnect cycles while the target is powered

#### Command-line flashing

For standard targets, the repository already provides OpenOCD-based make targets:

Example:

```bash
make -j flash_bootloader
make -j flash_sequencer
```

You can also flash explicit `.hex` artifacts with OpenOCD. Typical pattern:

```bash
openocd \
  -f interface/<your-interface>.cfg \
  -f target/stm32f4x.cfg \
  -c "program <artifact>.hex verify reset exit"
```

Flashing behavior is configured in `src/platform/stm32/CMakeLists.txt`. Default settings currently use `OPENOCD_INTERFACE=ftdi/olimex-arm-usb-ocd-h` and `OPENOCD_TARGET=stm32f4x`.

Available OpenOCD interface scripts can be found under:

```text
tools/openocd/share/openocd/scripts/interface
```

#### STM32CubeProgrammer (GUI)

For initial bootloader flashing, STM32CubeProgrammer is often the easiest and safest path.

Recommended GUI flow:

1. Connect ST-Link V3 to the device SWD/JTAG header.
2. Open STM32CubeProgrammer.
3. Select `ST-LINK` connection mode and connect.
4. Open the `.hex` artifact you want to flash (for example `bootloader.hex`).
5. Start programming with verify enabled.
6. Reset/power-cycle target and confirm expected startup behavior.

If you prefer CLI, use STM32CubeProgrammer's command-line tool (`STM32_Programmer_CLI`) and validate options with:

```bash
STM32_Programmer_CLI --help
```

## On-Device Debugging

This section describes a practical and safety-first approach for debugging directly on the device MCU.

### Why ST-Link V3

For this project, ST-Link V3 is a strong default choice during development:

- affordable and widely available
- solid support in STM32 tooling
- generally more reliable and faster than low-cost ST-Link V2 clones
- better long-session behavior for iterative flash/debug cycles

Cheap ST-Link V2 clones are often inconsistent (speed, stability, support) and are a poor fit for serious on-device development.

### Critical safety notes (read first)

The JTAG header on the device connects directly to MCU debug pins without additional protection stages.

Treat this path as sensitive:

- verify connector orientation and polarity every time
- prevent ESD events (ground yourself, avoid synthetic surfaces, handle carefully)
- avoid hot-plugging USB/debug cables while the target board is powered
- avoid unstable USB ports/hubs that can cause brownouts or voltage glitches
- prefer a known-good USB port on the host machine

If something is uncertain, stop and re-check wiring and power sequencing before connecting.

### Recommended debugging procedure

1. Power down the target device.
2. Connect ST-Link V3 to the bottom JTAG header with verified orientation.
3. Connect ST-Link to a stable USB port on the host.
4. Start your debug server/tooling (OpenOCD or STM tools).
5. Power target and connect debugger.
6. Flash/test/debug.
7. Stop debug session.
8. Power down target before disconnecting debug hardware.

This conservative sequence reduces risk of accidental electrical stress on MCU pins.

### STM toolset and OpenOCD options

Common STM tooling choices:

- STM32CubeProgrammer (flash/programming and device connectivity checks)
- STM32CubeIDE (integrated debug workflow)
- STM32CubeCLT / STM debug server tools (command-line and scripted workflows)

OpenOCD remains a good option and is already integrated in this repository via `flash_*` targets.
For ST-Link hardware, you may need to adjust `OPENOCD_INTERFACE` in `src/platform/stm32/CMakeLists.txt` to match your adapter and local OpenOCD interface scripts.

### Build strategy for reliable on-device debugging

For this firmware, pure "full debug" builds can become too large or less representative for timing-sensitive behavior.
In practice, a release-oriented build with debug symbols is often the better compromise.

Recommended approach:

- start from `build/stm32/release`
- keep symbols enabled for source-level breakpoint mapping
- keep optimizations close to realistic runtime behavior

Explicit configure parameters for symbolized release builds:

```bash
cd build/stm32/release

# Reconfigure this build directory with release-like optimization + debug symbols
cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_FLAGS_RELEASE="-O2 -g3" \
  -DCMAKE_CXX_FLAGS_RELEASE="-O2 -g3" \
  .
```

Easy start (and retry) command sequence:

```bash
cd build/stm32/release

# Fast iteration for on-device debug work
make -j sequencer

# If you changed shared code or want to refresh everything
make -j
```

If a build fails after an interruption, rerun the same command first. If needed, switch to the single-target build (`make -j sequencer`) for faster feedback.

Watchdog note:

- stepping/halting can interact badly with watchdog behavior if watchdog is enabled
- this codebase provides a watchdog switch via `CONFIG_DISABLE_WATCHDOG`
- see `src/apps/sequencer/Config.h` (currently enabled in this fork)

If you re-enable watchdog for testing, expect debugger halt/resume behavior to require careful timing.

### When to debug on hardware vs simulator

Prefer on-device debugging for:

- filesystem/SD-card behavior
- real timing and scheduling effects
- external clocking and hardware IO interactions
- USB/device-level behavior on real peripherals
- performance validation under target constraints

Prefer simulator for:

- fast UI iteration
- logic debugging with quicker rebuild/run cycles
- non-hardware-dependent model/engine changes
- early feature prototyping before hardware validation

Practical workflow:

1. Develop and iterate quickly in simulator.
2. Validate hardware-sensitive behavior on real device.
3. Keep both paths in regular use to avoid regressions.

## Simulator workflow

Enter the simulator debug build directory:

```bash
cd build/sim/debug
```

Build:

```bash
make -j
```

Run:

```bash
./src/apps/sequencer/sequencer
```

Start the simulator from the build directory so it can find its assets correctly.

Typical simulator loop:

1. Edit code
2. Rebuild in `build/sim/debug`
3. Run simulator
4. Repeat

## Troubleshooting

### Build configuration issues

- If CMake configuration is stale, rerun:

```bash
make setup_stm32
make setup_sim
```

- If toolchain binaries are missing, rerun:

```bash
make tools_install
```

### WSL-specific issues

- If graphics are unstable, verify WSLg is active and retry with:

```bash
echo $WAYLAND_DISPLAY
```

- MIDI is expected to be unavailable in WSL simulator (ALSA sequencer limitation).

### Submodule / line-ending issues

Use the recovery block in the WSL section, then rerun setup.

### "It still fails"

When reporting issues, include:

- platform (Linux/macOS/WSL)
- build directory used (`build/stm32/...` or `build/sim/...`)
- full command run
- full error output

## Source tree overview

### Top-level source layout

- `src/apps` – applications
- `src/core` – shared core code used across applications
- `src/libs` – third-party source bundled in the tree
- `src/os` – shared OS helpers
- `src/platform` – simulator and STM32 platform implementations
- `src/test` – test infrastructure
- `src/tests` – unit and integration tests

### Applications

- `src/apps/bootloader` – bootloader
- `src/apps/hwconfig` – hardware configuration assets/code
- `src/apps/sequencer` – main sequencer application
- `src/apps/tester` – hardware test application

### Sequencer application layout

- `asteroids` – Asteroids mini-game
- `engine` – timing, sequencing, routing, outputs
- `model` – runtime data model and state transitions
- `python` – Python bindings for simulator/test tooling
- `tests` – Python-based tests
- `ui` – pages, painters, controllers, and general UI logic

For deeper implementation details, see the documents in `doc/`, especially:

- `doc/DesignDocument.md`
- `doc/MemoryMap.md`
- `doc/PinMap.md`

## Third-party libraries

This project uses, among others:

- [FreeRTOS](http://www.freertos.org)
- [libopencm3](https://github.com/libopencm3/libopencm3)
- [libusbhost](https://github.com/libusbhost/libusbhost)
- [NanoVG](https://github.com/memononen/nanovg)
- [FatFs](http://elm-chan.org/fsw/ff/00index_e.html)
- [stb_sprintf](https://github.com/nothings/stb/blob/master/stb_sprintf.h)
- [stb_image_write](https://github.com/nothings/stb/blob/master/stb_image_write.h)
- [soloud](https://sol.gfxile.net/soloud/)
- [RtMidi](https://www.music.mcgill.ca/~gary/rtmidi/)
- [pybind11](https://github.com/pybind/pybind11)
- [tinyformat](https://github.com/c42f/tinyformat)
- [args](https://github.com/Taywee/args)

## License

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

This work is licensed under the [MIT License](https://opensource.org/licenses/MIT).
