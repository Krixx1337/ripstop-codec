# Compatibility Policy

RipStop follows semantic versioning.

- Patch releases fix defects without intentional source or format changes.
- Minor 1.x releases may add API but should preserve existing source integrations and format-v1
  assets.
- Major releases may remove or reshape public API or introduce an incompatible wire format.

## Stable 1.x contracts

- 40-byte format-v1 header and current encoding/decoding derivation
- existing `ErrorCode` numeric values
- `ProjectOptions`, `AssetOptions`, byte APIs, typed APIs, file helpers, and `MemStream`
- deterministic output for identical inputs with `nonce = 0` and `padding_size = 0`
- installed target `RipStopCodec::ripstop-codec`

Typed payloads use native C++ object representation. They are compatible only when producer and
consumer agree on endianness, type layout, and ABI. Use explicit serialization for portable assets.

For v1.0 compatibility, a supplied custom scrambler still overrides the built-in scrambler even
when `scramble_id == 0`. New custom integrations should use a stable nonzero ID so encoded assets
identify their algorithm unambiguously.

## 1.1 intentional cleanup

`SecurityPolicy`, config-header injection, and hardened numeric error strings were added after the
1.0.1 tag and removed before broad adoption. They did not affect format-v1 bytes. Applications using
that unreleased main-branch surface must remove `.policy` and handle returned `ErrorCode` values
directly.
