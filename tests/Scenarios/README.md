<!--
SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>

SPDX-License-Identifier: Apache-2.0
-->

# Acceptance test scenarios for obs-backgroundremoval

- **Debian:** Place the plugin where OBS can load it, then run
  `+run-debian.sh run-all`.
- **Windows:** ST-2000 runs only on GitHub Actions, from
  `.github/ci_scripts/run_scenario_tests_windows.ps1` in the Windows build job.
  The job installs OBS Studio with winget, the script installs the generated
  plugin ZIP into `C:\ProgramData` and uses the runner's own OBS profile, and
  the job always uploads the OBS logs. Do not run the script on your own
  computer, because it overwrites your OBS profile and installed plugin.
  This GUI path is separate from CTest's source-built libobs/D3D11 runtime.
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
