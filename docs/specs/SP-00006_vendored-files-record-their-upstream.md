---
# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0
---

# SP-00006: Vendored files record their upstream

- Kind: Design principle
- Platforms: All

## Principle

Every third-party file copied into the repository unchanged has a `.upstream` file next to it that
says exactly where it came from. A file that differs from its upstream records its changes in the
file itself and has no `.upstream` file.

## Rationale

A copied file outlives the memory of where it came from. With an exact record, anyone can check the
copy against its source and update it to a newer release, without trusting the commit history. A
record is only exact when the copy is unchanged; once a file is modified, the place to say how is
the file itself.

## Requirements

1. A vendored file `<name>` lives under a `vendor` directory, such as `vendor/<project>/` for the
   plugin or `tests/vendor/<project>/` for tests only, with `<name>.upstream` next to it. The file
   is byte-identical to `Path` at `Commit` in `Repository`.
2. `<name>.upstream` is plain text, one `Key: value` per line, with these keys:
    - `Repository`: the URL of the upstream repository. Required.
    - `Commit`: the full commit hash the file was taken from, followed by ` # <tag>` when the commit
      is a release tag. Required.
    - `Path`: the file's path in the upstream repository at that commit. Required.
    - `License` and `Authors`: when the upstream terms require naming them and they are not recorded
      elsewhere. Optional.
3. `Modifications` and `Scope` are forbidden. A modified file states its changes in the file itself,
   where they cannot be separated from it, and has no `.upstream` file; the sidecar only ever
   describes an unchanged copy. A scope is ambiguous by nature: the record names the one file it
   describes and nothing else.
4. The license of the vendored file is declared for REUSE next to it, as `<name>.license` or the
   upstream license file in the same directory. The `.upstream` records themselves are covered by
   the `**/*.upstream` annotation of the root `REUSE.toml`.
5. A vendored file with a `.upstream` file is never edited in place. Updating to a new upstream
   release replaces the file and its `Commit` together.

## Example

`tests/vendor/doctest/doctest.h.upstream`:

```text
Repository: https://github.com/doctest/doctest
Commit: 2d0a9359a60c51affe2a9bebb1be1dca47868151 # v2.5.3
Path: doctest/doctest.h
```
