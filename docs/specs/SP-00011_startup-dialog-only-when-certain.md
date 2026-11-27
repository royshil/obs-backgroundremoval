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
startup after the plugin was installed or updated, and only after it has recorded that startup in
`update.ini`. When it cannot be certain, or cannot record, it never shows the dialog.

The plugin finds out whether it can be certain by using libobs as it naturally would, and checks
nothing beyond what those calls report. When a call fails, the plugin gives up at once: it neither
retries, nor falls back to another way, nor repairs what it found. The startup dialog is for a
startup on which everything is in good order. On almost every startup everything is, so the
strictness costs little.

Apart from that, a file named `NO_STARTUP_DIALOG.txt` in the config directory of the plugin turns
the startup dialog off without exception. The user or an administrator places it there, and the
plugin only looks for it.

The startup procedure is a transaction over `update.ini`. Reading the file and deciding is its
body, replacing the file with one that records the new version is its commit, and the dialog is
shown only once the commit has succeeded: every call has reported success, including the flushes
that ask the operating system to make it durable, and the operating system presents the new
content when the file is read back. Before the commit, nothing is shown, and
`update.ini` is not changed. After it, nothing is undone or retried.

This principle alone decides how the startup dialog behaves: when it is shown, in which order the
steps of the startup procedure run, and what each failure leads to. Another goal, such as telling
the user about a new version or letting them turn update checks off before the first fetch, is
pursued only where it does not conflict with the principle, and a behavior that conflicts with it is
a defect, whatever else it gains.

## Rationale

A dialog that opens by itself takes the user's attention before they can work. On the first
startup after an install or an update, that is a fair price for telling them what the plugin does
and letting them turn update checks off. On any other startup it is a nuisance, and if it comes back
at every startup, it is a defect that the user cannot work around: nothing in the dialog stops it
from returning, and the only remedy left is to remove the plugin.

The plugin learns which startup this is from `update.ini`, a file that it does not control. The
config directory may be read-only or on a failing disk, the file may be unreadable or edited by
hand, two installations of different versions may share it, and the frontend may have no main
window. Each such state could make the plugin take an ordinary startup for a first one. Rather than
telling these states apart with checks of its own, which would only grow and still miss cases, the
plugin accepts only what it can recognize by ordinary use of the file: the file does not exist, or
it is valid and records a version older than this one. A valid file that records no version is
deliberately taken as recording 0.0.0, so that a file from version 1.5.0, or one that the About
dialog wrote before any startup committed, leads to the dialog once. Anything else, and any failure
on the way, ends the procedure without the dialog. Missing a dialog that should have been shown
costs the user little, because the About dialog stays in the Tools menu.

Some users never want the dialog: an administrator who deploys OBS Studio to many machines, or a
user whose `update.ini` is reset by their setup. `NO_STARTUP_DIALOG.txt` gives them a switch that
does not depend on `update.ini` at all, and that the plugin cannot turn back on, because it never
writes the file. Checking it first keeps the switch absolute: when it is there, nothing else in the
procedure runs. The procedure then records nothing, so removing the file later leaves the next
certain startup to show the dialog: the first one after the removal if `update.ini` does not exist
or records no version, for example because the About dialog was closed in the meantime, or the
first one after the next update.

Treating the replacement as a commit is what keeps the dialog from repeating. The dialog follows a
durable commit, and a commit changes the recorded version, so a later startup that reads the
committed file is no longer certain: except for the cases under Known limits, each version is shown
at most once. A failure before the commit leaves the file as it was, and since nothing has been
shown, deciding again at the next startup from the same file repeats nothing. A failure after the
commit, such as a dialog that fails to show or a session that ends while it is open, is not
retried, because the commit already says that this startup has happened.

## Requirements

### Terms

- A file *opens* when `config_open` with `CONFIG_OPEN_EXISTING` returns `CONFIG_SUCCESS`. That
    does not mean that anything was read: an empty file, a read that returns no bytes, and on Linux
    and macOS a directory, open with no sections (`config_parse_file` and `os_fread_utf8`; see
    Findings).
- A file is *valid* when it opens and its section `[update]` has a value (`config_has_user_value`)
    for `version` or for `check_for_updates`. Every file that the plugin or version 1.5.0 writes has
    at least one of them, so an empty read is never valid.
- The *recorded version* of a valid file is its `version`, or 0.0.0 when it has none.
- The libobs functions named here are those of OBS Studio 32.2.2 and 33.0.3, which do not differ
    in `util/config-file.c`, `util/platform.c`, `util/platform-nix.c` and
    `util/platform-windows.c`.

