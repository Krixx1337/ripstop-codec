# Contributing

Requirements:

- preserve format-v1 decoding unless a major release explicitly changes it
- avoid public API breaks in 1.x
- keep errors explicit; no logging, callbacks, retries, or process termination
- keep output deterministic when inputs and nonce are unchanged
- add focused regression tests for parser, format, or compatibility changes

Before submitting:

```powershell
cmake --preset x64-debug
cmake --build --preset x64-debug
ctest --preset x64-debug
```

Also build one Release preset and the examples when changing public API or documentation.

Clang/libFuzzer smoke:

```bash
cmake -S . -B fuzz -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DRIPSTOP_BUILD_TESTS=OFF -DRIPSTOP_BUILD_FUZZER=ON
cmake --build fuzz --target ripstop-codec-fuzz-decode
./fuzz/ripstop-codec-fuzz-decode -runs=1000
```
