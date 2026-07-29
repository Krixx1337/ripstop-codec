# Changelog

## 1.1.0

- Removed unused `SecurityPolicy` callbacks and config-header injection.
- Removed polymorphic/numeric error-string hardening; error names are always readable.
- Kept the format-v1 40-byte header and existing asset compatibility.
- Made built-in scrambling the documented plug-and-play default.
- Added one-line deterministic `make_project_options(seed)` setup and removed the Python config generator.
- Allowed typed encoding of all trivially copyable values.
- Added stricter header validation and transactional file replacement.
- Preserved existing POSIX destination permissions during transactional replacement.
- Fixed `MemStream` initialization and constrained secure wiping to live safe buffers.
- Added installable CMake package metadata, isolated test and fuzz instrumentation, examples, and adoption docs.

## 1.0.1

- Previous stable release.
