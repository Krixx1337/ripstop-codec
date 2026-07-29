---
name: update-deps
description: Update RipStop Codec's CPM bootstrap, miniz, and doctest dependencies when the user says "update deps", "update dependencies", "check dependency updates", or asks to bump third-party libraries. Use stable release tags only, keep CPM hash-verified without vendoring it, preserve format-v1 compatibility, and validate CMake consumers.
---

# Update Dependencies

## Sources of Truth

- Dependency declarations and CPM bootstrap: `cmake/RipStopDependencies.cmake`
- Third-party versions and terms: `NOTICE`
- User-visible dependency changes: `CHANGELOG.md`
- Library version: `CMakeLists.txt`

## Workflow

1. Inspect the worktree:

   ```powershell
   git status --short -uall
   ```

   Preserve unrelated user changes.

2. Read current dependency declarations and identify:

   - `RIPSTOP_CPM_VERSION` and `RIPSTOP_CPM_SHA256`
   - miniz `VERSION` and `GIT_TAG`
   - doctest `VERSION` and `GIT_TAG`

3. Query official upstream release tags:

   - CPM.cmake: `cpm-cmake/CPM.cmake`
   - miniz: `richgel999/miniz`
   - doctest: `doctest/doctest`

   Select latest stable release only. Ignore prereleases unless user explicitly requests one.

4. If CPM has a newer stable release:

   - Download exact `CPM.cmake` release asset to a temporary or build directory.
   - Compute its SHA-256.
   - Update `RIPSTOP_CPM_VERSION` and `RIPSTOP_CPM_SHA256`.
   - Do not add `CPM.cmake` to repository.
   - Never use `releases/latest` without a pinned version and expected hash.

5. If miniz or doctest has a newer stable release:

   - Update matching `VERSION`.
   - Update matching `GIT_TAG` to release tag.
   - Preserve upstream tag style, such as `3.1.2` or `v2.5.3`.
   - Never use commit hashes, branches, or floating tags for `GIT_TAG`.

6. Preserve dependency integration:

   - miniz remains `DOWNLOAD_ONLY`, internal, and compiled from `miniz.c`, `miniz_tdef.c`, and
     `miniz_tinfl.c`.
   - Inspect new miniz tag for required source/header layout before building.
   - doctest remains test-only with its own tests, static main, and install rules disabled.
   - Installed `RipStopCodec` target must not reference CPM, miniz, or doctest.
   - Parent-provided `CPMAddPackage` must bypass RipStop's CPM bootstrap.

7. Update documentation:

   - Update dependency version and release-tag fields in `NOTICE`.
   - Update existing dependency-version bullet in `CHANGELOG.md`, or add one under current top release
     section when missing.
   - Do not bump RipStop's project version unless user explicitly requests it.

8. If everything is already current, make no edits. Report checked versions and stop after
   non-mutating checks.

## Validation

When dependencies changed:

1. Configure from a fresh build directory and source cache so new tags are resolved.
2. Build and run existing tests on Windows/MSVC.
3. Require format-v1 golden test to remain byte-for-byte identical. Never regenerate fixture to
   accept changed output.
4. Install RipStop and build/run `tests/package_smoke`.
5. Build/run `tests/subdirectory_smoke` with RipStop tests disabled; confirm doctest was not fetched.
6. Configure a fresh build directory using populated `CPM_SOURCE_CACHE` with
   `FETCHCONTENT_FULLY_DISCONNECTED=ON`.
7. Build and test on Linux/GCC through WSL when available.
8. Validate Clang/libFuzzer target when available; otherwise check sanitizer isolation structurally.
9. Confirm installed CMake exports contain no `CPM`, `miniz`, or `doctest` references.
10. Run:

    ```powershell
    rg -n "third_party|Pinned commit" . -g "!out/**" -g "!.vs/**"
    git diff --check
    git status --short -uall
    ```

    First `rg` command should return no matches.

## Constraints

- Keep dependency pins as stable Git tags only.
- Keep CPM download SHA-256 verification; it is asset integrity, not a Git revision pin.
- Do not vendor CPM or dependency source files.
- Do not introduce an updater script, package lock, CI, or new tests.
- Do not change public C++ API, codec format, default scrambler, or golden fixture.
- Do not move or recreate existing Git tags.

## Report

State:

- Each checked dependency with old and current/new tag.
- CPM old/new version and whether SHA-256 changed.
- Validation commands that passed.
- Any unavailable validation, such as missing Clang/libFuzzer.
