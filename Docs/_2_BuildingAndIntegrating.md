# Building & Integrating

Recast is designed to be integrated into your project as source files and built with whatever build system you use.

For building RecastDemo and for Recast library development there are [premake5](http://premake.github.io/) and [cmake](https://cmake.org/) build scripts.  Premake5 is recommended unless you're already familiar with cmake.

## Building RecastDemo

While Recast and Detour don't require any dependencies, RecastDemo requires SDL (not included), and the unit tests rely on Catch2 (included in the repository).

The following steps outline the process to install SDL2 in the appropriate location and build RecastDemo with Premake and your local .  Before you begin, you'll need to [download](https://github.com/premake/premake-core/releases) and install premake5 and ensure that the `premake5` executable location is included in your system path.

You can also reference the github actions build script `.github/Build.yaml` for examples of building with specific platform and toolchain combinations.

### Windows

- Grab the latest SDL2 development library release from [here](https://github.com/libsdl-org/SDL) and unzip it into `RecastDemo/Contrib`.  Rename the SDL folder such that the path `RecastDemo/Contrib/SDL/lib/x86` is valid.
- Navigate to the `RecastDemo` folder and run `premake5 vs2022`
- Open `Build/vs2022/recastnavigation.sln` in Visual Studio 2022 or Jetbrains Rider.
- Set `RecastDemo` as the startup project, build, and run.

### macOS

- Grab the latest SDL2 development library dmg from [here](https://github.com/libsdl-org/SDL) and place `SDL2.framework` in `RecastDemo/Bin`
- Navigate to the `RecastDemo` folder and run `premake5 xcode4`
- Open `Build/xcode4/recastnavigation.xcworkspace` in XCode
- Set the RecastDemo project as the target and build.

For a CMake build, place `SDL2.framework` in `RecastDemo/Bin` (or set
`-DSDL2_ROOT_DIR=/path/to/framework/parent`), then run from the repository root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target RecastDemo
open build/RecastDemo/RecastDemo.app
```

The CMake app contains SDL2 in `Contents/Frameworks`, and the test cases and font
in `Contents/Resources`. Sample meshes are copied to a `model` folder next to
`RecastDemo.app`, and empty `bin` and `gset` folders are created there as well.
The app can be launched from Finder or any working directory without setting
`DYLD_FRAMEWORK_PATH`. Keep `model`, `bin`, and `gset` beside the app when moving
it. `cmake --install build --prefix /path/to/install` installs
`bin/RecastDemo.app` and the sample meshes in `bin/model`.

On macOS, RecastDemo reads `.obj` models from the `model` folder next to the app.
Navmesh and tile-cache `.bin` files are saved in the sibling `bin` folder.
Pressing **9** saves geometry sets in the sibling `gset` folder, and those sets
appear in the Input Mesh list together with the models. `imgui.ini` is saved in
`~/Library/Application Support/RecastDemo/`.

Exports use the source model's basename: `model/dungeon.obj` produces
`bin/dungeon.bin` and `gset/dungeon.gset`. Loading a geometry set uses the name of
the model it references for subsequent exports. Saving another sample mode for
the same model replaces its `.bin` file; navmesh and tile-cache formats are
different, so load the file in the mode that saved it.

Runtime output directories are created when saving. A read-only installation
can still start and load models, but saving requires write access to the sibling
`bin` and `gset` folders. Missing external models in geometry sets report a load
error; only recognized legacy bundled references can fall back to `model`.

On Windows and Linux, RecastDemo reads resources next to the executable and saves
files in SDL's per-user application directory. Navmesh `.bin` files and
`imgui.ini` are saved there; pressing **9** saves geometry sets under `Meshes/`.
Saved sets and user `.obj` files in that directory appear in the Input Mesh list,
with user files taking precedence over bundled files of the same name. On Windows
the save directory is `%APPDATA%/RecastDemo/`; on Linux it is
`$XDG_DATA_HOME/RecastDemo/` (usually `~/.local/share/RecastDemo/`).

### Linux

- Install SDL2 and its dependencies according to your distro's guidelines.
- Navigate to the `RecastDemo` folder and run `premake5 gmake`
- Navigate to `RecastDemo/Build/gmake` and run `make`
- Navigate to `RecastDemo/Bin` and run `./RecastDemo`

## Preprocessor Defines

There are a few symbols you may wish to define when building Recast.

| Symbol                  | Usage                                                                                                                    |
|-------------------------|--------------------------------------------------------------------------------------------------------------------------|
| `RC_DISABLE_ASSERTS`    | Disables assertion macros. Useful for release builds that need to maximize performance. You can also customize Recasts's assetion behavior with your own assertion handler.  See `RecastAssert.h` and `DetourAssert.h`.
| `DT_POLYREF64`          | Use 64 bit (rather than 32 bit) polygon ID references. Generally not needed, but sometimes useful for very large worlds. |
| `DT_VIRTUAL_QUERYFILTER`| Define this if you plan to sub-class `dtQueryFilter`. Enables the virtual destructor in `dtQueryFilter`.                 |

## Running Unit tests

- Follow the instructions to build RecastDemo above.  Premake should generate another build target called "Tests".
- Build the "Tests" project.  This will generate an executable named "Tests" in `RecastDemo/Bin/`
- Run the "Tests" executable.  It will execute all the unit tests, indicate those that failed, and display a count of those that succeeded.  Check out the [Catch2 documentation](https://github.com/catchorg/Catch2/blob/devel/docs/command-line.md#top) for information on additional command line options.

## Integration

There are a few ways to integrate Recast and Detour into your project.  Source integration is the most popular and most flexible, and is what the project was designed for from the beginning.

### Source Integration

It is recommended to add the source directories `DebugUtils`, `Detour`, `DetourCrowd`, `DetourTileCache`, and `Recast` directly into your project depending on which parts of the project you need. For example your level building tool could include `DebugUtils`, `Recast`, and `Detour`, and your game runtime could just include `Detour`.

- *Recast*: Core navmesh building system.
- *Detour*: Runtime navmesh interface and query system
- *DetourCrowd*: Runtime movement, obstacle avoidance and crowd sim systems
- *DetourTileCache*: Runtime navmesh dynamic obstacle and re-baking system

### Installation through vcpkg

If you are using the [vcpkg](https://github.com/Microsoft/vcpkg/) dependency manager you can alternatively download, build, and install Recast with:

```
vcpkg install recast
```

## Customizing Allocation Behavior

Recast and Detour strive to avoid heap allocations whenever possible.  In the cases where they are needed, all allocations are routed through allocation functions that by default just wrap `malloc` and `free`.  Additionally, the `rcAllocHint` enum gives some coarse-grained information about the intended lifetime of the allocation.  

You can specify your own `rcAllocFunc` and `rcFreeFunc` in `RecastAlloc.cpp` (and similarly in `DetourAlloc.cpp`) to tune heap usage to your specific needs.

## A Note on DLL exports and C API

Recast does not yet provide a stable C API for use in a DLL or as bindings for another language.  The design of Recast relies on some C++ specific features, so providing a stable API is not easy without a few significant changes to Recast.  

There are a number of projects that offer unofficial language bindings for Recast, but official support for a C API is currently on our [development roadmap](_99_Roadmap.md).
