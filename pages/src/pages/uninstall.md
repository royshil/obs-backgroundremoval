---
# SPDX-FileCopyrightText: 2021-2026 Roy Shilkrot <roy.shil@gmail.com>
# SPDX-FileCopyrightText: 2023-2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: GPL-3.0-or-later

layout: ../layouts/MarkdownLayout.astro
pathname: uninstall
lang: en
title: Uninstalling the plugin
description: Uninstalling the plugin
---

# Uninstalling the Plugin

Follow the instructions below to remove OBS Background Removal from your system.

---

## Windows

Delete the following files and directories from your OBS installation directory:

- `data\obs-plugins\obs-backgroundremoval` (directory)
- `obs-plugins\64bit\obs-backgroundremoval.dll` (file)
- `obs-plugins\64bit\obs-backgroundremoval` (directory)

---

## macOS

Remove plugin files:

```sh
rm -rf \
  ~/Library/Application\ Support/obs-studio/plugins/obs-backgroundremoval.plugin \
  ~/Library/Application\ Support/obs-studio/plugins/obs-backgroundremoval.plugin.dSYM
```

Remove registered package info:

```sh
pkgutil --volume ~ --forget com.royshilkrot.obs-backgroundremoval
pkgutil --volume ~ --forget "'com.royshilkrot.obs-backgroundremoval'"
```

---

## Ubuntu

Uninstall the package:

```sh
sudo dpkg -r obs-backgroundremoval
```