### The startup dialog

1. The startup procedure runs at most once per session, after
    `OBS_FRONTEND_EVENT_FINISHED_LOADING`, once no modal dialog is open. When the wait for modal
    dialogs cannot be set up (`AboutDialogIntegration::runWhenNoModalDialog` returns false), the
    procedure gives up, and the dialog is not shown.
2. First of all, before it looks at `update.ini`, the procedure checks with `os_file_exists`
    whether `NO_STARTUP_DIALOG.txt` exists in the config directory of the plugin, next to
    `update.ini` (`obs_module_config_path("NO_STARTUP_DIALOG.txt")`). When it exists, the procedure
    ends there: it neither reads nor writes `update.ini`, and the dialog is not shown. The content
    of the file does not matter, and a directory of that name counts as well.
    `NO_STARTUP_DIALOG.txt` is the canonical name, and the plugin compares no names itself: the
    file system decides whether a name in another case matches, as it does on the case-insensitive
    file systems that Windows and macOS use by default. The plugin never creates, changes or
    removes the file.
3. The procedure decides with two calls:

    - When `os_file_exists` reports that `update.ini` does not exist, this is the first startup
        after an install.
    - Otherwise the procedure opens the file. When it does not open, the procedure gives up. When
        it is valid and its recorded version is less than `PLUGIN_VERSION` as semantic versions
        (SP-01021), this is the first startup after an update.

    In every other case the procedure is not certain: the file is not valid, or its `version` is
    not a semantic version, or is equal to or greater than `PLUGIN_VERSION` in precedence, which
    ignores build metadata. `os_file_exists` also reports false when it cannot tell, for example
    when a parent directory cannot be searched; the commit then usually fails, by Requirement 4,
    so that answer needs no check of its own.
4. When the procedure is certain, it commits before it shows the dialog, and shows it only when the
    commit has succeeded. The commit uses libobs's config API and its `os_*` file functions, and
    calls the platform only to flush. A failure of any step, reported by its return value, ends the
    procedure without the dialog:

    1. Remove `update.ini.tmp` with `os_unlink`, whose result is not checked. A leftover that
        cannot be removed makes a later step fail or is overwritten by it.
    2. Make `update.ini.tmp`. For an update, copy `update.ini` to it with `os_copyfile`, which
        fails when the target exists, and open the copy with `config_open` and
        `CONFIG_OPEN_EXISTING`. For a first install, create the config directory with `os_mkdirs`,
        and the file with `config_create`.
    3. Set `version` to `PLUGIN_VERSION`, and `check_for_updates` to the value that the decision
        read from `update.ini`, or to `true` when it read none, and write the file with
        `config_save`. The other keys of the copy are written back as they were read. Taking
        `check_for_updates` from the decision rather than from the copy keeps a short copy from
        turning a user's `false` into `true`.
    4. Read `update.ini.tmp` back with `config_open` and `CONFIG_OPEN_EXISTING`, and check that it
        holds the `version` and the `check_for_updates` of step 3. This catches a write that failed
        unnoticed while `update.ini` is still as it was: `config_save` checks its `fwrite` calls but
        not `fclose`, where a small file is actually written, and on Linux and macOS `os_copyfile`
        checks neither a short final `fwrite` nor `fclose`, so a full disk can leave an empty or
        short `update.ini.tmp`. On Windows, `os_copyfile` is `CopyFileW`, which reports its own
        errors.
    5. Flush `update.ini.tmp` to the disk:

        | Platform | Call |
        | --- | --- |
        | Linux | `open` with `O_WRONLY`, `fsync`, `close` |
        | macOS | `open` with `O_WRONLY`, `fcntl` with `F_FULLFSYNC`, `close` |
        | Windows | `CreateFileW` with `GENERIC_WRITE`, `FILE_SHARE_READ \| FILE_SHARE_WRITE \| FILE_SHARE_DELETE` and `OPEN_EXISTING`, `FlushFileBuffers`, `CloseHandle` |

        On macOS, `fsync` does not ask the drive to write its cache, and `F_FULLFSYNC` does. A file
        system that does not support `F_FULLFSYNC` fails this step, and the dialog is not shown:
        the plugin does not fall back to `fsync`.
    6. Rename `update.ini.tmp` to `update.ini`, replacing it. This is the commit. On Linux and
        macOS this is `os_rename`, which is `rename`. On Windows it is `MoveFileExW` with
        `MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH`, with the paths converted by
        `os_utf8_to_wcs_ptr`, since libobs's `os_rename` passes no `MOVEFILE_WRITE_THROUGH`.
    7. Make the rename durable. On Linux and macOS, flush the config directory, opened with
        `O_RDONLY`, with `fsync` on Linux and `F_FULLFSYNC` on macOS. When step 2 created the config
        directory (`os_mkdirs` returned `MKDIR_SUCCESS`), flush its parent, `plugin_config`, in the
        same way. On Windows, step 6's `MOVEFILE_WRITE_THROUGH` is all that the plugin does.
    8. Read `update.ini` back in the same way as step 4, and check the same values. This confirms
        that the operating system now presents the new content as `update.ini`, which is what the
        next startup will read, without taking the result of the rename on trust across the
        platforms and file systems that the plugin runs on. It does not confirm that the content is
        on the disk: only the operating system knows that, so the plugin asks for it with the
        flushes of steps 5 and 7 and does not try to check it.

    Until step 6, `update.ini` is as it was, and a first install leaves no `update.ini`. The
    decision and the commit run under one lock, so that no other call within the process writes
    `update.ini` between them.

    A commit that cannot be written does not happen, and then the dialog is not shown. This is the
    intended behavior, not a limit: whatever makes a call of the commit fail, such as a full disk,
    ends the procedure in silence, at every startup until the cause is removed. The plugin does not
    look at file modes or attributes: whether the files can be written is for the operating system
    to decide, and the plugin learns it only from the results of the calls. When they can be
    written, the commit records the version, so the dialog cannot come back.
