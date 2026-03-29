# ripstop::codec Specification

## 1. Purpose

`ripstop::codec` is an asset hardening layer designed to raise the cost of automated analysis and bind data to project-specific contexts.

It wraps arbitrary byte buffers with:

- optional Deflate compression
- caller-selected scrambling
- CRC32 validation of the uncompressed payload

It is designed as a reusable codec layer that can sit in front of an existing binary format without changing that format's internal structure.

Standard encryption is intentionally avoided in favor of lightweight scrambling that callers can customize per project.

---

## 2. Design Goals

- Keep the API small and easy to integrate into existing save/load paths
- Avoid project-specific knowledge inside the codec itself
- Make protected payloads lose obvious structural signatures
- Allow callers to bind files to project identity, per-asset context, and asset class
- Detect decode mismatch and payload corruption cheaply

---

## 3. Threat Model

`ripstop::codec` is intended to raise the cost of:

- casual inspection in hex editors
- trivial parsing based on recognizable magic values or obvious float arrays
- reuse of one static XOR transform across multiple unrelated files

`ripstop::codec` is not intended to resist a determined reverse engineer with debugger access, live process inspection, or recovered integration secrets.

---

## 4. Processing Model

The protected path is:

1. compute CRC32 over the raw uncompressed input
2. optionally compress the raw input with the configured compression algorithm
3. derive a 64-bit scramble state from hashed project and asset inputs
4. optionally mix in a caller-provided nonce and hashed password
5. deepen that state with one SplitMix64 mixing round
6. optionally scramble the stored payload with the configured scrambler
7. populate the fixed 40-byte header with caller-owned magic and metadata
8. write the compression id into the header
9. optionally inject caller-selected junk padding bytes between the header and payload
10. XOR-mask header bytes from offset 4 onward with a project-derived mask

The decode path is:

1. parse and validate the header
2. verify `magic`, `codec_version`, `scramble_id`, and `domain_id`
3. descramble the stored payload with the caller-provided scrambler if needed
4. decompress if needed
5. compute CRC32 over the final uncompressed buffer
6. reject on mismatch by returning a structured error

CRC32 is used as a fast corruption and decode-mismatch check. It is not a cryptographic anti-tamper mechanism.

---

## 5. State Derivation Model

The codec derives scrambling state from caller-supplied project and asset inputs:

- `project_secret`: private application-owned secret
- `context_seed`: per-asset or per-context seed chosen by the caller
- `format_tag`: asset-class discriminator chosen by the caller
- `nonce`: optional advanced per-encode value for anti-diffing and non-deterministic output
- `password`: optional advanced per-call `string_view` lever for additional separation

State derivation is:

```text
state = hash_uint64(project_secret) ^ hash_uint64(context_seed) ^ hash_uint64(format_tag)
state ^= hash_uint64(nonce)
if password is not empty:
    state ^= hash_string(password)
state = mix64(state)
```

The default scrambler then performs a SplitMix64-style XOR pass over the full payload buffer when `scramble_id == 0`. Callers may replace that scrambler entirely by providing `ProjectOptions::scrambler`, but custom scramblers are an advanced integration path.

This gives the caller four independent levers:

- project ownership separation
- per-asset context binding
- keystream separation between different payload classes
- optional password-based separation

Recommended default policy:

- choose one `format_tag` per asset class
- derive `context_seed` from a stable logical asset identifier
- leave `nonce = 0`
- leave `password` empty
- use the built-in scrambler

Canonical `context_seed` policy example:

```text
context_seed = hash_string("textures/player_idle")
```

The codec does not enforce how `context_seed` or `format_tag` should be chosen, but using a stable logical asset identifier is the recommended baseline policy.

`format_tag` and `context_seed` are intentionally not stored in the header. The caller must provide the same values again during decode. If decode is attempted with the wrong tag or seed, the derived scramble state will not match the encoded payload and the final CRC check will fail.

---

## 6. Binary Format

The header is packed to 1-byte alignment and fixed at 40 bytes.

| Offset | Type | Field | Meaning |
| :--- | :--- | :--- | :--- |
| 0 | `uint32` | `magic` | Caller-defined file marker stored in plaintext for fast detection |
| 4 | `uint32` | `domain_id` | Caller-defined project/domain identifier, XOR-masked at rest |
| 8 | `uint16` | `codec_version` | Codec format version |
| 10 | `uint16` | `asset_version` | Caller-defined payload version |
| 12 | `uint8` | `identity_type` | Caller-defined identity kind |
| 13 | `uint8` | `scramble_id` | Caller-defined scrambler id. `0` is the built-in default scrambler |
| 14 | `uint16` | `flags` | Independent `HeaderFlags::Compressed` and `HeaderFlags::Scrambled` bits |
| 16 | `uint32` | `uncompressed_size` | Final raw payload size |
| 20 | `uint32` | `compressed_size` | Stored payload size |
| 24 | `uint32` | `masked_crc` | CRC32 of the raw payload XOR-masked with the low 32 bits of `project_secret` |
| 28 | `uint16` | `header_size` | Total header size in bytes. Parsers must use this to find the payload start |
| 30 | `uint8` | `compression_id` | Compression algorithm id. `0 = None`, `1 = Deflate` |
| 31 | `uint8` | `reserved` | Reserved, always `0` |
| 32 | `uint64` | `nonce` | Caller-supplied optional anti-diffing nonce mixed into scramble state |

