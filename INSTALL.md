# RipStop Codec Integration Guide

`RipStop Codec` is a C++20 static library. This document covers the two supported integration paths for open-source and in-house projects:

1. CMake integration
2. manual source integration without CMake

`ProjectOptions` is intentionally project-owned input. In particular, `magic` does not have a library default, so each integration supplies its own file marker instead of inheriting a shared global signature.

## For CMake Projects

Add the library as a subdirectory and link the CMake target:

```cmake
add_subdirectory(path/to/ripstop-codec)
target_link_libraries(MyProject PRIVATE ripstop-codec)
```

You can also link the alias target:

```cmake
target_link_libraries(MyProject PRIVATE RipStopCodec::ripstop-codec)
```

This path automatically carries the public headers, the private third-party include path used by the codec implementation, and the C++20 requirement through the target definition in `CMakeLists.txt`.

## Manual Source Integration

Use this when your project does not use CMake and you want to compile the codec directly inside an existing build system such as a Visual Studio solution, custom game build pipeline, or another native project format.

1. Add `include/` to your project's include directories.
2. Add `third_party/` to your project's include directories.
3. Add `src/RipStop.cpp` to your project's source files.
4. Add `third_party/miniz/miniz.c` to your project's source files and compile it with `MINIZ_NO_ZLIB_COMPATIBLE_NAMES=1`.
5. Build the project as C++20 or newer.

Notes:

- `src/RipStop.cpp` is C++ and must be compiled with C++20 support.
- `third_party/miniz/miniz.c` is C and is bundled as the codec's compression backend.
- `MINIZ_NO_ZLIB_COMPATIBLE_NAMES=1` keeps the bundled `miniz` private so it does not collide with another linked `zlib` or `miniz`.
- No Visual Studio `.props` files are required. The repository keeps the integration surface intentionally small so the same instructions work across build systems.

## Generating your Project Config

Generate a randomized project-local config header with:

```bash
python tools/generate_config.py
```

By default this writes `RipStop_Config.h` into the current working directory. Use `--out path/to/RipStop_Config.h` when you want to place it somewhere specific.

The generated header now includes project-owned identity constants, a project-local `RIPSTOP_ERROR_XOR`, an obfuscated project secret, and helper builders for `ProjectOptions` and `AssetOptions`.

If you prefer to edit a checked-in template manually, start from [`templates/RipStop_Config.example.h`](./templates/RipStop_Config.example.h). It uses `ripstop::codec::utils::make_obfuscated_secret<...>()`, and `MakeProjectOptions()` resolves the secret at runtime with `.resolve()`.

## Technical Notes

### Simple Path

`AssetOptions` is optional in the public API, so the minimal integration path is:

```cpp
auto encoded = ripstop::codec::encode(bytes, project);
auto decoded = ripstop::codec::decode(encoded.value, project);
```

### Recommended Default Profile

For most integrations, start here:

```cpp
auto encoded = ripstop::codec::encode(bytes, project, {
    .format_tag = 0x11223344u,
    .context_seed = ripstop::codec::utils::hash_string("textures/player_idle"),
    .compress = true,
    .scramble = true,
});
```

Recommended policy:

- set `format_tag` per asset class
- derive `context_seed` from a stable logical asset identifier
- leave `nonce = 0`
- leave `password` empty
- use the built-in scrambler unless you have a defined project-specific requirement

Canonical `context_seed` policy example:

```cpp
const std::uint64_t context_seed =
    ripstop::codec::utils::hash_string("textures/player_idle");
```

This gives deterministic output for unchanged assets while still separating payload state per asset.

### Advanced Options

When you need extra separation, pass `AssetOptions` explicitly:

```cpp
auto encoded = ripstop::codec::encode(bytes, project, {
    .format_tag = 0x11223344u,
    .context_seed = 0x55667788u,
    .nonce = 0x1020304050607080ull,
    .padding_size = 16,
    .password = "Secret",
});
```

`AssetOptions` defaults to `compress = true` and `scramble = true`. You can disable either independently:

```cpp
auto encoded = ripstop::codec::encode(bytes, project, {
    .format_tag = 0x11223344u,
    .compress = false,
    .scramble = true,
});
```