5. When the procedure is not certain, it writes nothing and shows nothing. It does not record
    `PLUGIN_VERSION` over a version that is newer or not a semantic version, or into a file that is
    not valid: recording over a newer version would let two installations that share `update.ini`
    and are started in turn take every startup of the newer one for an update.
6. A file that exists but does not open is left as it is. The plugin does not create, empty or
    rewrite it.
7. A failure after step 6, of the flush in step 7, of the check in step 8, or of showing the
    dialog, is not retried, in this session or the next.
8. `PLUGIN_VERSION` comes from the file `VERSION` and is a semantic version of three parts,
    `MAJOR.MINOR.PATCH`, as CMake's `project(VERSION)` requires, which accepts no pre-release.
    CMake also accepts four parts, but such a `PLUGIN_VERSION` is not a semantic version: with it,
    a first install is still certain, but no update ever is.

### Update checks

9. Whenever the startup procedure ends without showing the dialog, for any reason, including
    Requirements 1 and 2 and a failure to show it, the plugin calls
    `PluginLoaderFetchRemoteLatestVersionAsync()` in the same session, which fetches the latest
    version only when `checkForUpdatesEnabled()` returns true. When the dialog has been shown, the
    latest version is fetched only after the user closes it with update notifications enabled.
10. `checkForUpdatesEnabled()` returns true only when `update.ini` opens and states
    `check_for_updates` as true. A missing file, a file that does not open, and a file without the
    value all return false, so that only a file that the plugin committed, or that the user wrote,
    can enable a request. libobs's defaults (`config_set_default_bool`) are not used.
11. `setCheckForUpdates()`, which the About dialog calls when it is closed, writes through the
    steps of Requirement 4, with these differences:

    - Before step 1, it opens `update.ini`. When the file exists but is not valid, it gives up,
        so that it never rewrites a file whose read may have failed.
    - In step 3 it sets `check_for_updates` to the setting of the user and changes nothing else.
        It writes no `version`: that is the startup procedure's record.
    - In steps 4 and 8 it checks only `check_for_updates`.
    - When the file does not exist, it creates it as a first install does, with only
        `check_for_updates`.

### `update.ini`

12. `PluginGlobal::UpdateConfig` (`src/PluginGlobal/UpdateConfig.cpp`) is the only code that reads
    or writes `update.ini`, which is `obs_module_config_path("update.ini")`: on OBS Studio,
    `obs-studio/plugin_config/obs-backgroundremoval/update.ini` in the user's config directory,
    since the frontend always passes `obs-studio/plugin_config` to `obs_startup`. It keeps the keys
    that version 1.5.0 wrote, in the section `[update]`:

    | Key | Value | Missing |
    | --- | --- | --- |
    | `version` | The plugin version that the startup procedure last committed | 0.0.0 in a valid file |
    | `check_for_updates` | `true` or `false`, as libobs's `config_get_bool` reads it | False |

