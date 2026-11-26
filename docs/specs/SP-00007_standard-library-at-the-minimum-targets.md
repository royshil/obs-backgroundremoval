---
# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0
---

# SP-00007: Standard library features available at the minimum targets

- Kind: Design principle
- Platforms: All

## Principle

The plugin uses a C++ standard library feature only when every supported toolchain provides it and,
where the library ships with the operating system, the minimum supported OS provides it at run time.
A feature that one platform lacks gets a fallback on that platform only.

## Rationale

The plugin is built with a different standard library on each platform: libc++ from Xcode on macOS,
libstdc++ from the distribution on Linux and the MSVC STL on Windows. Supporting C++20 on paper does
not mean that every library implements a feature, and on macOS a feature that the SDK implements in
`libc++.dylib` is available only on the macOS releases whose system libc++ has it. The SDK marks
those features with availability attributes, so building with the minimum deployment target turns
a use that the oldest supported macOS cannot run into a compile error.

## Requirements

1. macOS builds set `CMAKE_OSX_DEPLOYMENT_TARGET` to the minimum supported macOS, 13.0, so that the
    SDK rejects any libc++ feature newer than it.
2. Before relying on a standard library feature, check it on each platform against the table in
    Findings, and add the result there.
3. A feature that some supported toolchain lacks is not used when an older feature that every
    toolchain has does the job. Only when none does is it replaced where it is missing, behind a
    feature-test macro from `<version>` rather than a platform check.
4. Numbers in text are parsed with the integer overloads of `std::from_chars`. The floating-point
    overloads are not used.
5. Background threads are `std::thread` objects that their owner joins explicitly, including in its
    destructor, rather than `std::jthread`.

## Findings

### `std::from_chars`

Checked on 2026-11-25, when `src/UpdateConfig/UpdateConfigSemver.cpp` replaced `std::regex` with
`std::from_chars`.

| Standard library | Integer overloads | Floating-point overloads |
| --- | --- | --- |
| libc++ (macOS 27.2 SDK, Xcode 27.2) | Header-only, no availability attribute | `_LIBCPP_AVAILABILITY_FROM_CHARS_FLOATING_POINT`, introduced in LLVM 20, so not available at macOS 13.0 |
| libc++ 16.0.6 (macOS 14.2 SDK, Xcode 15.2) | Header-only (`inline _LIBCPP_HIDE_FROM_ABI` in `<charconv>`), no availability attribute | Not implemented |
| libstdc++ | Since GCC 8, header-only | Since GCC 11, in `libstdc++.so` |
| MSVC STL | Since Visual Studio 2017 15.7, header-only | Since Visual Studio 2017 15.8 |

The integer overloads are templates in the headers on all three, so a plugin that builds also runs:
it imports nothing from the system library for them. Verified as follows:

- The universal macOS plugin built with Xcode 27.2 and deployment target 13.0 has `minos 13.0` in
    both slices and imports no `from_chars`, `to_chars` or `itoa` symbol.
- On macOS 27.2.1 (26B131) on arm64, with that build, all the tests passed, `UpdateConfigSemverTest`
    among them.
- `UpdateConfigSemverTest`, built with Xcode 27.2 for macOS 13.0 without sanitizers, passed on macOS
    13.6 (22G120) on arm64.
- On macOS 13.6 with Xcode 15.2 (Apple clang 15.0.0), `UpdateConfigSemverTest` built for arm64 and
    x86_64 with deployment target 13.0, with and without `-fsanitize=undefined`, imported no
    `from_chars`, `to_chars` or `itoa` symbol, and passed on arm64.
- Not checked: the x86_64 slice at run time, and the SDK of Xcode 26.5, which CI builds with.
- The compilers in CI on 2026-11-24 are well past the versions above: AppleClang 21.0.0 (Xcode
    26.5), GCC 16.2.0 (Debian forky), GCC 15.2.0 (Ubuntu resolute), GCC 16.2.1 (Arch Linux) and
    MSVC 19.52.

### `std::jthread`

Checked on 2026-11-25. Until then, `src/UpdateConfig/UpdateConfigCurl.cpp` used `std::jthread` on
Linux and Windows and fell back to `josuttis::jthread` from `vendor/josuttis-jthread` on macOS.

| Standard library | `std::jthread` and `<stop_token>` |
| --- | --- |
| libc++ 22.1 (macOS 27.2 SDK, Xcode 27.2) | Available. A program built for macOS 13.0 imports only `std::thread`, `sleep_for` and the atomic wait functions from the system libc++, and it ran correctly on macOS 13.6 (22G120) on arm64 |
| libc++ 16.0.6 (macOS 14.2 SDK, Xcode 15.2) | Missing: there is no `<stop_token>`, and `std::jthread` does not compile |
| libc++ of Xcode 26.5, which CI uses | Not checked |

Whether `std::jthread` exists depends on the toolchain, not on the macOS release. The plugin only
used it to join the fetch thread when the thread object is destroyed, never `std::stop_token`, so it
was replaced with `std::thread` and explicit joins, which every toolchain has, and
`vendor/josuttis-jthread` was removed.

## Verification

- The macOS builds in CI compile with deployment target 13.0.
- `UpdateConfigSemverTest` covers the parsing with `std::from_chars`.
