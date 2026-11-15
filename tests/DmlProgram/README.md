<!--
SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>

SPDX-License-Identifier: Apache-2.0
-->

# DirectML correctness tests

`OperatorFixtureTest` compares five fixed inputs per operator with checked-in
little-endian FP32 outputs under `tests/fixtures/operators`. NumPy generates
those references offline from mathematical definitions, without WinML or DirectML.
Tests only read them. Absolute and relative tolerances are both 2e-5; contract
and mask-postprocessing checks remain in the same executable.

`GpuImageInferenceTest.PortraitFixture` compares the complete model mask with
the visually accepted portrait alpha fixture using IoU and pixel disagreement.
Rendering and initialization retain their separate checks. No ONNX download or
WinML runtime is required for CTest. Performance measurement lives in
`tests/Benchmarks` and uses Google Benchmark.

To regenerate operator references deliberately, use Python with NumPy:

```powershell
python tests/fixtures/generate_operator_fixtures.py
```

Review changed reference values before accepting them; never regenerate them
automatically to make a failing test pass. The original portrait and its accepted
cutout use CC-BY-NC-SA-4.0; operator references use Apache-2.0.