The codec reserves `identity_type` as caller-owned metadata. The built-in `IdentityType` enum provides common low-range values, but callers may also store project-defined `uint8_t` values when they need custom identity categories. The exact enum surface may evolve, but its role remains the same: it describes how the integrating application interprets the protected payload's identity context.

Parsers must use `header_size` rather than `sizeof(Header)` to locate the payload. This allows both future header growth and caller-selected junk padding while preserving backward-compatible payload discovery.

For stealth, the on-disk header layout is fixed but bytes from offset 4 through 39 are XOR-masked with an 8-byte mask derived from `hash_uint64(project_secret ^ domain_id)`. Only the 4-byte `magic` field remains plaintext, so `is_encoded()` can reject mismatches quickly while other header metadata appears as high-entropy data until unmasked with project knowledge.

The stored CRC field is additionally masked as:

```text
stored_crc = raw_crc32 ^ low32(project_secret)
```

This prevents simple known-plaintext CRC guesses from matching obvious header bytes on disk.

## 7. Security Features

- Header Obfuscation: header bytes from offset 4 onward are XOR-masked with project-derived state, leaving only `magic` plaintext for quick detection.
- CRC Masking: `masked_crc` prevents straightforward known-plaintext CRC scans against the on-disk header.
- Contextual Binding: scramble state is derived from project secret, asset context, format tag, and optionally advanced inputs such as password and nonce.
- Offset Randomization: `header_size` can exceed `sizeof(Header)` so callers can push the payload start forward with junk padding.

---

## 8. Integration Surface

The library exposes:

- `ripstop::codec::ProjectOptions` and `ripstop::codec::AssetOptions`, which group the caller-controlled codec context
- `ripstop::codec::encode(...)`, which wraps a raw byte buffer into a caller-magic envelope and returns `Result<std::vector<std::uint8_t>>`
- `ripstop::codec::decode(...)`, which validates and unwraps a caller-magic envelope back into raw bytes and returns `Result<std::vector<std::uint8_t>>`
- `ripstop::codec::decode_to_string(...)`, which validates and unwraps a caller-magic envelope into `std::string`
- `ripstop::codec::encode_file(...)` and `ripstop::codec::decode_file(...)`, which provide high-level file I/O wrappers
- `ripstop::codec::peek_header(...)`, which validates and returns the unmasked header without decoding the payload
- `ripstop::codec::decode_into(...)`, which decodes into caller-owned storage and returns `ErrorCode`
- `ripstop::codec::decode_to_vector<T>(...)`, which decodes into a freshly allocated typed vector
- `ripstop::codec::to_string(ErrorCode)`, which maps error codes to stable string names in standard builds or hardened numeric strings when `RIPSTOP_HARDEN_ERRORS=1`
- `ripstop::codec::SecureWipe(...)`, which erases caller-owned sensitive buffers
- `ripstop::codec::utils::hash_string(...)` and `ripstop::codec::utils::hash_uint64(...)`, which provide deterministic caller-owned seed derivation and are `constexpr`

The simple path is intentionally supported:

- `encode(buffer, project)`
- `decode(buffer, project)`
- `decode_into(buffer, output, project)`

`AssetOptions` is optional in the public API. Callers only need to provide it when they want to set `format_tag`, `context_seed`, `padding_size`, identity metadata, or override the default `compress = true` / `scramble = true` behavior. `password` and `nonce` are advanced options.

The stable behavioral contract is:

- encode always writes a caller-magic envelope header
- encode always derives CRC32 from the raw input
- encode stores the CRC32 masked with the low 32 bits of `project_secret`
- encode can compress and scramble independently
- encode writes an explicit `compression_id` so decoders know how to interpret compressed payload bytes
- encode can optionally shift the payload start by adding caller-selected junk padding
- encode is deterministic by default when `nonce = 0` and the same inputs are provided
- encode still writes a valid caller-magic envelope when compression and scrambling are both disabled
- decode validates header fields, domain, payload shape, and CRC32
- decode reports malformed or mismatched data through `Result<T>::error` or `ErrorCode`

Decode failures are programmatically classified with `ripstop::codec::ErrorCode`, including:

- `BufferTooSmall`
- `MagicMismatch`
- `UnsupportedVersion`
- `UnsupportedScrambleId`
- `MissingScramblerFunc`
- `InvalidFlags`
- `InvalidIdentityType`
- `SizeLimitExceeded`
- `DomainMismatch`
- `CompressionFailed`
- `DecompressionFailed`
- `CrcMismatch`

---

## 9. Integration Responsibilities

The caller is responsible for:

- generating and protecting `project_secret`
- choosing `domain_id`
- deriving `context_seed`
- choosing `format_tag`
- choosing whether to use a custom `scramble_id` and `scrambler`
- keeping `password` policy consistent between encode and decode
- interpreting `identity_type`
- enforcing `asset_version` policy
- deciding how to respond to decode failures

Typical caller behavior on decode failure is to reject the asset and fall back to rebuild, regeneration, redownload, or cache invalidation logic. The exact recovery policy is outside the scope of `ripstop::codec`.

---

## 10. Packaging Boundary

The `ripstop::codec` package is intended to remain zero-knowledge:

- no project-specific headers
- no application secrets
- no asset-class-specific policy

Project-owned integration files such as secret configuration, seed derivation policy, and rebuild behavior should live outside this package.
