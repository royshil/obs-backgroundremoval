---
# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0
kind: Design principle
platforms: All
---

# SP-00009: A C module over a C++ static library

## Principle

The plugin binary is built only from C, and all of the plugin's C++ is in a static library that the
binary links. The two meet at a few functions with C linkage. Because the binary
itself has no C++, the symbols it exports are exactly the entry points that SP-00008 lists, and on
Linux the linker keeps every symbol that comes from the static library local.

## Rationale

C has no templates and no implicitly instantiated functions, so a C file exports what its code
marks for export and nothing else. C++ code instantiates templates of the standard library as it
uses them, and libstdc++ declares namespace `std` with default visibility, so on Linux those
instantiations are exported even when the plugin is compiled with `-fvisibility=hidden`. Every use
of `std::unordered_set`, `std::from_chars` or `std::optional` can add another exported symbol.

Keeping all C++ in a static library turns this into a property of the build layout. The linker can
make every symbol from static libraries local with one option, `--exclude-libs,ALL`, without a list
of names to keep in step with the code. The entry points stay exported because they are defined in
the module's own C object, not in the library. The same static library is what the tests link, so
they run the code that ships.

## Requirements

1. The CMake target `${CMAKE_PROJECT_NAME}`, a `MODULE` library, is the plugin binary. Its only
   source is `src/plugin-main.c`. No C++ source is added to it.
2. All C++ of the plugin is in the `STATIC` library `${CMAKE_PROJECT_NAME}_pluginlibs`, which the
   module links privately. The library passes its compile and link settings on to the module
   through its public usage requirements, so both are compiled the same way.
3. The module and the library meet at an interface with C linkage, and nowhere else:
    - `src/plugin-main.c` declares, and the library defines with `extern "C"`, the functions
      `pluginloader_load`, `pluginloader_post_load`, `pluginloader_unload`,
      `pluginloader_get_name`, `pluginloader_get_description` and `pluginloader_get_author`.
    - The library calls `obs_current_module`, through the `obs_module_file()` and
      `obs_module_config_path()` macros of `obs-module.h`, and `obs_module_text`, which
      `src/plugin-main.c` defines.

4. A static library contributes only the objects that the module references. The code that the
   plugin runs is reached from the functions of Requirement 3, starting with `pluginloader_load`,
   which registers the filter.
5. On Linux, the module is linked with `--exclude-libs,ALL`, so that every symbol from the static
   library, standard library instantiations among them, stays local to the module. On macOS and
   Windows, hidden visibility and `__declspec(dllexport)` already limit the exports to the entry
   points.
6. Tests link the static library, or compile its sources themselves, and tests of the plugin as a
   whole load the built module.

## Findings

Checked on 2026-11-26.

### Exports before `--exclude-libs`

With the module built from `src/plugin-main.c` and the C++ in the static library, the full plugin
build exported:

| Platform                                           | Exports                                          |
| -------------------------------------------------- | ------------------------------------------------ |
| macOS, universal (Apple clang)                     | The 11 entry points                              |
| Windows x64 (MSVC 19.51)                           | The 11 entry points                              |
| Debian forky, Ubuntu resolute and Arch Linux (GCC) | The 11 entry points, and 12 symbols of libstdc++ |

The 12 symbols were six members of `std::_Hashtable` for strings, used by an
`std::unordered_set<std::string>`, three helpers of `std::from_chars` and `std::to_chars`, and the
`typeinfo`, its name and the `vtable` of `std::bad_optional_access`.

### Ways to hide them

A static library with the same uses of `std::unordered_set<std::string>`, `std::from_chars`,
`std::to_chars` and `std::optional::value()`, linked by GCC 16.2.0 (Debian forky) into a shared
library with a C entry point:

| Link                                            | Exports                                   | Loads and runs |
| ----------------------------------------------- | ----------------------------------------- | -------------- |
| As before                                       | The entry point and the libstdc++ symbols | Yes            |
| `-Wl,--exclude-libs,ALL`                        | The entry point only                      | Yes            |
| `-Wl,--version-script` exporting `obs_module_*` | The entry point only                      | Yes            |

A version script works without the layout of Requirement 1, but it repeats the list of entry points
that SP-00008 already fixes. `-Wl,-Bsymbolic` binds references inside the module without hiding
anything. Overriding libstdc++'s `_GLIBCXX_VISIBILITY` relies on an internal macro, and avoiding the
templates in the code would have to be repeated for every new use of the standard library.

### Exports with `--exclude-libs`

The full plugin build with `--exclude-libs,ALL` exported exactly the 11 entry points and passed all
40 tests other than the performance tests, including the tests that load the built module into
libobs and run the filter, on each of these:

| Distribution    | Compiler   | libobs | Package      |
| --------------- | ---------- | ------ | ------------ |
| Debian forky    | GCC 16.2.0 | 32.2.2 | `.deb` built |
| Ubuntu resolute | GCC 15.2.0 | 32.1.0 | `.deb` built |
| Arch Linux      | GCC 16.2.1 | 33.0.3 | Not built    |

## Verification

- The exported symbols of the built module are exactly the 11 entry points of SP-00008 on every
  platform.
- The tests that load the built module into libobs pass on every platform.
