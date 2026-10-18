<!--
SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>

SPDX-License-Identifier: GPL-3.0-or-later
-->

# DirectML output verification

`WinMlComparison` checks the production operators and the generated Landscape model
against Windows ML. CPU and GPU reference sessions use the original pinned ONNX,
not an ONNX reconstructed from the lowered DirectML graph.

The full-model cases cover zeros, constant gray, RGB gradients, deterministic
random pixels and the portrait fixture. Every output element must be finite and
satisfy `abs(actual - reference) <= 2e-5 + 2e-5 * abs(reference)`.
The mask case also checks mandatory refinement against the CPU mask reference.

The original model must be available at `MEDIAPIPE_LANDSCAPE_ONNX`. CMake checks its
SHA-256 against revision `2497d5bec26c626c7b3c4edc6e1fefc21b64f6c3`.

Before configuring Windows tests, run `cmake -P scripts/windows/build-test-obs.cmake`.
This builds libobs and its D3D11 backend from the pinned OBS submodule, using the
dependency archive and hash in that submodule's buildspec.json. CTest uses this
runtime and its data files, not an installed OBS application. Missing runtime or
model files fail configuration. OBS application scenario tests are separate.

For MSVC 19.51, the runtime build disables optimization only for the pinned
OBS `matrix4.c`: its optimized identity-view matrix multiplication produces
incorrect results with this compiler. `LandscapeMaskEffect` checks that matrix
identity explicitly, in addition to its unchanged pixel comparisons.

```powershell
cmake --build build --config RelWithDebInfo --target WinMlComparison
ctest --test-dir build -C RelWithDebInfo -R '^WinMlComparison\.' --output-on-failure
```

The other retained tests cover runtime definitions and resource reuse
(`DmlProgramTest`), GPU texture transfer and inference (`MPLD10EngineTest`),
final alpha/blur output and repeated renders through the actual filter
(`GpuImageInferenceTest`), and inference-gate history
(`MPLD10DifferenceHistoryTest`).

Run the complete suite with:

```powershell
ctest --test-dir build -C RelWithDebInfo --output-on-failure
```

Performance analysis uses the production Pipeline through libobs; see
[MPLD10CameraBenchmark.md](MPLD10CameraBenchmark.md).
