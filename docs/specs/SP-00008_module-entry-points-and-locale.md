---
# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0
kind: Design principle
platforms: All
---

# SP-00008: Module entry points and locale strings

## Principle

The plugin module exports exactly the functions that libobs looks up, and nothing else. Its entry
points are written out in `src/plugin-main.c` against the libobs module ABI, without
`obs-module.h`. Its locale strings are read from `data/locale/*.ini` with libobs's own
`text_lookup`, as `OBS_MODULE_USE_DEFAULT_LOCALE()` does.

## Rationale

libobs finds a module's functions by name with `os_dlsym`, so the symbols the module exports are its
whole interface to libobs. `obs-module.h` does not describe that interface consistently:

- Of the optional information functions, `obs_module_name` and `obs_module_description` are declared
  with `MODULE_EXPORT`, but `obs_module_author` is declared with it only inside the
  `OBS_MODULE_AUTHOR()` macro. A module that defines `obs_module_author` by hand under hidden
  visibility does not export it, and libobs never sees it.
- `OBS_DECLARE_MODULE()` and `OBS_MODULE_USE_DEFAULT_LOCALE()` define `obs_current_module`,
  `obs_module_text` and the variable `obs_module_lookup` with no visibility of their own. The last
  has no declaration at all. Whether they leak out of the module depends on the build passing
  `-fvisibility=hidden`, not on the header.
- The header pulls in all of `obs.h` to declare a handful of functions.

The ABI itself is very stable, so writing it out costs little. The names libobs looks up, the
macro bodies and every prototype the plugin needs are the same in the libobs headers vendored from
August 2024 (version 30.2.3) and in OBS Studio 33.0.3.

For locale strings, the plugin keeps the mechanism that OBS Studio and its translators already
use: one `.ini` file for each locale, read at run time by libobs's parser in `util/text-lookup.c`.
Compiling the strings into the module was tried and dropped, because it added a generator and
tables to keep in step without making the strings any more correct. The parser has rules of its
own, which the `.ini` files have to follow.

## Requirements

### Exports

1. The module exports exactly these 11 functions, the names that `load_module_exports()` in
   libobs's `obs-module.c` resolves:

    | Function                 | libobs treats it as | Role                                                                                |
    | ------------------------ | ------------------- | ----------------------------------------------------------------------------------- |
    | `obs_module_load`        | Required            | Initializes the plugin and registers its sources. Returning `false` fails the load. |
    | `obs_module_set_pointer` | Required            | Receives the module's `obs_module_t *` right after the library is opened.           |
    | `obs_module_ver`         | Required            | Returns the libobs API version that the module targets.                             |
    | `obs_module_unload`      | Optional            | Called at shutdown, only if `obs_module_load` succeeded.                            |
    | `obs_module_post_load`   | Optional            | Called after every module has loaded.                                               |
    | `obs_module_set_locale`  | Optional            | Called with the UI locale right after `obs_module_set_pointer`.                     |
    | `obs_module_free_locale` | Optional            | Called at shutdown, before `obs_module_unload`.                                     |
    | `obs_module_get_string`  | Optional            | Looks up one of the module's strings for libobs.                                    |
    | `obs_module_name`        | Optional            | The display name.                                                                   |
    | `obs_module_description` | Optional            | A description of the plugin.                                                        |
    | `obs_module_author`      | Optional            | The author.                                                                         |

2. Each of them is defined with `PLUGIN_PUBLIC`, which is `__declspec(dllexport)` with MSVC and
   `__attribute__((visibility("default")))` with other compilers.
3. A function that other files of the module call, but that libobs does not look up, is defined
   with `PLUGIN_PRIVATE`, which is `__attribute__((visibility("hidden")))` with GCC and Clang and
   nothing with MSVC. These are `obs_current_module`, which the `obs_module_file()` and
   `obs_module_config_path()` macros of the C++ sources call, and `obs_module_text`.
4. Anything used only in its own file is `static` and carries no visibility attribute. GCC reports
   `'visibility' attribute ignored` for a `static` declaration, which `-Werror` turns into a build
   failure.
5. An `extern` declaration carries no visibility attribute. The definition decides whether a symbol
   is exported.
6. `src/plugin-main.c` does not include `obs-module.h`. It declares the few libobs items it needs
   itself: the incomplete types `obs_module_t` and `lookup_t`, `blog`, `obs_module_load_locale`,
   `text_lookup_getstr` and `text_lookup_destroy`.
7. `obs_module_ver` returns a fixed version, `(30 << 24) | (2 << 16) | 3` for 30.2.3, rather than
   the `LIBOBS_API_VER` of whichever headers the build uses. libobs 33 compares only the major and
   minor versions, and refuses a module that targets a newer libobs than itself, so the value
   states the oldest libobs that the plugin supports.

### Lifecycle and module information

