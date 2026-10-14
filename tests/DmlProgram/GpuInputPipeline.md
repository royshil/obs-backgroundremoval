<!--
SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
SPDX-License-Identifier: GPL-3.0-or-later
-->

# GPU input and gated DirectML execution

The transfer and image-gate implementations live in the dedicated
`src/BackgroundRemoval/windows/MediaPipeLandscapeDirectMLPipeline/` directory
and `BackgroundRemoval::MediaPipeLandscapeDirectML` namespace. Their image
layout and PSNR policy belong to this model, rather than common ObsBridgeUtils
utilities. Only DmlProgram exposes the model-independent GPU execution API.

`MediaPipeLandscapeDirectMLPipeline` uses `D3D12TextureReader` instead of the CPU
`TextureReader`. The existing OBS RGB extraction still produces a 256 x 432 R32F
texture containing the three 256 x 144 planes. No input pixels are mapped,
copied to CPU arrays, or uploaded again for inference.

## Transfer and synchronization

The reader obtains OBS's D3D11 device and input texture through
`gs_get_device_obj()` and `gs_texture_get_obj()`. Both are exported in OBS 29's
public `graphics.h`, though absent from its web API reference. No private OBS
structures are accessed. The reader verifies the OBS adapter LUID matches the
DirectML device, and requires ID3D11Device5/ID3D11DeviceContext4 fence support.

The D3D12-created NT-shared R32F texture is opened on the OBS device. In the OBS
graphics context, its immediate context waits for previous D3D12 consumption,
then copies the rendered input directly into that texture, signals the shared
ready fence and flushes. Rendering and copying use the same D3D11 command stream.
D3D12 waits for readiness on its queue and signals consumption after submission.
The texture is in COMMON state at both boundaries. There is one image copy,
no independent D3D11 device, no intermediate texture and no keyed mutex handoff.
Unsupported sharing/fence support, mismatched adapters or a source from another
device produces an error, without a CPU input fallback. Use the reader on one
thread within the OBS graphics context; complete its consumer submission before
reuse/destruction.

## GPU decision and execution

`GpuImageInference` records two custom compute dispatches on the runtime's D3D12
direct queue. The first packs the texture into a Float32 buffer and reduces
squared error against the last successfully committed input. The second reduces
those sums and compares MSE to `10^(-PSNR_threshold / 10)`. All RGB values are
included. Identical images skip for finite thresholds. Float32 reductions can
differ slightly from the previous double-precision CPU sum near a threshold.

The decision is stored as a 64-bit 0/1 value, with a separate invalid-input flag.
`BaselineDmlProgramRuntime::recordGpuRun` applies EQUAL_ZERO predication: zero
suppresses input-buffer copies, DirectML dispatches and output readback copies.
Resource transitions and the resource-specific UAV barriers in the static
dependency plan still execute to keep state valid on both branches. Initialization and weight upload are always performed normally.

The first frame, settings changes and recovery from failure force inference.
The GPU packet contains a 16-byte decision/error header followed by the model
output. One submission-completion wait precedes CPU access to this packet.
The header is always copied; the output is copied only on inference frames.
There is no PSNR readback or CPU decision before inference. Skipped frames leave
the caller's output vector and the previous mask unchanged.

The CPU converts inferred output to the existing uint8 mask and runs the existing
threshold/component/smoothing/resize/morphology operations. Only after those
succeed does `commitReference()` swap the current and reference GPU buffers and
the pipeline update its OBS alpha texture. A failure forces inference next frame
and retains the old comparison reference. No extra reference-copy submission is
needed for the swap.

The runtime's CPU byte-span API remains available for tests and other callers.
CPU input upload buffers are allocated lazily on its first CPU `run`. GPU-only
callers use padded buffers on the same device and queue; they must complete their
submission before reusing the runtime or releasing its resources.

## Tests

With `OBS_TEST_BIN_DIR` set to an installed OBS `bin/64bit`, CTest registers:

- `GpuImageInferenceTest.GpuImageGate`: real OBS sharing, input values, partial
  thread groups, first/forced/skip decisions, accumulated changes, all RGB planes,
  reference commit behavior, invalid input and recovery.
- `GpuImageInferenceTest.PredicatedDirectML`: pipeline-statistics Query checks
  that predicate zero causes **zero CS invocations** inside DirectML execution;
  predicate one executes and returns the expected output.
- `GpuImageInferenceTest.GpuLandscape`: the portrait fixture and its inverted
  image compare the full model's GPU-input result against its existing CPU-input
  result, and check skipped-frame output retention. This complements the existing
  full-model WinML comparison in CTest.

The standard D3D12 debug layer is enabled when installed, and error-severity
messages fail these tests. Performance measurement disables that instrumentation.

```powershell
cmake --build build --config RelWithDebInfo
ctest --test-dir build -C RelWithDebInfo --output-on-failure
```

## Performance measurement

