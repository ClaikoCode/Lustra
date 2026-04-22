# Lustra

**Lustra** is meant to be a modern Vulkan pet engine that can be used to experiement with graphics and try out cutting edge technologies. The engine will be built from scratch by myself and is intended to be used only by me. However, my goal is that the engine can be compiled and ran by others to test its features.

It is my first Vulkan project which means that many mistakes will be made and many lessons will be learned.

This engine is a work in progress and to keep the projects scope and complexity manageable, it is currently not planned to support any of the following:

* Older graphics hardware that doesnt support hardware accelerated ray tracing capabilities
* Graphics APIs other than Vulkan
* Mobile GPUs

## Cloning

    git clone --recurse-submodules <url>

If you already cloned without `--recurse-submodules`:

    git submodule update --init --recursive

## Building

### Prerequisites (one-time install per machine)

**Linux (Ubuntu 24.04):**
- clang 18+, cmake 3.30+, ninja
- Vulkan SDK 1.3+ (tarball from [LunarG](https://vulkan.lunarg.com/sdk/home#linux) and put `downloaded-sdk-version/setup-env.sh` into `~/.bashrc`)
- Install SDL3 [build dependencies](https://wiki.libsdl.org/SDL3/README-linux#build-dependencies)
- OTHER DEPENDENCIES NOT YET LISTED

**Windows:**
- Visual Studio 2026 with C++ workload
- Vulkan SDK 1.3+ from LunarG installer
- CMake 3.30+ (or use the one bundled with VS)

### Build

Inside of `Lustra/`:
    
    cmake --preset x64-[debug|release]
    cmake --build --preset [debug|release]
