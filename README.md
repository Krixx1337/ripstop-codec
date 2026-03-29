# RipStop Codec (C++20)

**A lightweight C++20 library for protecting game assets, application resources, and proprietary data from casual ripping.**

`RipStop Codec` is a modern C++20 library for wrapping arbitrary data in a project-owned binary envelope that makes casual asset ripping, signature-based tooling, and basic file inspection more expensive. It is designed for games, desktop apps, tools, and other software that need a practical way to harden packaged resources, internal blobs, or proprietary files without redesigning the payload format itself.

Looking to securely download RipStop assets over hostile networks? Check out [BurnerNet](https://github.com/Krixx1337/burner-net).

If you are searching for how to protect assets from being stolen, how to hide application resources in C++, or how to make binary files harder to identify and extract, this library is built for that use case.

It gives you:

- project-specific file magic instead of obvious source signatures
- optional Deflate compression
- an explicit on-disk `compression_id` so the format can evolve safely
- optional scrambling tied to your project and asset context
- CRC validation on decode
- optional hardened numeric error output with project-specific `ErrorCode` values
- public `SecureWipe(...)` helpers for caller-owned sensitive buffers
- a small `std::span`-based API that fits into existing load/save paths
- an optional zero-copy `std::istream` bridge for decoded buffers in C++20

## How Do I Protect Assets From Being Ripped?

Many developers ask questions like:

- how do I protect game assets from extraction?
- how do I hide proprietary application files?
- how can I make JSON, textures, models, or binary resources harder to rip?

Standard formats such as PNG, JSON, MP3, or custom binary blobs are easy for tools to classify because they expose recognizable headers and metadata. `RipStop Codec` solves that by moving data from a public, easy-to-identify layout into a private project-specific wrapper.

It raises the cost of analysis with:

- **Signature camouflage:** choose your own `magic` so shipped files stop looking like the formats every extractor expects
- **Metadata masking:** everything after the plaintext magic is masked at rest
- **Contextual scrambling:** bind data to your project and asset context
- **Integrity validation:** CRC checks help reject corrupted or mismatched decodes
- **Compression support:** reduce size while keeping the wrapped format private

## Use Cases

RipStop is engine-agnostic and can protect any file format your application already uses:

- **Games:** textures, shaders, meshes, audio, dialogue, save-adjacent resources, and level data
- **Desktop applications:** proprietary configuration, packaged content, internal databases, and bundled scripts
- **Creative software:** presets, templates, brushes, sample packs, and other intellectual property
- **Embedded or edge systems:** lightweight binary payload protection where full encryption is not the goal

## Why RipStop

- **Engine-agnostic:** wrap textures, JSON, meshes, audio, or custom blobs without redesigning the payload format itself
- **Modern weave:** built around C++20 spans, deterministic helpers, and caller-owned policy
- **Signature camouflage:** choose your own `magic` so shipped files stop looking like the formats every extractor expects
- **Header stealth:** everything after the plaintext magic is masked at rest
- **Patch-friendly:** output stays deterministic by default when `nonce = 0`
- **Lean integration:** fits existing pipelines without imposing a heavyweight packaging system

RipStop intentionally does not ship with a baked-in default `ProjectOptions::magic`. You supply project-owned identity values instead of inheriting a shared global signature.

## Security Boundary

`RipStop Codec` is **not cryptographic encryption**. It is an obfuscation and integrity layer for hardening assets, caches, application resources, or internal data blobs against casual analysis and basic tooling. Do not use it as a substitute for AES or ChaCha20 when you need real confidentiality for secrets, credentials, or user data.

Its purpose is practical asset protection and file hardening, not strong secrecy against a determined reverse engineer.

## Quick Start

### 1. Generate a project-local config

```bash
python path/to/ripstop-codec/tools/generate_key.py
```

The generated header now includes:
- project-owned `magic`, `domain_id`, tags, and secret material
- `RIPSTOP_ERROR_XOR`
- randomized `ErrorCode` values for hardened builds
- helper functions for `MakeProjectOptions()` and `MakeAssetOptions()`

If you prefer to check in a template and edit constants manually, start from [templates/RipStop_Config.example.h](./templates/RipStop_Config.example.h).

### 2. Encode assets or proprietary data into your private format

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

### Wipe sensitive caller-owned buffers

```cpp
ripstop::codec::SecureWipe(out_data);
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

### How to add anti-diffing or offset noise

```cpp
ripstop::codec::AssetOptions asset{
    .format_tag = 0x11223344u,
    .context_seed = 0x55667788u,
    .nonce = 0x1020304050607080ull,
    .padding_size = 24,
};
```

### How to use signature camouflage

To make protected assets harder to classify, set `ProjectOptions::magic` to mimic a familiar format like PNG: `0x474E5089`. RipStop still stores your private payload and metadata, but automated rippers now see a decoy signature first.

### How to reduce asset swapping mistakes or tampering

Because `format_tag` and `context_seed` participate in decode, mismatched asset identity inputs can cause CRC validation to fail. That gives you a lightweight way to bind decoding to the expected asset context.

### One-call disk helpers

```cpp
auto err = ripstop::codec::encode_file("input.bin", "output.rip", project);
```

The file helpers accept `std::filesystem::path`, so UTF-8 and native wide-character paths work correctly across supported platforms.

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

For manual integration without CMake, add `include/` and `third_party/` to your include paths, then compile `src/RipStop.cpp` and `third_party/miniz/miniz.c` as part of your project. The full setup is documented in [INSTALL.md](./INSTALL.md).

## More

- integration details: [INSTALL.md](./INSTALL.md)
- binary format and threat model: [docs/SPEC.md](./docs/SPEC.md)
- project config template: [templates/RipStop_Config.example.h](./templates/RipStop_Config.example.h)
- licensing and bundled third-party notices: [LICENSE](./LICENSE), [NOTICE](./NOTICE)