Run the full-model stage benchmark explicitly, from a new output directory:

```powershell
$env:PATH = 'C:/Program Files/obs-studio/bin/64bit;' + $env:PATH
$gpuRunDir = Join-Path (Resolve-Path build) ('gpu-input-performance-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $gpuRunDir | Out-Null
Push-Location $gpuRunDir
try {
    & ../tests/DmlProgram/RelWithDebInfo/GpuImageInferenceTest.exe --test-case=GpuLandscapePerformance --no-skip --benchmark-no-debug
} finally {
    Pop-Location
}
```

It writes `gpu-landscape-benchmark.csv`. CPU/GPU order alternates, with 20 warmup
pairs and 100 measured pairs per scenario and a 10 ms pause between pairs. Both
methods use the portrait input and the same model. The CPU baseline includes
TextureReader, CPU PSNR and, when required, input upload/inference/output readback.
The GPU method includes shared copies/synchronization, GPU packing/decision,
conditional inference and combined mask/flag readback. The old D3D11 scalar
prototype is not the CPU baseline. Source rendering, RGB extraction and CPU mask
refinement are excluded from both sides. Finish builds and other GPU workloads
before collecting timing.

On 2026-10-14, Intel UHD Graphics 630 (driver 27.20.100.8681), RelWithDebInfo,
with the OBS application stopped and D3D12 validation disabled, the full-model
stage comparison produced:

| Scenario | Method | Mean ms | Median ms | p95 ms |
| --- | --- | ---: | ---: | ---: |
| Inference skipped | CPU input/PSNR | 0.821 | 0.794 | 1.389 |
| Inference skipped | D3D12 GPU input/gate | 3.459 | 3.375 | 4.082 |
| Inference required | CPU input/PSNR/DirectML | 8.895 | 8.716 | 9.982 |
| Inference required | D3D12 GPU input/gate/DirectML | 8.504 | 8.375 | 9.491 |

The GPU path reduced mean inference-frame time by about 4.4%, but increased
skipped-frame time. Predication suppresses model dispatches, not all command-list
state changes and resource barriers. Shared transfer and synchronization also
remain. These measurements do not establish an overall OBS performance gain;
the relative frequency of inference and skipped frames matters. Raw samples and
summary are saved in `build/gpu-input-performance-20261014-061128/`.

The portrait GPU-input/CPU-input output comparison had maximum absolute error
zero. The standalone predication test measured zero CS invocations for decision
zero and 256 invocations for decision one on its small Relu graph.

TraceLogging scopes include `Pipeline.D3D12SharedCopy`,
`Pipeline.GpuGateAndInference`, `Pipeline.GpuOutputWait`, and
`Pipeline.MaskAndFlagReadback`, plus the existing frame/refinement and
inference-decision events. The decision event is emitted after the GPU packet is
read, rather than before CPU-scheduled inference.


### Skipped-frame investigation (2026-10-14)

Pass `--profile-gpu` to the opt-in benchmark to collect CPU phase timings and
D3D12 timestamps. Profiling is disabled in production. GPU durations are portions
of CPU wait time, not additional time to sum with CPU phases.

The profiled baseline averaged 3.500 ms over 100 skipped frames: capture CPU
0.297 ms, command recording 0.735 ms, submission 0.042 ms, completion wait
2.398 ms, readback 0.015 ms. Within the GPU list, packing cost 0.120 ms,
gating 0.014 ms, the predicated DirectML section 1.827 ms, and flag copy
0.009 ms. Zero model CS invocations do not imply a free model command sequence:
state changes and UAV barriers remain unconditional.

Completed command lists are now reused when texture, input/reference buffers,
initial states, threshold, force flag and profiling mode match. Pixel contents
and the predicate are evaluated afresh. Changing those parameters re-records the
list. This removed steady-state CPU recording cost (0.0004 ms), but the second
run averaged 3.453 ms (median 3.184 ms, p95 5.307 ms), versus baseline median
3.546 ms and p95 4.177 ms. Capture rose to 0.338 ms, waiting to 3.048 ms,
and the GPU DirectML section to 2.034 ms. The CPU baseline also slowed from
0.907 to 1.190 ms between runs. These sequential measurements establish reduced
CPU recording work, but not a reliable total latency improvement. Recording
previously overlapped GPU transfer; removing it can increase measured waiting.

Raw CSVs and logs: `build/gpu-skip-baseline-20261014-064515/` and
`build/gpu-skip-reuse-20261014-064625/`. Model synchronization remains intact:
removing barriers indiscriminately would also affect frames that run inference.
Further reduction requires a validated synchronization redesign or a change in
the GPU-only scheduling policy; Map optimization is not the main opportunity.


### Direct OBS D3D11 copy comparison (2026-10-14)

The old and new executables were measured sequentially after the build finished,
with profiling enabled and D3D12 validation disabled on Intel UHD Graphics 630.
Each scenario/method uses 20 warmup iterations and 100 measured iterations.
Both versions include command-list reuse; the changed variable is the transfer
bridge. An earlier preliminary run overlapped a build and is excluded.

