# RipStop Codec — C++ Asset Protection and Obfuscation Library

**Lightweight C++20 asset protection library for making game assets, application resources, and
proprietary binary files harder to identify and casually rip.**

RipStop wraps existing data in a project-specific binary envelope. It adds compression, deterministic
scrambling, contextual asset binding, and corruption checks without requiring a custom file format,
custom scrambler, config generator, or encryption stack.

RipStop provides:

- project-specific file identity
- Deflate compression
- deterministic built-in scrambling
- CRC corruption/context checks
- byte, typed, file, and `std::istream`-bridge APIs
- no callbacks, logging, retries, or process termination

RipStop provides asset obfuscation plus corruption and decode-context checks. It does not provide
encryption, cryptographic confidentiality, or protection against intentional forgery. Do not use it
for credentials, personal data, or cryptographic secrets.

## How do I protect assets from being ripped?

Common files such as JSON, textures, meshes, shaders, audio, and custom binary blobs can expose
recognizable headers or payload patterns. RipStop encodes those bytes into a project-specific wrapper
so basic file inspection, signature scanning, and automated extraction tools have less obvious data
to work with.

Typical uses include:

- protecting game assets such as maps, models, textures, shaders, audio, and dialogue
- hiding bundled resources, proprietary data files, presets, scripts, and internal caches
- replacing recognizable file signatures with project-specific identity
- binding encoded data to an expected project, asset class, or logical asset ID

This is practical C++ asset obfuscation and file hardening against casual ripping—not DRM or
cryptographic security. A determined reverse engineer who can inspect the running application can
recover the data.

## Why RipStop?

- **Plug and play:** provide one stable project seed, then call `encode()` and `decode()`
- **Format agnostic:** wrap any in-memory bytes without redesigning their original format
- **Small API:** byte, typed, file, and `std::istream`-bridge workflows
- **Deterministic by default:** stable output supports reproducible builds and efficient patches
- **Self-contained package:** installed consumers do not need to manage miniz, doctest, or CPM
- **Predictable failures:** structured error codes; no hidden logging, callbacks, retries, or exits

## Quick start

### 1. Add the library

```cmake
add_subdirectory(path/to/ripstop-codec)
target_link_libraries(my_app PRIVATE RipStopCodec::ripstop-codec)
```

Installed packages work too:

```cmake
find_package(RipStopCodec 1.1 CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE RipStopCodec::ripstop-codec)
```

### 2. Provide one project-unique seed

```cpp
#include <ripstop/Codec.h>

constexpr auto project =
    ripstop::codec::make_project_options("your-company:your-product:change-this");
```

Keep this seed stable after shipping. Changing it makes existing encoded assets unreadable.
No generated file, copied template, or setup tool is required.

### 3. Encode and decode

```cpp
std::vector<float> input{1.0f, 2.0f, 3.0f};

auto encoded = ripstop::codec::encode(std::span{input}, project);
if (!encoded) {
    std::cerr << ripstop::codec::to_string(encoded.error) << '\n';
    return;
}

auto decoded = ripstop::codec::decode_to_vector<float>(*encoded, project);
if (!decoded) {
    std::cerr << ripstop::codec::to_string(decoded.error) << '\n';
    return;
}
```

Compression and built-in scrambling are enabled by default. Most applications need no custom
scrambler, hook, policy class, config generator, or generated code. Full runnable example:
[`examples/basic.cpp`](examples/basic.cpp).

## Asset identity

Use `AssetOptions` when a project has multiple asset classes or logical asset IDs:

```cpp
const ripstop::codec::AssetOptions asset{
    .format_tag = ripstop::codec::utils::hash_string("mesh"),
    .context_seed = ripstop::codec::utils::hash_string("maps/forest_region"),
};

auto encoded = ripstop::codec::encode(bytes, project, asset);
auto decoded = ripstop::codec::decode(*encoded, project, asset);
```

`format_tag`, `context_seed`, and `password` are caller-supplied decode context; they are not stored
in the header. Wrong values fail decompression or CRC validation. The password is only another
non-cryptographic scramble input. Leave `nonce = 0` for reproducible builds; use a varying nonce
only when non-deterministic output is useful for anti-diffing.

## Error handling

All failures return `ErrorCode`. RipStop never logs, aborts, retries, or invokes hidden callbacks.

```cpp
if (result.error != ripstop::codec::ErrorCode::Success) {
    log(ripstop::codec::to_string(result.error));
}
```

`to_string()` returns stable readable names. Cast `ErrorCode` to its underlying integer when numeric
telemetry is preferred.

## Advanced custom scrambler

Most users should keep the built-in scrambler. A custom scrambler must:

- use a stable, project-owned, nonzero `scramble_id`
- be deterministic
- be self-inverse because the same function runs during encode and decode
- remain available for every asset written with that ID

See [`examples/custom_scrambler.cpp`](examples/custom_scrambler.cpp).

Legacy v1.0 custom scramblers using ID `0` remain supported. New custom integrations should use a
nonzero ID so assets identify their algorithm unambiguously.

## Important limits

- Format v1 uses native little-endian, native object representation for typed overloads.
- Typed overloads accept trivially copyable types. Their padding and layout remain compiler/ABI
  dependent; serialize explicitly for portable assets.
- Default maximum encoded or decoded payload is 256 MiB.
- `SecureWipe` accepts strings and trivially copyable live buffers. It is best-effort memory hygiene,
  not a guarantee against compiler/runtime copies.
- `MemStream` does not own its source buffer. Keep that buffer alive for the stream lifetime.

## More

- [installation and source integration](INSTALL.md)
- [wire format and threat model](docs/SPEC.md)
- [changelog](CHANGELOG.md)
- [third-party notices](NOTICE)
