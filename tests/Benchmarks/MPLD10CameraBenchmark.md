<!--
SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>

SPDX-License-Identifier: Apache-2.0
-->

# Profile the production Landscape pipeline

`MPLD10CameraBenchmark` links the production Pipeline and Engine into libobs.
The normal OBS graphics thread drives `video_tick` and `video_render`.
Inference gating, texture transfer, refinement and final drawing use the
production implementation.

Build this manual profiling target explicitly:

Real-camera profiling uses an OBS installation with win-dshow, selected by
`OBS_PROFILE_BIN_DIR`. This is separate from the source-built libobs runtime used
by CTest; it is not a CTest dependency.

```powershell
cmake --build build --config RelWithDebInfo --target MPLD10CameraBenchmark
$env:PATH = 'C:/Program Files/obs-studio/bin/64bit;' + $env:PATH
./build/tests/Benchmarks/RelWithDebInfo/MPLD10CameraBenchmark.exe --help
```

For a real camera, supply either the camera settings JSON or an OBS scene
collection containing exactly one `dshow_input`. The file is read without
changing the OBS configuration. Close other applications using the camera first.

```powershell
./build/tests/Benchmarks/RelWithDebInfo/MPLD10CameraBenchmark.exe `
  --camera-settings camera.json --output build/camera-profile-unique `
  --seconds 30 --fps 30 --sensitivity 60 --preview
```

The canvas is 1280x720 NV12. Mandatory mask refinement uses threshold 0.5 and
background blur is disabled. After the first frame renders, the tool warms up
for three seconds, then records
the requested interval (1 to 90 seconds). It stores timings and conditions,
without recording camera images or audio.

For an unattended launch check, `--synthetic` supplies a static pattern instead
of opening a camera. A static pattern exercises inference gating after the
initial inference; use a moving camera scene for steady inference analysis.

```powershell
./build/tests/Benchmarks/RelWithDebInfo/MPLD10CameraBenchmark.exe `
  --synthetic --output build/pipeline-smoke-unique --seconds 1
```

Launch the executable under PIX Timing Capture, passing the same arguments.
Use the production Engine TraceLogging scopes and the native D3D11/D3D12 queue
timeline to inspect the typical path. Capture the warmed-up measurement interval.
Keep `--force11on12` disabled for native performance measurements.
The installed OBS plugin remains the final reference for full OBS behavior.

`samples.csv` contains frame timestamps, CPU tick/render durations and QPC
callback boundaries. `conditions.txt` records the source, frame rate, dimensions,
settings and process ID. Google Benchmark reports the recorded mean callback
duration with median and p95 counters and writes `benchmark.json`. These CPU
durations are not GPU execution time; use PIX for GPU timing.
Use a fresh output directory for each run.
