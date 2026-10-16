<!--
SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Isolated BiasedConv2dRelu experiment

`BiasedConv2dReluBenchmark` extracts all 22 fused Conv+ReLU nodes from the pinned
Landscape definition. It preserves each input/filter/output shape, group count,
stride, dilation, padding, FP32 type and actual model weights. Each case has its
own initialized operators and dedicated buffers. Production dispatch is unchanged.

The default hardware comparison is DirectML `NONE` against
`DISABLE_META_COMMANDS`. Both use NCHW, OWNED_BY_DML filter/bias and fused ReLU,
matching production apart from the explicit MetaCommand control. No half precision
flag or storage change is tested.

`--custom` adds six experimental multiplier-one Depthwise implementations:
linear dispatches with 64/128/256 threads, and groupshared tiles of 16x4, 16x8 and
32x8 outputs. Shapes/strides/dilations/padding are compile-time shader constants.
The tiled variants load an input tile including its halo once per group. Both
versions use precise FP32 accumulation and add bias before ReLU.

## Correctness

The program constructs a Conv -> ReLU ONNX model with nanopb, then evaluates it
using WinML CPU. This is the independent reference, not a hand-written CPU kernel.
It compares every output element for bounded signed FP32 inputs, a larger signed
ramp and zeros. The bound is unchanged: 2e-5 absolute + 2e-5 relative. Nonfinite
outputs fail. Verification precedes timing for every enabled implementation.

CTest runs the two DirectML implementations on hardware for all 22 nodes. A
separate test runs all enabled implementations and shapes on WARP. WARP is only
a software correctness diagnostic; it is not a production fallback or a source
of hardware performance measurements.

## Measurement

```powershell
cmake --build build --config RelWithDebInfo --target BiasedConv2dReluBenchmark
ctest --test-dir build -C RelWithDebInfo -R BiasedConv2dReluBenchmark --output-on-failure
build/tests/DmlProgram/RelWithDebInfo/BiasedConv2dReluBenchmark.exe --output build/conv-relu-run
```

`--node 230` selects one original Node index. `--verify` skips timing. `--warp`
selects the software adapter. `--custom` is opt-in and not enabled by default.

Each case initializes and validates before the measured command list. Ten warmup
samples and 50 measured GPU timestamp pairs are recorded per implementation.
Implementations rotate order each iteration and reuse the same nonzero input.
Uploads, initialization, readback and CPU waits are outside each timed interval.
A UAV barrier before each timestamp pair orders previous output/temp accesses;
this synchronization is excluded from the interval. These are hot repeated
isolated dispatches, not a prediction of full-model latency or end-to-end time.

`conv-relu-cases.csv` records original Node IDs and maximum numerical error.
`conv-relu-samples.csv` records raw GPU milliseconds. Repeat whole processes to
check run-to-run variation. PIX can capture the same executable; native DML
operator regions and custom Dispatch events can be matched to case/iteration
order. Do not benchmark concurrently with PIX replay or CTest.

## Initial custom-shader failure

The initial hardware custom implementation using root StructuredBuffer UAVs
caused DXGI_ERROR_DEVICE_HUNG on UHD 630. DirectML verification preceding that
custom dispatch completed. The custom access was changed to ByteAddressBuffer;
software verification does not establish that the hardware issue is resolved.
Hardware custom dispatch requires a separately approved retry. The default
hardware tests do not execute it. `CONV_RELU_DEBUG=1` enables the D3D12 debug layer
for diagnosis; do not use debug-layer timings as performance results.


An explicitly approved hardware retry of the ByteAddressBuffer version also
reproduced the hang at Node 230/direct-64. The subsequent source clamps all input
read addresses before applying the zero-padding mask. Its WARP agreement does
not establish that hardware execution is safe; the exact clamped version initially awaited renewed approval. Root cause remains
unresolved. See build/conv-relu-experiment/REPORT.md and retry logs.


