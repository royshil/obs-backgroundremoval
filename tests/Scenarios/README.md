<!--
SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>

SPDX-License-Identifier: Apache-2.0
-->

# Acceptance test scenarios for obs-backgroundremoval

- **Debian:** Place the plugin where OBS can load it, then run
  `+run-debian.sh run-all`.
- **Windows:** Run `+run-windows.ps1` from PowerShell after configuring the
  project. Set `PLUGIN_CONFIG` when the build configuration is not
  `RelWithDebInfo`.
