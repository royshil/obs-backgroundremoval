<!--
SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>

SPDX-License-Identifier: Apache-2.0
-->

# Background Removal property preview

This Windows-only manual target displays the production `FilterContext::getProperties()`
with the pinned OBS frontend's `OBSPropertiesView` and production defaults. OBS's
widgets handle sliders, checkable groups, property callbacks and settings updates;
Restore Defaults resets the in-memory settings. About opens the shared AboutDialog.
Automatic motion suppression opens the production calibration dialog. This
standalone runner has no live pipeline, so measurement is unavailable there.
In OBS, measurement collects only a three-second maximum and sample count.
Preview applies the recommendation temporarily; Cancel restores the original
value, including whether a user value was present. Apply keeps the recommendation.
Pipeline defaults to Auto, which resolves to the MediaPipe Landscape DirectML
pipeline at runtime. Explicit selection takes precedence.
The runner does not start video, network update checks or persistent configuration.

The build reuses widget sources from the OBS submodule. Build-tree copies of
`properties-view.cpp` and `qt-wrappers.cpp` replace only the `obs-app.hpp` include
with `PropertyUIApp.hpp`, which supplies frontend translation, theme and dialog
parent services. The submodule is unchanged; review this adapter when updating OBS.

Run from the repository root after the normal Windows configure step:

```powershell
cmake --build build --config RelWithDebInfo --target BRPropUI
$env:PATH = "$PWD/.deps/obs-deps-qt6/bin;$PWD/build/obs-test-runtime/rundir/RelWithDebInfo/bin/64bit;$PWD/.deps/obs-deps/bin;" + $env:PATH
$env:QT_PLUGIN_PATH = "$PWD/.deps/obs-deps-qt6/plugins"
./build/tests/UI/RelWithDebInfo/BRPropUI.exe Yami_dark
./build/tests/UI/RelWithDebInfo/BRPropUI.exe Yami_light
```

Like `AboutDialogRunner`, this runner accepts `Yami_dark` (the default) or
`Yami_light` and shares `tests/UI/palette.hpp` for its palette and stylesheet.

The source-built libobs path above assumes the default `OBS_TEST_BIN_DIR`; adapt
PATH if that CMake cache setting differs. `--smoke-test` checks that the property
groups render and closes the window automatically; adding `--screenshot <file.png>`
saves the rendered dialog before closing. This executable is excluded from the
default build and CTest.
