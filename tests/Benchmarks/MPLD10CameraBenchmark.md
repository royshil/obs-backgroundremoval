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
initial inference. `--animated` alternates two preloaded complementary textures
once per video tick to exercise sustained inference without camera or upload
variability. The selected texture stays unchanged across renders within a tick.
This deterministic workload does not represent a natural camera scene.

```powershell
./build/tests/Benchmarks/RelWithDebInfo/MPLD10CameraBenchmark.exe `
  --synthetic --output build/pipeline-smoke-unique --seconds 1
```

Launch the executable under PIX Timing Capture, passing the same arguments.
Use the production Engine TraceLogging scopes and the native D3D11/D3D12 queue
timeline to inspect the typical path. Capture the warmed-up measurement interval.
Keep `--force11on12` disabled for native performance measurements.
The installed OBS plugin remains the final reference for full OBS behavior.

Run PIX from an elevated PowerShell session: on the tested Windows/driver setup,
normal-user captures completed successfully but contained no GPU executions.
For PIX versions that reject `--command-line`, configure the benchmark through
inherited environment variables and launch it without arguments. CLI options
override these defaults. Start PIX before the target creates its D3D devices;
check that the resulting capture actually contains GPU executions.

```powershell
$env:MPLD10_BENCHMARK_INPUT = 'animated' # 'static' also supported
$env:MPLD10_BENCHMARK_OUTPUT = "$PWD/build/pipeline-pix-unique"
$env:MPLD10_BENCHMARK_SECONDS = '90'
$exe = (Resolve-Path build/tests/Benchmarks/RelWithDebInfo/MPLD10CameraBenchmark.exe).Path
& 'C:/Program Files/Microsoft PIX/2603.25/pixtool.exe' launch --timing $exe `
  take-new-timing-capture "$PWD/build/pipeline-pix-unique.wpix" --duration=30000
Remove-Item Env:MPLD10_BENCHMARK_INPUT, Env:MPLD10_BENCHMARK_OUTPUT, Env:MPLD10_BENCHMARK_SECONDS
```

Keep the target alive longer than the capture and its stop processing; otherwise
PIX may abort with `PIXTOOL18`. PIX terminates its launched target on completion,
so callback CSV/JSON may not be finalized in this run. Collect CPU callback
statistics in a separate uninstrumented run, and use the completed `.wpix` for
GPU timing. Allow for initialization, then select a steady interval (for example,
capture seconds 15–30) and verify sustained executions near the configured fps.

`samples.csv` contains frame timestamps, CPU tick/render durations and QPC
callback boundaries. `conditions.txt` records the source, frame rate, dimensions,
settings and process ID. Google Benchmark reports the recorded mean callback
duration with median and p95 counters and writes `benchmark.json`. These CPU
durations are not GPU execution time; use PIX for GPU timing.
Use a fresh output directory for each run.