8. `obs_module_load`, `obs_module_post_load` and `obs_module_unload` delegate to
   `pluginloader_load`, `pluginloader_post_load` and `pluginloader_unload`, which the C++ part of
   the plugin implements. `obs_module_load` returns `false` when `pluginloader_load` does.
9. At `obs_module_unload`, libobs objects are still valid. The plugin releases the references it
   holds there, but it does not destroy objects that libobs may still call back into. OBS Studio
   33.0.3 documents this in `obs-module.h`.
10. `obs_module_name`, `obs_module_description` and `obs_module_author` return what
    `pluginloader_get_name`, `pluginloader_get_description` and `pluginloader_get_author` return,
    which the C++ part of the plugin implements with the display name, the long description and
    the author of `src/plugin-support.h`. The Plugin Manager
    of OBS Studio 33.0.3 shows `obs_module_name` for a module whose data directory has no
    `manifest.json` with a `display_name`. Nothing in OBS Studio 33.0.3 calls the other two.

### Locale strings

11. The locale functions do what `OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")` would:
    `obs_module_set_locale` replaces the lookup with
    `obs_module_load_locale(module, "en-US", locale)`, which reads `locale/en-US.ini` and then the
    file of the given locale over it; `obs_module_free_locale` destroys it;
    `obs_module_get_string` calls `text_lookup_getstr`; and `obs_module_text` returns the string
    for a key, or the key itself when there is none.
12. A key is the English text itself. Code passes it as a string literal to `obs_module_text`, so
    the key reads as the text that appears in the UI. Keys compare by exact bytes, so their case
    matters.
13. `data/locale/en-US.ini` has an entry for every key that the code passes to `obs_module_text`,
    with the English text as its value, and no other entries. The file of another locale has
    entries only for keys of `en-US.ini`. A key it leaves out shows the English text.
14. Each entry is one line, `"key"="value"`, with the key in double quotes, because libobs ends an
    unquoted key at the first space. The only escapes are `\n`, `\t`, `\r` and `\"`.
15. Comment lines, the SPDX header among them, start with `#`, the only comment character of
    libobs's parser. A line that starts with `;` becomes an entry with the key `;`.
16. `.clang-format` sets `BreakStringLiterals: false`, so that a long key stays one string literal
    that can be searched for and compared with `en-US.ini`.

## Findings

Checked on 2026-11-25.

### What libobs looks up

`load_module_exports()` resolves the same 11 names, requiring the first 3, in the libobs vendored
in `vendor/obsproject/obs-studio` (commit 144599f, API 30.2.3) and in OBS Studio 33.0.3 (commit
f2aa9ce). The prototypes of these functions, `obs_module_load_locale`, `text_lookup_getstr`,
`text_lookup_destroy` and `blog` are also the same in both. Of the bodies of `OBS_DECLARE_MODULE()`,
`OBS_MODULE_USE_DEFAULT_LOCALE()` and `OBS_MODULE_AUTHOR()`, only the line breaks differ, apart from
a longer comment on `obs_module_unload` in 33.0.3.

In the vendored libobs, `obs_module_ver` is required but never called. In 33.0.3,
`obs_open_module()` calls it and rejects the module with `MODULE_INCOMPATIBLE_VER` when
`obs_module_ver() & 0xFFFF0000` is greater than its own `LIBOBS_API_VER`.

### What a module built with `obs-module.h` exports

A `plugin-main.c` that used `OBS_DECLARE_MODULE()`, `OBS_MODULE_USE_DEFAULT_LOCALE()` and its own
definitions of the other functions was built as a macOS bundle with Apple clang, C17:

- With `-fvisibility=hidden`, it exported 10 functions. `obs_module_author`, defined by hand, was
  hidden. Replacing it with `OBS_MODULE_AUTHOR()` gave the 11.
- Without `-fvisibility=hidden`, it exported 17 symbols: the 11, plus `obs_current_module`,
  `obs_module_text`, `obs_module_lookup` and three functions of the plugin.

### libobs's locale parser

`util/text-lookup.c` treats only `#` as a comment, in OBS Studio 33.0.3 and in the vendored
libobs alike (`/* comments are designated with a #, and end at LF */`, then `if (ch == '#')`). On
2026-11-26, `text_lookup_create` and `text_lookup_getstr` of the libobs shipped with OBS Studio
33.0.3 on macOS gave these results:

- A line `; Plugin names - Do not translate them.` became an entry with the key `;` and the value
  ` `, and the rest of the line was skipped: `; "Strength"="WRONG"` did not change `Strength`.
- The same lines starting with `#` were ignored.
- An unquoted `Up to date="Up to date"` gave the key `Up` with the value ` `. Quoted keys with
  spaces, `!`, `(`, `)`, `:` and `%` were read correctly.

### Exports and loading, checked by hand

