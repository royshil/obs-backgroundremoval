---
# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0
kind: Design principle
platforms: All
---

# SP-00011: The startup dialog is shown only when it is certain

## Principle

The plugin shows the About dialog on its own only on the first startup after an install or an
update, and only once it has stored this version in `update.ini`. When it is not certain of both,
it shows nothing.

Storing the version is a transaction: the plugin reads `update.ini`, decides, and commits by
replacing the file. The dialog is shown only after a successful commit, and nothing is retried.

The plugin relies only on what libobs and the operating system report. It adds no checks of its
own, and gives up at the first failure. Almost every startup succeeds, so this costs little.

`NO_STARTUP_DIALOG.txt` in the config directory of the plugin always turns the startup dialog off.
The plugin never creates it.

This principle alone decides how the startup dialog behaves. Other goals, such as update checks,
give way to it.

## Rationale

A dialog that opens by itself is fair on the first startup after an install or an update. On any
other startup it is a nuisance, and one that comes back at every startup is a defect that the user
cannot work around, short of removing the plugin. Missing a dialog costs little in comparison: the
About dialog stays in the Tools menu.

`update.ini` is not under the plugin's control. It may be unwritable, broken, edited by hand, or
shared by two installations. Telling such states apart would take checks that only grow and still
miss cases. Instead, the plugin accepts only two states it can recognize by reading the file, and
lets any failure end the procedure. Because the dialog follows a commit that changes the stored
version, the next startup is no longer certain, so the dialog cannot repeat. A failure before the
commit changes nothing, so the next startup decides the same way and shows nothing again.

## Requirements

### How the requirements are written

Each operation on `update.ini` is specified as in Crash Hoare Logic, the logic of the verified file
system FSCQ (Chen et al., "Using Crash Hoare Logic for Certifying the FSCQ File System",
SOSP 2015), with the labels that the paper uses, and in prose:

- **PRE** is what holds when the operation starts.
- **POST** is what holds when the operation returns. It is split into named outcomes, which
    together cover every case and of which exactly one holds.
- **CRASH** is what holds if the process or the system stops at any point during the operation.

Crash Hoare Logic also requires that recovery after a crash can start again from what CRASH leaves.
Here, recovery is the next startup: what CRASH leaves must be a state from which the next startup
decides as usual.

### Assumptions

The commit relies on these properties of the operating system, which the plugin cannot check:

1. Other processes see `update.ini` replaced as a whole, never in between. POSIX requires this of
    `rename`.
2. A crash also leaves `update.ini` either as it was or replaced as a whole. macOS documents this
    for `rename`. On Linux, journaling and copy-on-write file systems behave this way but do not
    document it. Windows documents neither.
3. Content flushed before the replacement is on the disk before the replacement is.
4. A replacement flushed after it is made stays after a power loss, as far as the drive keeps the
    promises of the flush.

DFSCQ (Chen et al., SOSP 2017) proves that this pattern, a temporary file flushed and then renamed
over the target, leaves either the old or the new content after a crash, under its model of the file
system. Bornholt et al. (ASPLOS 2016) show that on ext4 the replacement can lose the new content
when the temporary file is not flushed before it, which is why Assumption 3 is needed.

### States of `update.ini`

`update.ini` in the config directory of the plugin is in one of these states:

| State | Meaning |
| --- | --- |
| Missing | No file of that name exists. |
| Valid | It can be read, and its section `[update]` has `version` or `check_for_updates`. |
| Invalid | Anything else, such as an empty or unreadable file, or a directory. |

The *stored version* of a valid file is its `version`, or 0.0.0 when it has none. Its
`check_for_updates` is false when it has none.

### Commit

The commit replaces `update.ini` with a file in a target state: it writes a temporary file next to
`update.ini`, flushes it, replaces `update.ini` with it, flushes the replacement, and reads
`update.ini` back.

- **PRE:** No other commit runs in the process.
- **POST, committed:** Every operation reported success, and `update.ini`, read back, is in the
    target state.
