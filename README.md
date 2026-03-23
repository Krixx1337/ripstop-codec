# RipStop Codec (C++20)

**Tear-resistant, lightweight asset protection for modern games and apps.**

Named after the reinforced weaving technique used in parachutes, `RipStop Codec` is a lightweight C++20 library for wrapping arbitrary data in a project-owned binary envelope that makes casual asset ripping and signature-based tooling more expensive.

It gives you:

- project-specific file magic instead of obvious source signatures
- optional Deflate compression
- an explicit on-disk `compression_id` so the format can evolve safely
- optional scrambling tied to your project and asset context
- CRC validation on decode
- a small `std::span`-based API that fits into existing load/save paths
- an optional zero-copy `std::istream` bridge for decoded buffers in C++20

## Why RipStop

- **Engine-agnostic:** wrap textures, JSON, meshes, audio, or custom blobs without redesigning the payload format itself
- **Modern weave:** built around C++20 spans, deterministic helpers, and caller-owned policy
- **Signature camouflage:** choose your own `magic` so shipped files stop looking like the formats every extractor expects
- **Header stealth:** everything after the plaintext magic is masked at rest
- **Patch-friendly:** output stays deterministic by default when `nonce = 0`

RipStop intentionally does not ship with a baked-in default `ProjectOptions::magic`. You supply project-owned identity values instead of inheriting a shared global signature.

## Security Boundary

`RipStop Codec` is **not cryptographic encryption**. It is an obfuscation and integrity layer for hardening assets, caches, or internal data blobs against casual analysis and basic tooling. Do not use it as a substitute for AES or ChaCha20 when you need real confidentiality for secrets, credentials, or user data.

## Quick Start

### 1. Generate a project-local config

```bash
python path/to/ripstop-codec/tools/generate_key.py
```

If you prefer to check in a template and edit constants manually, start from [templates/RipStop_Config.example.h](./templates/RipStop_Config.example.h).

### 2. Encode assets into your private format

```cpp
#include <ripstop/Codec.h>
#include "RipStop_Config.h"

#include <iostream>
#include <vector>

std::vector<float> mesh_data = {1.0f, 2.0f, 3.0f};

const auto project = example::ripstop::MakeProjectOptions();

const auto asset = example::ripstop::MakeAssetOptions(
    example::ripstop::tagPrimaryAsset,
    example::ripstop::HashContextString("meshes/player"));

auto encoded = ripstop::codec::encode(std::span{mesh_data}, project, asset);

if (!encoded) {
    std::cerr << ripstop::codec::to_string(encoded.error) << '\n';
    return;
}
```

### 3. Decode into your own memory

```cpp
std::vector<float> out_data(mesh_data.size());

const auto err = ripstop::codec::decode_into(*encoded, std::span{out_data}, project, asset);

if (err != ripstop::codec::ErrorCode::Success) {
    std::cerr << ripstop::codec::to_string(err) << '\n';
    return;
}
```

## Recommended Default Profile

For most projects, start with:

- one `format_tag` per asset class
- one stable `context_seed` per logical asset id
- `compress = true`
- `scramble = true`
- `nonce = 0` for deterministic builds
- no `password` unless you have a specific compatibility policy for it

```cpp
ripstop::codec::AssetOptions asset{
    .format_tag = 0x11223344u,
    .context_seed = ripstop::codec::utils::hash_string("textures/player_idle"),
    .compress = true,
    .scramble = true,
};
```

`format_tag` and `context_seed` are not stored in the file header. The caller provides them again during decode. If the caller supplies the wrong tag or seed, decode will fail its CRC check.

## Useful Patterns

### Mixed raw and wrapped loader path

```cpp
if (ripstop::codec::is_encoded(file_bytes, project.magic)) {
    auto decoded = ripstop::codec::decode(file_bytes, project);
} else {
    // Load the raw file normally.
}
```

### Decode text directly

```cpp
auto json = ripstop::codec::decode_to_string(file_bytes, project);
```

### Zero-copy parsing with `std::istream`

```cpp
#include <ripstop/MemStream.h>

auto decoded = ripstop::codec::decode(file_bytes, project);
if (!decoded) {
    return;
}

ripstop::codec::MemStream stream(std::span{decoded.value});
```

`<ripstop/MemStream.h>` stays separate from `<ripstop/Codec.h>` so the core codec API remains lean, but C++20 callers can still pass decoded in-memory data to existing `std::istream`-based loaders without copying it into a `std::stringstream`.

### Add anti-diffing or offset noise when needed

```cpp
ripstop::codec::AssetOptions asset{
    .format_tag = 0x11223344u,
    .context_seed = 0x55667788u,
    .nonce = 0x1020304050607080ull,
    .padding_size = 24,
};
```

### Pro-tip: camouflage pattern

To make protected assets harder to classify, set `ProjectOptions::magic` to mimic a familiar format like PNG: `0x474E5089`. RipStop still stores your private payload and metadata, but automated rippers now see a decoy signature first.

### One-call disk helpers

```cpp
auto err = ripstop::codec::encode_file("input.bin", "output.rip", project);
```

## Integration

For CMake, treat this repository as the source of truth:

```cmake
add_subdirectory(path/to/ripstop-codec)
target_link_libraries(MyProject PRIVATE ripstop-codec)
```

Or link the alias target:

```cmake
target_link_libraries(MyProject PRIVATE RipStopCodec::ripstop-codec)
```

For manual integration without CMake, add `include/` and `third_party/miniz/` to your include paths, then compile `src/RipStop.cpp` and `third_party/miniz/miniz.c` as part of your project. The full setup is documented in [INSTALL.md](./INSTALL.md).

## More

- integration details: [INSTALL.md](./INSTALL.md)
- binary format and threat model: [docs/SPEC.md](./docs/SPEC.md)
- project config template: [templates/RipStop_Config.example.h](./templates/RipStop_Config.example.h)
- licensing and bundled third-party notices: [LICENSE](./LICENSE), [NOTICE](./NOTICE)