| GPU path scenario | Bridge | Mean ms | Median ms | p95 ms |
| --- | --- | ---: | ---: | ---: |
| Inference skipped | Two copies / separate D3D11 device | 3.731 | 3.427 | 6.175 |
| Inference skipped | Direct OBS D3D11 copy | 3.063 | 2.999 | 3.453 |
| Inference required | Two copies / separate D3D11 device | 8.627 | 8.472 | 8.989 |
| Inference required | Direct OBS D3D11 copy | 8.667 | 8.213 | 12.152 |

Skipped-frame mean decreased about 17.9%. Its CPU capture phase decreased from
0.423 to 0.192 ms. Inference-frame mean was essentially unchanged (+0.5%);
its CPU capture phase decreased from 0.433 to 0.235 ms. Timing varies across
sequential runs, and this does not measure full OBS frame rendering performance.
Source rendering, RGB extraction and CPU mask refinement are excluded.
The skipped DirectML GPU section remains 1.864 ms in the new run.

The new bridge passed the GPU gate/partial-group/threshold/force/recovery tests,
zero-dispatch predication test, and full Landscape portrait/changed-image
comparison with the CPU-input DirectML path. These tests use a real libobs D3D11
backend. They do not establish OBS 29 runtime compatibility: OBS 29 API source
availability was checked separately; the installed OBS runtime was used here.

Raw samples, logs and phase means are in
`build/gpu-direct-copy-comparison-20261014-082748/{before,after}/` and
`build/gpu-direct-copy-comparison-20261014-082748/summary.json`.


### Compile-time dependency synchronization

The generated input specifications, nodes and Graph are constexpr. A constexpr
`makeBarrierPlan(graphNodes)` computes ordering before each node, synchronizing
a produced tensor at its first consumer. Repeated reads and duplicate operands
do not add more barriers. Runtime inputs and weights are already ordered by
transfer/initialization. Invalid forward references fail constant evaluation.
Static assertions cover an input, a producer, duplicate operands and a branch
with a previously synchronized input.

The compiler resolves planned producer node indices to tensor allocations;
reshape/broadcast descriptor views retain their allocation identity. Runtime
builds resource-specific UAV barrier descriptors once and reuses them. Non-static
test graphs without a supplied plan use the same planner during compilation.
This assumes the current variant's one-output, non-in-place operators and dedicated
output allocations; adding in-place or storage-reuse operations requires revisiting
the plan. The model does not contain a hand-maintained barrier table.

Each operator owns its temporary allocation sized for its initializer and
execution requirements. No scratch allocation is shared between operators.
Temporary sizes, GPU allocation and DirectML shader compilation remain device-
dependent initialization work, not compile-time C++ work. Initializer barriers
and input/output state transitions remain separate from the model dependency plan.

The post-change opt-in benchmark (100 measured frames per scenario) averaged
3.093 ms skipped and 8.802 ms inferred, versus the earlier direct-copy run's
3.063 and 8.667 ms. These separate runs do not show a total latency improvement.
Raw samples are in `build/gpu-constexpr-barriers-20261014-090831/`.


### Previous Base64 weight embedding experiment

The generated C++ encodes 425,692 unchanged weight bytes in Base64 chunks.
A full chunk contains 12,288 decoded bytes (12 KiB) and 16,384 ASCII characters;
there are 34 full chunks and a 7,900-byte tail. `Embedding::decodeBase64` is
consteval, with a compile-time FNV-1a check per chunk. The full decoded object
has a compile-time size/offset check so its object representation is the exact
contiguous weight byte sequence. Runtime uses a read-only byte span over that
object, with no resource lookup or decoding. The .rc artifact was removed.

MSVC 14.51 compiles it as C++20 with `/constexpr:steps2000000` applied only to
the generated translation unit. The generated source is 618,515 bytes. The
binary generation artifact's SHA256 remains
`7d728c302e0a679329dd21883478d22e2112a5d04f5ae6454bc7f6db1c693bcf`.
The built DLL contains the complete original weight sequence once in read-only
`.rdata`; the sampled encoded Base64 prefix is absent. Generator tests cover
empty weights, exact chunk boundaries, padded tails and reproducible generation.
C++ static assertions cover empty, one/two-byte padded, unpadded and binary input.


### Direct hexadecimal string embedding

The current generated C++ uses a single aligned constexpr char array initialized
by adjacent hex-escaped string literals, with 16 weight bytes per line. This
replaces the Base64 experiment: no decoder, chunk aggregation, compile-time hash
loop or weights-specific constexpr-step option is needed. A size assertion
checks the array length; the runtime span excludes the final string terminator.
Generator tests parse the emitted escapes and compare every byte with the binary
weight artifact. The previously measured Base64 timings are historical.