`nonce` is optional and defaults to `0`, which preserves deterministic output for identical inputs. Set a non-zero `nonce` when you want anti-diffing behavior. `padding_size` inserts 0-255 junk bytes after the fixed header so the payload starts at a caller-selected offset.

Treat `password`, `nonce`, and custom `scrambler` support as advanced features. They are useful for specific workflows, but they are not required for the recommended default profile.

### Typed Encode/Decode Safety

The typed `encode()` / `decode()` overloads require `std::has_unique_object_representations_v<T>`. This intentionally rejects padded types.

```cpp
struct Bad {
    char a;
    int b;
}; // usually rejected due to padding

struct Good {
    int a;
    int b;
}; // OK
```

If your type is rejected, serialize it explicitly or define a packed/byte-stable representation.

### Custom Scrambler Integration

Projects can replace the built-in SplitMix64 XOR pass by filling `ProjectOptions::scramble_id` and `ProjectOptions::scrambler`. This is an advanced integration path. The header stores the scrambler id, and decode requires the caller to provide a matching scrambler id and function pair. If a non-zero `scramble_id` is configured without a scrambler function, encode/decode fails with `MissingScramblerFunc`.

The example blueprint lives in [`templates/RipStop_Config.example.h`](./templates/RipStop_Config.example.h).

### Header Peeking

Use `peek_header()` to validate or inspect files without full decode:

```cpp
auto header = ripstop::codec::peek_header(raw_file_bytes, project);
if (header && header.value.asset_version < 2) {
    // Trigger asset rebuild...
}
```

`peek_header()` requires the full `ProjectOptions` because header fields from offset 4 onward are XOR-masked with project-derived state before storage.

When you only need a quick loader gate, use `is_encoded()` with the expected project magic:

```cpp
if (ripstop::codec::is_encoded(raw_file_bytes, project.magic)) {
    auto decoded = ripstop::codec::decode(raw_file_bytes, project);
}
```

### Decode Into Pre-Allocated Storage

Use `decode_into()` when the destination storage is already allocated:

```cpp
std::vector<std::byte> output(header.value.uncompressed_size);
auto err = ripstop::codec::decode_into(encoded_bytes, std::span{output}, project, asset);
```

If the asset is both compressed and scrambled, `decode_into()` uses a temporary heap buffer for the descrambled compressed payload before decompression. The destination buffer remains caller-owned.

### Text Decode Convenience

Use `decode_to_string()` when the payload is text and you want a `std::string` result directly:

```cpp
auto json = ripstop::codec::decode_to_string(encoded_bytes, project);
```

`decode_to_string()` wipes the transient internal byte buffer that RipStop owns before returning the resulting `std::string`.

### Secure Wipe Helpers

Use `SecureWipe(...)` when you want to erase caller-owned buffers after use:

```cpp
std::string password = "Secret";
ripstop::codec::SecureWipe(password);
```

Overloads exist for `std::string`, `std::vector<T>`, and `std::span<T>`.

### Zero-Copy Stream Parsing

When a downstream parser expects `std::istream`, include `<ripstop/MemStream.h>` and wrap the decoded bytes directly:

```cpp
#include <ripstop/MemStream.h>

auto decoded = ripstop::codec::decode(encoded_bytes, project, asset);
if (!decoded) {
    // handle decoded.error
}

ripstop::codec::MemStream stream(std::span{decoded.value});
```

This is a header-only utility in `ripstop::codec`. It is intentionally separate from `<ripstop/Codec.h>` so projects that do not need stream adapters do not pay for extra includes, while C++20 callers still get a zero-copy bridge to older `std::istream`-based code.

### File Helpers

Use `encode_file()` and `decode_file()` when you want a one-call path for disk I/O:

```cpp
auto err = ripstop::codec::encode_file("input.bin", "output.rip", project);
if (err != ripstop::codec::ErrorCode::Success) {
    // log ripstop::codec::to_string(err)
}
```

These helpers accept `std::filesystem::path`, which preserves native Unicode path handling on Windows and POSIX platforms.

`<ripstop/MemStream.h>` is installed as a public header, so decoded buffers can also be wrapped in `ripstop::codec::MemStream` when you need `std::istream`-style parsing over in-memory data.
