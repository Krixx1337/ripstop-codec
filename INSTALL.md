# Installation

RipStop Codec is a C++20 static library.

## CMake subdirectory

```cmake
add_subdirectory(path/to/ripstop-codec)
target_link_libraries(my_app PRIVATE RipStopCodec::ripstop-codec)
```

Tests default on only when RipStop is the top-level project. Parent projects do not inherit test
headers, config definitions, or test targets. Explicit controls:

```cmake
set(RIPSTOP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(RIPSTOP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
```

## Installed package

```powershell
cmake --install out/build/x64-release --prefix C:\deps\ripstop
```

Consumer:

```cmake
find_package(RipStopCodec 1.1 CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE RipStopCodec::ripstop-codec)
```

Configure with `-DCMAKE_PREFIX_PATH=C:\deps\ripstop` when the prefix is not otherwise discoverable.

## Manual source integration

Add:

- include path: `include/`
- private implementation include path: `third_party/`
- C++ source: `src/RipStop.cpp`
- C source: `third_party/miniz/miniz.c`
- C definition for miniz: `MINIZ_NO_ZLIB_COMPATIBLE_NAMES=1`
- language modes: C++20 and C99

`CMakeLists.txt` remains the canonical source list.

## Project setup

Copy `templates/RipStop_Config.example.h` into the application, rename it if desired, and change
`kProjectSeed`. Built-in compression and scrambling need no custom code.

`tools/generate_config.py` is an optional advanced path for randomized constants:

```powershell
python tools/generate_config.py --out path\to\RipStop_Config.h
```

Generator refuses to overwrite an existing config unless `--force` is passed. Rotation changes
project identity and invalidates old encoded assets.

## Build and test

```powershell
cmake --preset x64-debug
cmake --build --preset x64-debug
ctest --preset x64-debug
```

Build examples with `-DRIPSTOP_BUILD_EXAMPLES=ON`.
