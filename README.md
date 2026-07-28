# RipStop Codec

Small C++20 static library for wrapping assets in a project-specific binary envelope.

RipStop provides:

- project-specific file identity
- Deflate compression
- deterministic built-in scrambling
- CRC corruption/context checks
- byte, typed, file, and `std::istream`-bridge APIs
- no callbacks, logging, retries, or process termination

RipStop is obfuscation and integrity protection, not encryption. Do not use it for credentials,
personal data, or secrets requiring cryptographic confidentiality.

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
No generated file or setup tool is required. For a reusable central config header, optionally copy
[`templates/RipStop_Config.example.h`](templates/RipStop_Config.example.h) and change only
`kProjectSeed`.

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
    .context_seed = ripstop::codec::utils::hash_string("maps/queensdale"),
};

auto encoded = ripstop::codec::encode(bytes, project, asset);
auto decoded = ripstop::codec::decode(*encoded, project, asset);
```

`format_tag`, `context_seed`, and `password` are caller-supplied decode context; they are not stored
in the header. Wrong values fail decompression or CRC validation. Leave `nonce = 0` for reproducible
builds.

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
