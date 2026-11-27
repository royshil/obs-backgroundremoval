---
# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0
---

# SP-00011: The startup dialog is shown only when it is certain

- Kind: Design principle
- Platforms: All

## Principle

The plugin shows the About dialog on its own only when it is certain that this is the first
startup after an install or an update, and only after it has recorded that startup in
`update.ini`. When it cannot be certain, or cannot record, it shows nothing.

Recording is a transaction over `update.ini`: reading the file and deciding is its body, and
replacing the file with one that holds the new version is its commit. The dialog is shown only when
the commit has succeeded. Before the commit nothing is shown and `update.ini` is unchanged, and after
it nothing is undone or retried.

The plugin learns whether it can be certain, and whether the commit succeeded, only from what
libobs and the operating system report when used as usual. It adds no checks of its own, and gives
up at the first failure, without retrying, falling back or repairing. On almost every startup
everything is in good order, so this strictness costs little.

`NO_STARTUP_DIALOG.txt` in the config directory of the plugin turns the startup dialog off without
exception. The plugin only looks for it, and never creates it.

This principle alone decides how the startup dialog behaves. Other goals, such as update checks,
are pursued only where they do not conflict with it.

## Rationale

A dialog that opens by itself is fair on the first startup after an install or an update. On any
other startup it is a nuisance, and one that comes back at every startup is a defect that the user
cannot work around, short of removing the plugin. Missing a dialog costs little in comparison: the
About dialog stays in the Tools menu.

`update.ini` is not under the plugin's control. It may be unwritable, broken, edited by hand, or
shared by two installations. Telling such states apart would take checks that only grow and still
miss cases. Instead, the plugin accepts only two states it can recognize by reading the file, and
lets any failure end the procedure. Because the dialog follows a commit that changes the recorded
version, the next startup is no longer certain, so the dialog cannot repeat. A failure before the
commit changes nothing, so the next startup decides the same way and shows nothing again.

## Requirements

### Terms

- A file *opens* when `config_open` with `CONFIG_OPEN_EXISTING` returns `CONFIG_SUCCESS`. This
    does not mean that anything was read: an empty file opens with no sections.
- A file is *valid* when it opens and its section `[update]` has a value
    (`config_has_user_value`) for `version` or `check_for_updates`.
- The *recorded version* of a valid file is its `version`, or 0.0.0 when it has none.

### The startup dialog

1. The startup procedure runs at most once per session, after
    `OBS_FRONTEND_EVENT_FINISHED_LOADING`, once no modal dialog is open. When that wait cannot be
    set up, the procedure gives up.
2. First, the procedure checks with `os_file_exists` for
    `obs_module_config_path("NO_STARTUP_DIALOG.txt")`. When it exists, as a file or a directory,
    the procedure ends without touching `update.ini`. The file system decides whether a name in
    another case matches.
3. The procedure is certain in two cases only:

    - `os_file_exists` reports that `update.ini` does not exist: a first install.
    - `update.ini` is valid, and its recorded version is less than `PLUGIN_VERSION` as semantic
        versions (SP-01021): an update.

    In every other case it writes nothing and shows nothing. In particular, it never writes into a
    file that does not open or is not valid, nor over a version that is equal, newer, or not a
    semantic version. `PLUGIN_VERSION` has three parts, as CMake's `project(VERSION)` gives it.
4. When certain, the procedure commits, and shows the dialog only when every step succeeds. The
    decision and the commit run under one lock.

    1. Remove `update.ini.tmp` with `os_unlink`, ignoring the result.
    2. For an update, copy `update.ini` to `update.ini.tmp` with `os_copyfile` and open the copy.
        For a first install, create the directory with `os_mkdirs` and the file with
        `config_create`.
    3. Set `version` to `PLUGIN_VERSION` and `check_for_updates` to the value read in the
        decision, or `true` when there was none, and write with `config_save`. Other keys are kept.
    4. Read `update.ini.tmp` back and check both values, because `config_save` and `os_copyfile` do
        not check every write.
    5. Flush `update.ini.tmp`.
    6. Rename `update.ini.tmp` to `update.ini`, replacing it. This is the commit.
    7. Flush the rename.
    8. Read `update.ini` back and check both values. This confirms that the operating system
        presents the new content as `update.ini`, without trusting the result of the rename. Whether
        it is on the disk only the operating system knows, so the plugin asks for that with the
        flushes and does not check it.

    | Platform | Step 5 | Step 6 | Step 7 |
    | --- | --- | --- | --- |
    | Linux | `fsync` | `rename` | `fsync` on the directory |
    | macOS | `fcntl(F_FULLFSYNC)` | `rename` | `F_FULLFSYNC` on the directory |
    | Windows | `FlushFileBuffers` | `MoveFileExW` with `MOVEFILE_REPLACE_EXISTING \| MOVEFILE_WRITE_THROUGH` | Nothing more |

    When step 2 created the config directory, step 7 also flushes its parent on Linux and macOS.
    A step that fails, including on a full disk or a file system without `F_FULLFSYNC`, ends the
    procedure in silence. The plugin does not look at file modes or attributes: whether the files
    can be written is the operating system's to decide.