13. Every function opens `update.ini`, reads or writes it, and closes it within the call. Every
    write goes through the steps of Requirement 4, so that `update.ini` is only ever replaced by a
    rename. `config_save_safe` is not used. A mutex serializes the calls within the process.
14. The functions are called between `pluginloader_load` and `pluginloader_unload`, while
    `obs_current_module()` is valid, and no exception leaves them.

## Findings

### How libobs reads a broken `update.ini`

Checked on 2026-11-27 with `config_open` and `CONFIG_OPEN_EXISTING`, each case in a process of its
own so that a crash would show:

- Ubuntu 26.04 in WSL 2 (kernel 6.18, ext4), with libobs 32.1.0 from Ubuntu;
- macOS 27.2 (APFS), with the libobs of OBS Studio 33.0.3;
- Windows 11 (NTFS), with `obs.dll` of OBS Studio 33.0.3, built with Visual Studio 18.

All three gave the same results, except for a directory.
A broken file is handled by the same rules as any other, with no check of its own, and the last
column follows from Requirements 3 and 5.

| Content of `update.ini` | Valid | `version` read | Outcome |
| --- | --- | --- | --- |
| A directory: opens on Linux and macOS (`ftell` 0 on ext4, 64 on APFS), does not open on Windows | No | | Silence |
| Empty, or only a UTF-8 BOM | No | | Silence |
| Binary data, or a NUL byte before `[update]` | No | | Silence |
| No section, `[Update]`, `Version=`, or ` version = ` with spaces | No | | Silence |
| `[upd`, cut inside the section name | No | | Silence |
| A line without `=` | No | | Silence |
| `version=1.5` cut short | Yes | `1.5` | Not a semantic version: silence |
| `version="1.5.0"` | Yes | `"1.5.0"` | Not a semantic version: silence |
| `version=1.5.0` followed by invalid UTF-8 | Yes | `1.5.0` and the bytes | Not a semantic version: silence |
| `version=1.5.0\n2`, with an escaped line break | Yes | Two lines | Not a semantic version: silence |
| Garbage lines before `[update]`, comments, CRLF, a BOM, no final line break | Yes | `1.5.0` | Compared as usual |
| A NUL byte after `version=1.5.0` | Yes | `1.5.0`; the rest is lost | Compared as usual |
| `version` twice, or `[update]` twice | Yes | The last one | Compared as usual |
| Only `check_for_updates=maybe` | Yes | None, so 0.0.0 | Certain: the dialog once, then a clean file |

No case crashed. A broken file thus leads to silence, or to the dialog once, after which the commit
leaves a file that libobs wrote itself. On Windows, a directory named `update.ini` exists by
`os_file_exists` but does not open, so the procedure gives up by Requirement 3, with the same
silence.

## Known deviations

None. `src/PluginGlobal/UpdateConfig.cpp` and the startup procedure in `src/PluginGlobal/PluginLoader.cpp`
follow the requirements as of 2026-11-27.

### How the implementation behaves

Checked on 2026-11-27 with a harness that calls `PluginGlobal::UpdateConfig` against the real
libobs in a temporary config directory, in place of `obs_module_get_config_path`: on macOS 27.2 with
OBS Studio 33.0.3 and AddressSanitizer, on Ubuntu 26.04 in WSL 2 with libobs 32.1.0 and
AddressSanitizer, and on Windows 11 with OBS Studio 33.0.3 and Visual Studio 18. It covered a first
install, an update keeping `check_for_updates=false` and other keys, equal, newer, short, quoted and
build-metadata versions, a pre-release before its release, files that are not valid, a valid file
without a version, `NO_STARTUP_DIALOG.txt` as a file and as a directory, a config directory that
cannot be written, a leftover `update.ini.tmp` as a file and as a directory, `update.ini` as a
directory, the config directory being a file, and the About dialog closed with no file, with a
file, and with an empty file. Every case behaved as the requirements say on all three platforms.
A config directory that cannot be written was arranged only on Linux and macOS: Windows ignores
the read-only attribute of a directory. A lowercase `no_startup_dialog.txt` counted on macOS and Windows, and not on
Linux. On Windows, `config_save` wrote the file with a UTF-8 BOM.

## Known limits

These concern the commit or the reading of `update.ini`. The principle asks for no checks against
them. Each can show the dialog once more, or keep it away, but none shows it at every startup.