The user subsequently approved the clamped Node 230 hardware retry. It passed
WinML comparison for all eight enabled implementations on UHD 630. Three mixed
runs selected the 16x8 shared-memory tile as the fastest candidate, with a pooled
median of 0.194 ms versus native DML 0.721 ms in those same mixed runs. Absolute
native-only timings vary, so this is not a full-model speedup claim. See
build/conv-relu-experiment/NODE230-APPROVED-REPORT.md for raw results and limits.
Other depthwise shapes remain software-verified only. Production dispatch remains
unchanged.


## Error dumps

`--verify --dump --custom --node 230 --output <directory>` saves the three
verification inputs, WinML CPU references and per-implementation FP32 outputs.
This adds CPU file output only; GPU kernels, binding and submission order are
unchanged. `--input <planar-f32-file>` requires `--node` and replaces the synthetic
scenarios with one saved intermediate tensor of the exact required byte size.
The verification bound remains unchanged. Dumping is outside timed dispatches.

The error evaluation includes synthetic signed inputs and the saved FP32
Node 227 output for the portrait fixture. This measures local Node 230 error;
it does not measure downstream accumulation or final-mask differences.
See build/conv-relu-experiment/ERROR-REPORT.md.


## Thread-local vertical strips

The custom benchmark also tests 2, 4 and 8 consecutive output rows per thread,
with 64/128/256 threads per group, for 3x3 stride-one/dilation-one depthwise
operators. Adjacent lanes handle adjacent x coordinates. Rolling input rows and
filter coefficients are local to each thread; no shared memory or group barrier
is used. Raw input addresses remain clamped before the zero-padding mask. The
nine-tap accumulation order remains unchanged for each output.

The new version was software-validated first, then explicitly approved for
Node 230 hardware testing. All 17 methods passed on that node, including the
saved portrait activation. The new strip outputs numerically match direct-64;
worst local WinML RMSE is 4.482e-7 and maximum absolute error is 7.630e-6.
CTest passes all 45 tests; the WARP test exercises all eligible shapes.

Three mixed runs (150 GPU timestamp samples per method) measured the best
strip-8-64 at mean/median/p95 0.098810/0.094000/0.135212 ms, versus shared-memory
tile-16x8 at 0.158297/0.149958/0.215378 ms. The median reduction is about 37%.
These are isolated operator timings, not a full-model speedup. Register pressure,
occupancy and spills were not measured. Production dispatch is unchanged.
Raw logs, dumps and detailed results are in build/conv-relu-strip/REPORT.md and
analysis.json.


## Production integration: Node 230

The isolated experiment above is now integrated as `MPLDDepthwiseConv2dRelu`.
GenerateModel.py replaces only the matching fixed FP32 depthwise Conv+ReLU
(Node 230 in the pinned model, original ONNX #140 Conv -> #141 Relu).
Input/output are [1,16,72,128], weights [16,1,3,3], bias [16], stride/dilation 1
and padding 1. The compiler validates this fixed contract. The following Add,
final ConvTranspose and Sigmoid remain separate. UAV barriers are unchanged.
`--no-mpld-depthwise-conv` reproduces the native baseline byte for byte.

Windows build and all 47 CTest cases pass, including full-model WinML CPU/GPU
comparisons with the existing tolerance. On portrait the final output has
RMSE 7.15e-8 against WinML CPU, with no threshold-0.5 mask disagreements.

For full inference, use the opt-in WinMlComparison test case
`MediaPipeLandscapeNode230Timing` (`--no-skip`). It reconstructs the native node,
initializes both runtimes and alternates execution order within one process.
`DML_TIMING_CSV` specifies the raw paired-sample CSV path. Both methods use the
same portrait input. Three runs, 200 samples/method each, measured:

| Model | Mean ms | Median ms | p95 ms |
|---|---:|---:|---:|
| Native Node 230 | 6.147 | 5.934 | 7.742 |
| Thread-local CS | 5.859 | 5.678 | 7.031 |

These are CPU wall times for upload, submission, GPU execution, completion wait
and readback, excluding initialization, mask correction and rendering. Mean and
median reductions are 4.69% and 4.32%. They are not pure GPU timings. Earlier
separate-process runs had substantial outliers; all raw results and the full
limitations are retained in build/node230-integration/REPORT.md.
