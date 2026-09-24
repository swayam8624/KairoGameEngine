# KAIRO Build Hygiene Policy

The KAIRO integration build has a **zero-warning budget**.

A successful build must satisfy all three conditions:

1. configure succeeds;
2. compilation/linking succeeds;
3. the configure/build log contains no compiler, linker, or CMake warning diagnostics.

`scripts/build_and_test.sh` enforces that order. CTest is started only after the
zero-warning gate passes.

## What is fixed versus suppressed

KAIRO-owned warnings are fixed at source whenever they represent a real issue:
incomplete Vulkan structures, deleted comparison operators, unhandled enum
cases, ignored `[[nodiscard]]` values, unused variables, and incomplete runtime
aggregates are examples.

Suppressions are allowed only at a narrow boundary when the warning describes
intentional or third-party behavior:

- Catch2 v3.5.4 uses `__COUNTER__`; upstream Clang 23 diagnoses that macro as a
  future-language extension under KAIRO's pedantic test flags. The exception is
  attached only to Catch2-consuming test executables on upstream Clang 23+.
- `PropertyMetadata` intentionally appends defaulted fields so existing
  reflection aggregate initializers remain source-compatible. The corresponding
  aggregate-default diagnostics are disabled only for the registration source
  and tests that intentionally exercise partial metadata.
- Dear ImGui's pinned Metal backend uses a storage-mode spelling deprecated by
  the macOS 27 SDK. That diagnostic is disabled only for the third-party source.
- ImGuizmo and generated GLAD warnings are isolated to their third-party targets.

No KAIRO production target receives a blanket `-w` or global warning
suppression.

## Darwin toolchain compatibility

The integration toolchain uses upstream Homebrew Clang for C/C++/Objective-C++.
On current macOS, Debug output is pinned to strict DWARF 4 because the Apple
linker bundled with Xcode otherwise emits a large volume of debug-info parser
warnings for upstream Clang 23 objects.

Upstream Clang 23 also emits Objective-C selector stubs that expect matching
linker synthesis. On Apple hosts using upstream Clang 23+, KAIRO's Objective-C++
Metal translation units disable those selector stubs; AppleClang and older Clang
are unaffected.

These are compatibility settings, not optimizations and not warning hiding.