- **POST, failed:** An operation before the replacement failed, and `update.ini` is unchanged.
- **POST, unconfirmed:** The replacement or an operation after it failed, or the read-back
    differed, and `update.ini` is unchanged or in the target state.
- **CRASH:** `update.ini` is unchanged or in the target state, by Assumptions 2 and 3. The
    temporary file may remain, and the next commit removes it first.

The plugin does not look at file modes or attributes. Whether the files can be written is the
operating system's to decide, and the plugin learns it only from the results of the operations.

### Startup

The startup procedure runs at most once per session, after OBS Studio has finished loading and no
modal dialog is open. When it cannot wait for that, it gives up, and only the fetch below runs.

When `NO_STARTUP_DIALOG.txt`, as a file or a directory, exists next to `update.ini`, the procedure
neither reads nor writes `update.ini`, and does not show the dialog. The file system decides
whether a name in another case matches. Otherwise the target state is:

| `update.ini` before | Target state |
| --- | --- |
| Missing | Valid, with `version` set to `PLUGIN_VERSION` and `check_for_updates` true |
| Valid, with a stored version less than `PLUGIN_VERSION` (SP-01021) | Valid, with `version` set to `PLUGIN_VERSION`, `check_for_updates` kept or set true when missing, and the other keys kept |
| Valid, with a stored version that is equal, greater or not a semantic version | Unchanged |
| Invalid | Unchanged |

`PLUGIN_VERSION` is a semantic version of three parts.

- **PRE:** The dialog has not been shown in this session.
- **POST, declined:** The target state is unchanged. Nothing is written, and the dialog is not
    shown.
- **POST, shown:** The commit was committed, and the dialog is shown.
- **POST, given up:** The commit failed or was unconfirmed, and the dialog is not shown. It is not
    retried, in this session or a later one.
- **CRASH:** `update.ini` is unchanged or in the target state.
- **Invariant:** Once the dialog has been shown, the stored version is `PLUGIN_VERSION`, so the next
    startup declines. This holds after a crash as well, under Assumption 4.

Whenever the procedure ends without showing the dialog, the plugin fetches the latest version if
`update.ini` is valid and its `check_for_updates` is true. When the dialog is shown, the fetch waits
until the user closes it with update notifications enabled.

### About dialog closed

When the About dialog is closed with update notifications set to a value, the target state is:

| `update.ini` before | Target state |
| --- | --- |
| Missing | Valid, with only `check_for_updates` set to the value |
| Valid | Valid, with `check_for_updates` set to the value and the other keys kept |
| Invalid | Unchanged |

- **PRE:** None.
- **POST, declined:** `update.ini` is invalid, and nothing is written.
- **POST, committed, failed or unconfirmed:** As the commit returned.
- **CRASH:** `update.ini` is unchanged or in the target state.

Because it keeps `version`, closing the About dialog keeps the invariant of the startup.

## Known limits

Each of these can show the dialog once more, or keep it away, but none shows it at every startup.

- The assumptions are not guaranteed. Windows documents no crash atomicity of the replacement, and
    a drive that ignores a flush, or a power loss on macOS without the full flush, can bring back
    the old `update.ini` after the dialog was shown, which then shows it once more.
- An unconfirmed commit may have left `update.ini` in a state that keeps the dialog and fetches
    away until the file is removed, such as a file that is short or empty.
- A file that the About dialog created before any startup committed has no `version`, so the next
    startup shows the dialog once.
- Two OBS Studio processes running at once are not considered.

## Verification

The assumptions and the flushes are checked by reading the code. The rest is tested by starting
OBS Studio with a prepared config directory.

- Each row of the tables under Startup and About dialog closed leads to the target state and the
    outcome listed there.
- After the dialog has been shown, the next startup declines.
- With a config directory that cannot be written, the commit fails, nothing is shown, and
    `update.ini` is unchanged.
- With `NO_STARTUP_DIALOG.txt`, nothing is shown and `update.ini` is unchanged.
- With `check_for_updates` false or missing, no fetch is made.
