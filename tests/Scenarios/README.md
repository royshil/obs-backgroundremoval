<!--
SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>

SPDX-License-Identifier: Apache-2.0
-->

# Acceptance test scenarios for obs-backgroundremoval

- **Debian:** Place the plugin where OBS can load it, then run
    `+run-debian.sh run-all`.
- **Windows:** Build the plugin, install OBS Studio, then run
    `+run-windows.ps1` from PowerShell. The runner finds the standard OBS Studio
    installation and the `release/obs-backgroundremoval` build automatically.
    Use `-ObsExecutable` and `-PluginRoot`, or the matching
    `GUI_SCENARIO_OBS_EXECUTABLE` and `GUI_SCENARIO_PLUGIN_ROOT` environment
    variables, for non-standard locations. Tests use an isolated OBS portable
    configuration and do not modify the user's OBS profile or installed plugins.
    Windows CI builds the plugin but does not run ST-2000.