- Durability rests on the operating system and the drive keeping the promises of the flushes,
    which the plugin cannot observe and which has not been tested. On macOS,
    `F_FULLFSYNC` on a directory returns success on APFS, but that it makes the rename durable is
    not documented. On Windows, `MOVEFILE_WRITE_THROUGH` is documented to flush a move done as a
    copy and a delete, not a rename within one volume, and the creation of the config directory is
    not flushed. A drive that reports a flush that it did not do can lose the commit. In each case a
    power loss right after the commit can show the dialog once more for the same version.
- When the check of step 8 fails, `update.ini` has already been replaced, by a file that is empty,
    short, or holds another `version`. Nothing is shown. An empty file is not valid, so no later
    startup is certain until the file is removed. A short `version`, such as `1.5`, is not a
    semantic version, so no update is certain, and `check_for_updates` may be missing, so no fetch
    is made, until the file is removed.
- A read that returns fewer bytes than the file holds parses a truncated file. A truncated
    `version` may read as older than `PLUGIN_VERSION`, and the dialog is shown once.
- When `update.ini` is a dangling symbolic link, `os_file_exists` reports false, the first-install
    commit replaces the link with a file, and the dialog is shown once.
- A file that the About dialog created before any startup committed has no `version`, so the next
    startup shows the dialog once, although the user has seen the About dialog of this version.

These concern update checks, not the startup dialog, and the principle allows them.

- When the commit has succeeded but `AboutDialogIntegration::show()` fails, the user has not
    decided about update checks, but the committed `check_for_updates=true` lets this session, by
    Requirement 9, and every later startup fetch the latest version. The same holds from the next
    startup on when the session ends while the dialog is open. The user still finds the setting in
    the About dialog from the Tools menu.
- When `update.ini` does not open, or is not valid, while the About dialog opens, its checkbox
    shows update notifications as disabled, and closing the dialog writes nothing, by Requirements
    6 and 11.
- The wait of Requirement 1 watches only modal dialogs, and the About dialog is not modal. When the
    user opens it from the Tools menu while the startup dialog is open, two About dialogs are open,
    and the one that is closed last decides `check_for_updates`.

## Verification

The durability of steps 5 and 7 of Requirement 4 cannot be tested, and is checked by reading the
code. The other items are tested by starting OBS Studio with a prepared config directory.

- With no `update.ini`, the first startup creates it with `version` set to `PLUGIN_VERSION` and
    `check_for_updates=true`, and shows the About dialog. The second startup shows nothing.
- With `version` older than `PLUGIN_VERSION`, or with `check_for_updates` and no `version`, the
    startup shows the dialog once and records the version, keeping `check_for_updates=false` when
    the file has it. With an equal or newer version or a version that is not a semantic version, it
    shows nothing and leaves the file as it was.
- Two installations of different versions that both follow this document, share `update.ini` and
    are started in turn show the dialog at most once for each version, and never again once the
    newer one has committed.
- With an `update.ini` that is empty or cannot be read, and the About dialog not closed in
    between, no startup shows the dialog, the file is left as it was, and no fetch is made.
- With no `update.ini` and a config directory that cannot be created or written, no startup shows
    the dialog, and no fetch is made.
- When a step before the rename fails, no dialog is shown, `update.ini` is as it was before the
    startup, and a first install leaves no `update.ini`.
- When `update.ini.tmp` is written empty or short, the check of step 4 fails, `update.ini` is as
    it was, and no dialog is shown. A full disk cannot be arranged reliably, so this is checked by
    reading the code.
- With a config directory that cannot be written, no startup shows the dialog, and `update.ini` is
    left as it was.
- Each case of the table under Findings leads to the outcome listed there, on Linux, macOS and
    Windows.
- With `NO_STARTUP_DIALOG.txt`, or a directory of that name, in the config directory of the plugin,
    no startup shows the dialog, with or without `update.ini`, and `update.ini` is left as it was.
    On the case-insensitive file systems that Windows and macOS use by default,
    `no_startup_dialog.txt` has the same effect. After the file is removed, a startup without
    `update.ini` shows the dialog once.
- When `runWhenNoModalDialog` fails, no dialog is shown in that session. A failure to show the
    dialog after the commit does not show it in the next session.
- With `check_for_updates=false`, or without the key, no startup fetches the latest version.
- An `update.ini` written by version 1.5.0 with a `version` keeps its `check_for_updates`, and the
    first startup of a newer version shows the dialog once.
- Closing the About dialog from the Tools menu keeps the `version` of an existing file, leaves a
    file that is not valid or does not open as it was, and with no `update.ini` creates one with
    only the chosen `check_for_updates`.
- `src/PluginGlobal/UpdateConfig.cpp` does not call `config_save_safe` or `CONFIG_OPEN_ALWAYS`.
