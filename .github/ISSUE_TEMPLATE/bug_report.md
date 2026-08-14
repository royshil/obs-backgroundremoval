---
# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

name: Bug report
about: Report a reproducible bug
title: 'what happened? write here briefly'
labels: bug
assignees: umireon
---

<!--
Thank you for reporting a bug.

Feature requests and general questions belong in GitHub Discussions:
https://github.com/royshil/obs-backgroundremoval/discussions

Please search existing issues before submitting a new report.
-->

## OS (Windows, macOS, Ubuntu, Debian, Arch, etc)

<!-- Write your OS here. -->

## Hardware (x64, arm64, 5070 Ti, 32GB RAM, M1 Pro, etc)

<!-- Write information of your hardware here. -->

## OBS Version

<!--
You must provide exact version of OBS Studio here.
Versions earlier than 31.1.1 are not supported and do not ask here to fix for such older versions.
-->

## Installation method

<!--
Write here how you obtained and installed the plugin.
Note that installing under `C:\Program Files` is no longer supported and issues like that will be closed immediately without confirmation.
-->

## How can we reproduce it?

1. Add a video source attached to my camera.
2. Add a Background Removal filter to this video source.
3. Turn on the advanced mode on the BR filter.
4. Set the feathering factor to -42.
5. Crashed. It won't start again without OBS's safe mode.

## Logs or crash report

When you attach any log or crash report to this issue, please mark the following checkbox:

- [ ] I confirm that the logs and the crash reports I upload do not contain personal information, and I agree to share their contents publicly with the community.

<!--
Attach log files or crash reports, or paste their contents here.

OBS logs:
- Windows: `%APPDATA%\obs-studio\logs`
- macOS: `~/Library/Application Support/obs-studio/logs`
- Linux: `~/.config/obs-studio/logs`
- Linux (Flatpak): `~/.var/app/com.obsproject.Studio/config/obs-studio/logs`

Crash reports:
- Windows: `%APPDATA%\obs-studio\crashes`
- macOS: Find the `.ips` report in Console > Crash Reports and compress it as a `.zip` file.
- Linux: Save the output of `coredumpctl info obs` or other available crash output as a `.txt` file.
-->
