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
  Windows CI installs OBS with winget in the build job, extracts the generated
  plugin ZIP, runs ST-2000 (including release builds), and always uploads logs.
  This GUI path is separate from CTest's source-built libobs/D3D11 runtime.
  Use `-CaseFilter 'ST-2000.1-ST-2000.4*'` for the About UI actions only;
  an unmatched filter fails instead of succeeding without running a scenario.
- **macOS:** Run `bash tests/Scenarios/+run-macos.sh` on the Mac used for testing.
  Accessibility permission is required. Windows cannot validate the macOS runner.

## Windows filter properties (ST-2100)

`2100-windows-filter-properties.feature` describes the current grouped properties
and automatic motion-suppression dialog. These are Windows-specific and do not
describe the separate macOS filter UI. For the lightweight local checks:

```powershell
cmake --build build --config RelWithDebInfo --target BRPropUI
./build/tests/UI/RelWithDebInfo/BRPropUI.exe --smoke-test
./build/tests/UI/RelWithDebInfo/BRPropUI.exe --lifecycle-test
```

Use CMake from the configured Visual Studio environment. Put the source-built
OBS runtime (`build/obs-test-runtime/rundir/RelWithDebInfo/bin/64bit`) and the Qt
runtime (`.deps/obs-deps-qt6/bin`) on PATH before running BRPropUI.
The smoke test uses the pinned OBS property widgets and simulated motion samples;
it checks dialog controls and Apply/OK/Cancel without a camera. The lifecycle test
uses libobs/D3D11. Neither is a packaged-plugin OBS end-to-end test or a CI target.
In OBS itself, confirm the same scenario with an active video source, including
measurement of real video and the refreshed property value after Apply/OK.
