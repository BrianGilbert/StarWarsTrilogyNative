# Star Wars Trilogy Native on Linux

The Linux build is maintained by contributors. It isn't tested by the project's author. It runs on x86_64 and arm64 (aarch64) with X11 or Wayland.

## Build

You need a GPU with a Vulkan 1.3 driver (or 1.2 with `VK_KHR_dynamic_rendering`), Python 3, a C++20 compiler (clang is used if there is one, else g++), CMake
3.27 or later and Ninja (or `build_linux.sh` fetches pinned copies of those two into `build/`), and the development
files SDL3 needs for the window, the sound and the gamepads. On Debian or Ubuntu:

```sh
sudo apt install clang cmake ninja-build python3 pkg-config \
    libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxss-dev libxfixes-dev libxtst-dev \
    libxkbcommon-dev libwayland-dev wayland-protocols libdecor-0-dev libegl-dev \
    libpulse-dev libpipewire-0.3-dev libasound2-dev libudev-dev libdbus-1-dev libvulkan1
```

On Arch: `sudo pacman -S --needed clang cmake ninja python pkgconf libx11 libxext libxrandr libxcursor libxi libxss
libxkbcommon wayland wayland-protocols libdecor libpulse pipewire alsa-lib systemd-libs dbus vulkan-icd-loader`.

1. Put `swtrilgy.zip` in the folder (or in `roms/`, or pass its path: `./build_linux.sh ~/roms/swtrilgy.zip`).
2. Run `./build_linux.sh`. It downloads SDL3, miniz and FreeType on the first run (all three built into the program)
   and leaves `StarWarsTrilogy` in the folder.
3. Play with `./StarWarsTrilogy`, from this folder or anywhere else (it goes to its own folder).

Settings, records and saves are kept in the folder, as on Windows.

## arm64 from an x86_64 PC

`./build_linux.sh --arch aarch64` cross-compiles with clang. It needs the arm64 versions of the libraries above and
`qemu-user` (the build runs its shader compiler and the tests for arm64 through it). On Debian or Ubuntu:

```sh
sudo dpkg --add-architecture arm64 && sudo apt update
sudo apt install lld qemu-user libc6-dev:arm64 libstdc++-14-dev:arm64 libgcc-14-dev:arm64 \
    libx11-dev:arm64 libxext-dev:arm64 libxrandr-dev:arm64 libxcursor-dev:arm64 libxi-dev:arm64 libxss-dev:arm64 \
    libxfixes-dev:arm64 libxtst-dev:arm64 libxkbcommon-dev:arm64 libwayland-dev:arm64 libdecor-0-dev:arm64 \
    libegl-dev:arm64 libpulse-dev:arm64 libpipewire-0.3-dev:arm64 libasound2-dev:arm64 libudev-dev:arm64 \
    libdbus-1-dev:arm64
```

On an arm64 computer itself, `./build_linux.sh` builds for it as on x86_64.

## Raspberry Pi 5

The arm64 build is made with the Raspberry Pi 5 in mind (built on it with `./build_linux.sh`, or on a PC with
`--arch aarch64`), with these limits:

- The GPU's driver (Mesa's v3dv) has Vulkan 1.3 from Mesa 24.3 (**Raspberry Pi OS Trixie**); with Mesa 24.2
  (Bookworm, updated) it has Vulkan 1.2 and dynamic rendering as an extension, which the game takes too (it then
  says `Vulkan 1.2 with VK_KHR_dynamic_rendering`). With an older driver the game names it and its Vulkan version
  when it starts. If the log says the picture is drawn `on the processor` (Mesa's llvmpipe), no GPU driver was
  usable: update the system.
- Built on a PC, the program needs a system at least as recent as the PC's (its glibc and C++ library): building on
  the Pi itself is the simplest.
- The GPU has no descriptor indexing: the game runs without the procedural skies (the game's own skies instead)
  and without ray tracing (the shadow map draws the sun shadows).
- It is far slower than a desktop GPU: start with sun shadows, ambient occlusion and anti-aliasing off
  (**Options > Graphics**, **Options > Display**), bloom low, and a 1280x720 or 1920x1080 screen.

## Compared with Windows

- Ray tracing works as on Windows, with a driver that has ray queries (NVIDIA, recent AMD and Intel with Mesa).
- No DLAA or DLSS yet: **Options > Display > Anti-aliasing** offers FXAA (cheap) and MSAA. **Options > Display > Render scale** above 100 % draws the
  3D larger and scales it down, for smoother edges still on a fast GPU (below 100 %: faster).
- No splash video: the splash picture shows in its place.
- One mouse: a second player uses a gamepad.
- No `update.bat`: to update, update the folder (`git pull`) and run `./build_linux.sh` again.

F11 or Alt+Enter switches between the window and full screen (**Options > Display > Mode**).

For how the port works, see `docs/ARCHITECTURE.md`.
