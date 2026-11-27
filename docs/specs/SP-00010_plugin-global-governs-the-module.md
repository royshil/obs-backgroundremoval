---
# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0
---

# SP-00010: PluginGlobal governs the plugin module

- Kind: Design principle
- Platforms: All

## Principle

`src/PluginGlobal` holds the code whose domain is the plugin module itself: how the module is
loaded and unloaded, and what the module is, such as its name, its author and its version. The
directory sits above the features of the plugin. Because its code is bound to the whole module,
every feature may use it, and a change to it may affect every feature. The division is not there to
isolate code. It keeps cohesive code together, and makes the reach of a change visible from where
the code is.

## Rationale

Most of the plugin's code belongs to a feature: the filter in `src/BackgroundRemoval`, the dialogs
in `src/UI`. A change there reaches that feature and nothing else. Some code belongs to no single
feature, because its subject is the module itself. The plugin loader is one: it is what
`src/plugin-main.c` calls, and it decides when each feature is set up and torn down. The version of
the plugin is another: the plugin knows its own version, fetches the latest release, compares the
two, and shows the result in more than one feature. `PluginGlobal::SemVer` exists to manage the
remote version, so it is bound to the module as strongly as the version is.

The features form a hierarchy under the module, and what is bound to the module is open to all of
them. A feature that needs `SemVer`, or any other part of `src/PluginGlobal`, uses it directly,
without a copy or a wrapper of its own. What the directory tells a reader is the reach of a change:
a change in a feature's directory stays within that feature, and a change in `src/PluginGlobal` has
to be judged against the whole module.

For that to hold, the directory has to keep its domain. A directory that every feature can use
attracts helpers that have no better home, and each one added there makes the reach of a change
harder to judge. Code is placed by its domain, so a helper of one feature stays next to that
feature.

## Requirements

1. `src/PluginGlobal` holds only code whose domain is the plugin module itself:

    - the plugin loader: the `pluginloader_*` functions with C linkage that `src/plugin-main.c`
        calls, as SP-00009 Requirement 3 lists, and the order in which they set up and tear down the
        features of the plugin;
    - the identity of the module: its name, its description and its author;
    - the version of the module: the version of this build, the latest released version fetched
        from the remote, and `PluginGlobal::SemVer`, which they are managed with.
    - the settings of the module: `update.ini` in the config directory of the plugin, which
        `PluginGlobal::UpdateConfig` reads and writes, with the version that the user has seen and
        whether update checks are enabled.

2. Every feature of the plugin may use what `src/PluginGlobal` declares, `SemVer` included. The
    division into directories marks the reach of a change, and is not a boundary that features have
    to keep out of.
3. Code is placed in `src/PluginGlobal` for its domain, not because it is shared. A helper that one
    feature uses stays in that feature's directory, and a general helper that several features use
    is not a reason to add it to `src/PluginGlobal`.
4. State that `src/PluginGlobal` keeps for the module, such as the latest released version, is
    created no earlier than `pluginloader_load`, and it is released, with any thread that it started
    joined, in `pluginloader_unload`, while libobs still runs. The features read or change it
    through functions that `src/PluginGlobal` declares, not through variables that it exposes.
5. New C++ names in `src/PluginGlobal` are in the namespace `PluginGlobal`. The functions with C
    linkage keep the prefix `pluginloader_`.
6. Code in `src/PluginGlobal` that needs neither libobs nor Qt, such as `SemVer`, does not depend on
    the features of the plugin, and is tested on its own: its test in `tests/PluginGlobal` compiles
    only the sources that it tests.

## Verification

- Every file in `src/PluginGlobal` belongs to one of the domains of Requirement 1.
- A change to `src/PluginGlobal` is reviewed for its effect on every feature that uses it.
- `pluginloader_unload` releases every piece of state that `src/PluginGlobal` keeps.
- `PluginGlobalUnitTest` builds and passes without the static library of the plugin, libobs or Qt.