These checks were made while the locale strings were still compiled in from `src/locale.c`, which
`src/plugin-main.c` was linked with. The set of exported functions does not depend on how the
strings are looked up. The two files were linked into a module with stand-ins for the C++ part,
which were compiled with hidden visibility as the C++ sources are. On macOS and Linux, the two
files were built both without and with `-fvisibility=hidden`, and the results were the same.

The export tables:

| Platform                            | Toolchain and link                                                                                                                     | Exports                                            | External references                             |
| ----------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------- | ----------------------------------------------- |
| macOS, universal (arm64 and x86_64) | Apple clang, `-mmacosx-version-min=13.0`, linked with `vendor/sdk/obs-29.1.3-macos/lib/libobs.tbd`                                     | The 11 functions in each slice (`nm -gU`)          | `blog` from `@rpath/libobs.framework`, and libc |
| Linux                               | GCC 16.2.0 (Debian forky), `-shared`                                                                                                   | The 11 functions (`nm -D --defined-only`)          | `blog`, and libc                                |
| Windows x64                         | MSVC 19.51 (Visual Studio 2026) with `/std:c17 /utf-8 /Zc:preprocessor /W4 /WX`, linked with `vendor/obs-deps/lib/x64-windows/obs.lib` | The 11 functions, undecorated (`dumpbin /exports`) | `blog` from `obs.dll`                           |

A loader that, like libobs, finds the functions by name (`dlopen` and `dlsym`, or `LoadLibrary`
and `GetProcAddress`) found all 11, and none of 18 internal names: `obs_current_module`,
`obs_module_text`, `obs_module_lookup`, `compare_lookup_key`, the three `pluginloader_*`
functions, the `g_plugin_*` strings, the tables and counts of
`src/locale.c`, and the two `static` variables of `src/plugin-main.c`. It then called each
function: `obs_module_ver` returned `0x1e020003` (30.2.3), `obs_module_get_string` returned `true`
for a key and `false` for an unknown string after `obs_module_set_locale("ja-JP")`, and
`obs_module_load`, `obs_module_post_load`, `obs_module_free_locale` and `obs_module_unload`
ran. It used the `blog` of the libobs installed with OBS Studio on macOS arm64 and of the
`obs.dll` installed with OBS Studio on Windows. On Linux, the loader defined `blog` itself.

A second loader went through libobs itself: `obs_startup("ja-JP", NULL, NULL)`,
`obs_open_module`, `obs_init_module`, the module information and locale getters, and
`obs_shutdown`. libobs thereby did its own lookup of the 11 names, the required-export check, the
version check of 33.0, `obs_module_set_locale`, `obs_module_load`, `obs_module_free_locale` and
`obs_module_unload`. With OBS Studio 33.0.3 on a macOS 15.8.3 x86_64 Mac and on a macOS 27.2.1
arm64 Mac, for both builds:

- `obs_open_module` returned `MODULE_SUCCESS`, and `obs_init_module` returned `true`.
- `obs_get_module_name`, `obs_get_module_author` and `obs_get_module_description` returned the
  module's strings.
- `obs_module_get_locale_string("Background Removal")` returned `true`.
- `obs_shutdown` returned normally.

The plugin built by `CMakeLists.txt`, with `src/PluginLoader.cpp` defining the
`pluginloader_*` functions and the `g_plugin_*` strings, was then checked on macOS. Both slices of
`obs-backgroundremoval.plugin` export exactly the 11 functions, all 42 tests pass, and loading it
through libobs 33.0.3 on arm64 returned `MODULE_SUCCESS`, the version 1.5.1 in its log, its name,
author and description, and the string for `Background Removal`.

On 2026-11-26, the `g_plugin_*` strings were replaced with the functions `pluginloader_get_name`,
`pluginloader_get_description` and `pluginloader_get_author`, so that the module and the static
library meet at functions only. The arm64 build on macOS still exported exactly the 11 functions,
and none of the three.

### The module with libobs's locale handling

On 2026-11-26, after `src/plugin-main.c` went back to libobs's `text_lookup`, the plugin built by
`CMakeLists.txt` on macOS exported exactly the 11 functions in both slices, imported only `blog`,
`obs_module_load_locale`, `text_lookup_getstr` and `text_lookup_destroy` from libobs, and passed
all 42 tests. Loaded through libobs 33.0.3 on arm64 with the locale `ja-JP`,
`obs_module_get_locale_string("Background Removal")` returned `背景除去` from `ja-JP.ini`.

Not checked: the full plugin build on Linux and Windows, and the module loaded through libobs on
Linux and Windows.

## Verification

- A check compares the keys that the sources pass to `obs_module_text` with the entries of
  `data/locale/en-US.ini`, and the entries of every other `.ini` file with those of `en-US.ini`,
  as libobs's parser reads them.
- The exported symbols of the built module are exactly the 11 functions of Requirement 1 on every
  platform.
