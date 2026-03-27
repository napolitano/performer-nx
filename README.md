![Build Status](https://github.com/westlicht/performer/actions/workflows/ci.yml/badge.svg?branch=master)

# PER|FORMER

<a href="doc/sequencer.jpg"><img src="doc/sequencer.jpg"/></a>

## Overview

This repository contains the firmware for the **PER|FORMER** eurorack sequencer.

For more information on the project go [here](https://westlicht.github.io/performer).

The hardware design files are hosted in a separate repository [here](https://github.com/westlicht/performer-hardware).

## Development

If you want to do development on the firmware, the following is a quick guide on how to setup the development environment to get you going.

### Setup on macOS and Linux

First you have to clone this repository (make sure to add the `--recursive` option to also clone all the submodules):

```
git clone --recursive https://github.com/westlicht/performer.git
```

After cloning, enter the performer directory:

```
cd performer
```

Make sure you have a recent version of CMake installed. If you are on Linux, you might also want to install a few other packages. For Debian based systems, use:

```
sudo apt-get install libtool autoconf cmake libusb-1.0.0-dev libftdi-dev pkg-config
```

To compile for the hardware and allow flashing firmware you have to install the ARM toolchain and build OpenOCD:

```
make tools_install
```

Next, you have to setup the build directories:

```
make setup_stm32
```

If you also want to compile/run the simulator use:

```
make setup_sim
```

The simulator is great when developing new features. It allows for a faster development cycle and a better debugging experience.

### Setup on Windows

#### Windows Setup (WSL)

**Important:**  
Do not clone the repository into a path that contains spaces. This can break builds and tooling.

---

#### 1. Install WSL (Windows Subsystem for Linux)

Open PowerShell as Administrator:

```powershell
wsl --install
```

Verify installation:

```powershell
wsl -l -v
```

If Ubuntu is not installed yet:

```powershell
wsl --install -d Ubuntu
```

Launch WSL once and complete the initial setup.

---

#### 2. Configure WSL (Networking and Integration)

WSL networking is a common source of issues.

Edit:

```powershell
notepad %USERPROFILE%\.wslconfig
```

```ini
[wsl2]
networkingMode=mirrored
localhostForwarding=true
```

Ensure in WSL settings:

- Networking mode: Mirrored
- Localhost forwarding: Enabled
- Host address loopback: Enabled
- DNS tunneling: Disabled

Apply:

```powershell
wsl --shutdown
```

Note: With this configuration, `/etc/resolv.conf` should not require manual changes.

---

#### 3. Install Build Toolchain

```bash
sudo apt update
sudo apt install build-essential gdb cmake git pkg-config
```

---

#### 4. Install Simulator Dependencies

```bash
sudo apt install libsdl2-dev python3-dev pybind11-dev mesa-utils libgl1-mesa-dri libglx-mesa0 alsa-utils
```

---

#### 5. Graphics Configuration (Mesa / D3D12)

```bash
export GALLIUM_DRIVER=d3d12
```

Persist:

```bash
echo 'export GALLIUM_DRIVER=d3d12' >> ~/.bashrc
source ~/.bashrc
```

---

#### 6. Development Workflow

Start WSL before each session:

```powershell
wsl
```

All build steps must run inside WSL:

```bash
# First-time setup
make tools_install
make setup_stm32
make setup_sim

# Build firmware for hardware
cd build/stm32/<debug|release>/
make -j

# The resulting UPDATE.DAT is located in:
#   src/apps/sequencer/
# Copy it to the root directory of the SD card.

# Build and run the simulator
cd build/sim/debug/
make -j
./src/apps/sequencer/sequencer
```

---

#### Audio

##### WSLg (Preferred)

WSLg provides:

- Audio (PulseAudio-compatible)
- Graphics (Wayland/X11)

Verify:

```bash
echo $WAYLAND_DISPLAY
```

Test audio:

```bash
paplay /usr/share/sounds/alsa/Front_Center.wav
```

No configuration required.

---

##### PulseAudio (Manual Setup)

Only required if WSLg is unavailable.

Start server on Windows:

```powershell
pulseaudio.exe --load=module-native-protocol-tcp --exit-idle-time=-1
```

WSL:

```bash
export PULSE_SERVER=tcp:localhost
```

---

#### MIDI

Not supported in WSL.

- No `/dev/snd/seq`
- ALSA sequencer unavailable
- RtMidi ALSA backend fails

---

#### Simulator Status

- Build: works
- Graphics: works
- Audio: works (WSLg or PulseAudio)
- MIDI: not available

---

#### Recommendation

WSL is suitable for:

- Building firmware
- Basic simulator use

Use native Linux or macOS for full functionality.


### Build directories

After successfully setting up the development environment you should now have a list of build directories found under `build/[stm32|sim]/[release|debug]`. The `release` targets are used for compiling releases (more code optimization, smaller binaries) whereas the `debug` targets are used for compiling debug releases (less code optimization, larger binaries, better debugging support).

### Developing for the hardware

You will typically use the `release` target when building for the hardware. So you first have to enter the release build directory:

```
cd build/stm32/release
```

To compile everything, simply use:

```
make -j
```

Using the `-j` option is generally a good idea as it enables parallel building for faster build times.

To compile individual applications, use the following make targets:

- `make -j sequencer` - Main sequencer application
- `make -j sequencer_standalone` - Main sequencer application running without bootloader
- `make -j bootloader` - Bootloader
- `make -j tester` - Hardware tester application
- `make -j tester_standalone` - Hardware tester application running without bootloader

Building a target generates a list of files. For example, after building the sequencer application you should find the following files in the `src/apps/sequencer` directory relative to the build directory:

- `sequencer` - ELF binary (containing debug symbols)
- `sequencer.bin` - Raw binary
- `sequencer.hex` - Intel HEX file (for flashing)
- `sequencer.srec` - Motorola SREC file (for flashing)
- `sequencer.list` - List file containing full disassembly
- `sequencer.map` - Map file containing section/offset information of each symbol
- `sequencer.size` - Size file containing size of each section

If compiling the sequencer, an additional `UPDATE.DAT` file is generated which can be used for flashing the firmware using the bootloader.

To simplify flashing an application to the hardware during development, each application has an associated `flash` target. For example, to flash the bootloader followed by the sequencer application use:

```
make -j flash_bootloader
make -j flash_sequencer
```

Flashing to the hardware is done using OpenOCD. By default, this expects an Olimex ARM-USB-OCD-H JTAG to be attached to the USB port. You can easily reconfigure this to use a different JTAG by editing the `OPENOCD_INTERFACE` variable in the `src/platform/stm32/CMakeLists.txt` file. Make sure to change both occurrences. A list of available interfaces can be found in the `tools/openocd/share/openocd/scripts/interface` directory (or `/home/vagrant/tools/openocd/share/openocd/scripts/interface` when running the virtual machine).

### Developing for the simulator

Note that the simulator is only supported on macOS and Linux and does not currently run in the virtual machine required on Windows.

You will typically use the `debug` target when building for the simulator. So you first have to enter the debug build directory:

```
cd build/sim/debug
```

To compile everything, simply use:

```
make -j
```

To run the simulator, use the following:

```
./src/apps/sequencer/sequencer
```

Note that you have to start the simulator from the build directory in order for it to find all the assets.

### Source code directory structure

The following is a quick overview of the source code directory structure:

- `src` - Top level source directory
- `src/apps` - Applications
- `src/apps/bootloader` - Bootloader application
- `src/apps/hwconfig` - Hardware configuration files
- `src/apps/sequencer` - Main sequencer application
- `src/apps/tester` - Hardware tester application
- `src/core` - Core library used by both the sequencer and hardware tester application
- `src/libs` - Third party libraries
- `src/os` - Shared OS helpers
- `src/platform` - Platform abstractions
- `src/platform/sim` - Simulator platform
- `src/platform/stm32` - STM32 platform
- `src/test` - Test infrastructure
- `src/tests` - Unit and integration tests

The two platforms both have a common subdirectories:

- `drivers` - Device drivers
- `libs` - Third party libraries
- `os` - OS abstraction layer
- `test` - Test runners

The main sequencer application has the following structure:

- `asteroids` - Asteroids game
- `engine` - Engine responsible for running the sequencer core
- `model` - Data model storing the live state of the sequencer and many methods to change that state
- `python` - Python bindings for running tests using python
- `tests` - Python based tests
- `ui` - User interface

## Third Party Libraries

The following third party libraries are used in this project.

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

This work is licensed under a [MIT License](https://opensource.org/licenses/MIT).