5. A failure after step 6, including a failure to show the dialog, is not retried.

### Update checks

6. Whenever the startup procedure ends without showing the dialog, the plugin calls
    `PluginLoaderFetchRemoteLatestVersionAsync()`. When the dialog is shown, the fetch waits until
    the user closes it with update notifications enabled.
7. `checkForUpdatesEnabled()` is true only when `update.ini` opens and states `check_for_updates`
    as true. No libobs defaults are used.
8. `setCheckForUpdates()`, called when the About dialog is closed, writes through Requirement 4
    with only `check_for_updates`, and checks only that value. It gives up on a file that exists and
    is not valid, and creates the file when there is none.

### `update.ini`

9. `PluginGlobal::UpdateConfig` is the only code that reads or writes
    `obs_module_config_path("update.ini")`, with these keys in `[update]`:

    | Key | Value | When missing |
    | --- | --- | --- |
    | `version` | The version last committed by the startup procedure | 0.0.0 in a valid file |
    | `check_for_updates` | `true` or `false` | False |

10. Every write goes through Requirement 4. `config_save_safe` and `CONFIG_OPEN_ALWAYS` are not
    used. Each function opens and closes the file within the call, runs under one mutex, lets no
    exception out, and is called only while the module is loaded.

## Findings

On 2026-11-27, broken `update.ini` files were read with `config_open` on Ubuntu 26.04 in WSL 2
with libobs 32.1.0, macOS 27.2 with OBS Studio 33.0.3, and Windows 11 with OBS Studio 33.0.3. No
case crashed, and all three platforms agreed, except that a directory named `update.ini` opens on
Linux and macOS but not on Windows. Either way it leads to silence.

| Content of `update.ini` | Outcome |
| --- | --- |
| Empty, binary, cut inside `[update]`, no `[update]`, or a misspelled key | Not valid: silence |
| `version=1.5`, `version="1.5.0"`, or a value with trailing garbage | Not a semantic version: silence |
| Comments, CRLF, a BOM, extra lines, or a duplicate key (the last wins) | Read as usual |
| `check_for_updates` without `version` | 0.0.0: the dialog once |

A harness that ran `PluginGlobal::UpdateConfig` against the real libobs on the same three
platforms, with AddressSanitizer on Linux and macOS, behaved as the requirements say in every case.
Windows ignores the read-only attribute of a directory, so an unwritable config directory was
tested only on Linux and macOS.

## Known limits

Each of these can show the dialog once more, or keep it away, but none shows it at every startup.

- Durability rests on the operating system and the drive. Power lost right after the commit can
    show the dialog once more.
- When the check of step 8 fails, `update.ini` has already been replaced by a bad file, which keeps
    the dialog and fetches away until the file is removed.
- A file that the About dialog created before any startup committed has no `version`, so the next
    startup shows the dialog once.
- Two OBS Studio processes running at once are not considered.

## Verification

Durability is checked by reading the code. The rest is tested by starting OBS Studio with a
prepared config directory.

- With no `update.ini`, the first startup records `PLUGIN_VERSION` and shows the dialog, and the
    second shows nothing.
- With an older `version`, or none in a valid file, the startup shows the dialog once and keeps
    `check_for_updates`. With an equal, newer or invalid version, nothing is shown or written.
- With an `update.ini` that is not valid, or a config directory that cannot be written, nothing is
    shown and the file is left as it was.
- With `NO_STARTUP_DIALOG.txt`, nothing is shown and `update.ini` is left as it was.
- With `check_for_updates=false` or without it, no fetch is made.
- Closing the About dialog keeps `version`, and leaves a file that is not valid as it was.
