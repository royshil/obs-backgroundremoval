<!-- SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Operator comparison against Windows ML

The doctest executable compares BaselineDml output directly with WinML output,
using CPU and DirectXHighPerformance sessions. GPU selection is a preference;
provider placement inside WinML is not instrumented.

Inputs are generated in C++ at runtime. There is no CPU operator reference,
Python, generated ONNX file, sidecar, or model-file I/O dependency.
`OnnxTestModel.hpp` builds one-node ONNX models with nanopb (IR 8, opset 14)
and passes encoded bytes through an InMemoryRandomAccessStream to WinML.
The nanopb dependency is confined to tests.

```powershell
cmake --build build --config RelWithDebInfo --target WinMlComparison
ctest --test-dir build -C RelWithDebInfo -R '^WinMlComparison\.' --output-on-failure
build/tests/DmlProgram/RelWithDebInfo/WinMlComparison.exe --test-case=Resize
```

Eleven operator cases have CPU/GPU subcases and five inputs: constant, signed
ramp, activation boundaries around -3/3, and first/last-corner impulses.
Each backend/input configuration compares DML with WinML (110 numerical
comparisons). All inputs are Float32 [1,2,3,4]. Nonfinite outputs or errors
greater than `2e-5 + 2e-5 * abs(WinML)` fail. Unavailable devices and unsupported
operators fail instead of skipping.

HardSwish uses the DML Add/Clip/Mul/Mul decomposition. ReduceMean reduces axes
[2,3] with keepdims using AveragePool2d. Resize uses half_pixel linear
interpolation with height scale 2 and width scale 3. Add/Mul use channel
broadcasting. Biased Conv and depthwise Conv use 3x3 filters, padding 1 and
stride 1. Biased ConvTranspose uses 2x2 filters, stride 2 and no padding.

On 2026-10-14 all 29 CTest entries passed (11 comparisons, 18 existing tests).
Failed comparisons report operator, WinML backend, scenario, maximum error,
and worst DML/WinML values.

These project tests are not official ONNX backend cases. They cover selected
finite Float32 inputs and attributes, not all shapes, strides, dilations,
padding, weights, Float16, NaN/infinity behavior, or the entire model.

## Full MediaPipe Landscape comparison

When the pinned source ONNX is present, `WinMlComparison.MediaPipeLandscape`
is registered with CTest. Set the CMake FILEPATH `MEDIAPIPE_LANDSCAPE_ONNX`
to the cached source model when it is stored elsewhere. The default is the
repository-local Hugging Face cache. CMake verifies its SHA256:
`7a0adcfdb1715d3b0ff0f61486d8a181c33f4a208343a5abfccbc6540e872d24`.
The model is from onnx-community/mediapipe_selfie_segmentation_landscape,
revision `2497d5bec26c626c7b3c4edc6e1fefc21b64f6c3`, `onnx/model.onnx`.
No model is downloaded during configure, build, or testing. When the model is
absent, configure reports that the full-model test is unavailable; the operator
tests remain available.

The test links the production generated graph and its weight resource, calls
`makeMediaPipeLandscapeDmlProgram`, and compares the raw foreground alpha
[1,1,144,256] against WinML for identical RGB Float32 NCHW [1,3,144,256] inputs.
Inputs are zeros, constant gray, RGB gradient, seeded pixels, and
`tests/Scenarios/0000-portrait-umireon.jpg`. Windows WIC decodes the portrait,
resizes it linearly to 256x144, and converts RGB bytes to planes in [0,1].
This verifies inference, not OBS capture, mask postprocessing, or rendering.

The OS WinML rejected the unmodified model because its maximum supported IR
version is 9 and the source declares IR 10. The pinned opset-14 model was
inspected and has no IR-10-only overloads, non-ModelProto metadata_props, or
INT4/UINT4 types. For the comparison only, `landscapeModel()` changes the IR
header from 10 to 9 in memory. Nodes, attributes, weights, and opsets remain
byte-identical, and the cached original file is unchanged. Thus these results
are for the graph with a compatibility-adjusted IR declaration, not successful
loading of the original IR-10 file by this WinML version.

On 2026-10-14 all 10 full-model comparisons passed the same tolerance as the
operator tests (absolute/relative 2e-5), including portrait:

CTest passed all 30 entries (18 existing, 11 operator, 1 full-model test).
The full-model entry completed in 28.04 seconds.

| WinML selection | Portrait max absolute error | Mean absolute error | RMSE |
| --- | ---: | ---: | ---: |
| CPU | 4.51505e-06 | 7.47798e-09 | 7.70073e-08 |
| GPU preference | 1.96695e-06 | 4.60552e-09 | 4.96409e-08 |

The full-model test prints metrics for all input scenarios and fails on
nonfinite outputs or elements outside tolerance. The recorded direct run is
`build/landscape-winml-comparison.txt` in this checkout.

### IR declaration verification

On 2026-10-14 the original IR-10 model and the exact in-memory IR-9 edit were
compared directly using ONNX Runtime 1.22.1 CPUExecutionProvider, which loads
both versions. ONNX checker accepted both models. The serialized files differ
only at byte offset 1 (10 versus 9), their graph/opsets are equal, and after
normalizing ir_version the complete ModelProto messages are equal. The IR-10
feature audit found no overloads, non-ModelProto metadata_props, or INT4/UINT4.

Zeros, constant gray, RGB gradient, seeded pixels and the portrait were tested
with graph optimization disabled and fully enabled (10 comparisons). Every
output was finite, with identical shape [1,1,144,256], and bitwise identical
between IR 10 and IR 9: maximum/mean error 0, differing elements 0. The cached
original model was unchanged. The portrait was resized using Pillow bilinear
for this diagnostic, not the previous WIC path; the same resulting tensor was
supplied to both IR versions. This establishes numerical equivalence for these
cases in ORT; the installed WinML cannot directly compare IR 10 because it
rejects that declaration.

The development-only diagnostic and machine-readable report are saved locally
as `build/VerifyLandscapeIr.py` and `build/landscape-ir-comparison.json`.
They introduce no Python dependency into the C++ tests or ordinary build.

## Optional runtime timing

`WinMlComparison --test-case=MediaPipeLandscapeTiming` measures the production
DML runtime with the portrait input, including upload, dispatch, synchronization
and readback. It records initialization separately, warms up 10 times, and
measures 200 runs, reporting mean, median, p95, minimum and maximum wall time.
It forces inference every iteration without the OBS pipeline's similarity skip.
It is deliberately not registered with CTest; timings are not correctness gates.

On 2026-10-14 with OBS running concurrently on Intel UHD 630, initialization
took 72.255 seconds. Runtime mean was 54.787 ms, median 51.308 ms, p95 83.426 ms,
minimum 17.746 ms and maximum 169.311 ms. Because this launches a second DML
runtime alongside OBS, GPU contention and scheduling are included; these are
not measurements of latency inside the running OBS filter or idle-GPU capacity.
The raw result is `build/dml-model-timing.txt`.

## OBS luminance gate and GPU-input inference

The Windows Landscape Pipeline owns its input/reference BGRA textures and R8
luminance-difference render target/staging surface. It draws the absolute
Rec.709-weighted RGB difference, maps the R8 image synchronously and sums rows
using their pitch. The normalized mean absolute difference controls inference;
no separate gate object owns this policy. The initial reference is black and
its corresponding mask is all background. Mask refinement settings do not
force inference. Only successful inference and mask refinement advance the
reference image and retained model output.

The Pipeline sends BGRA input to MediaPipeLandscapeDirectMLEngine only when
inference is required. The Engine owns input transfer, RGB-plane packing,
DirectML submission and output readback. CPU mask refinement and final rendering
remain in the Pipeline.

`GpuImageInferenceTest.MainEffectLumaAbsDiff` tests the shared effect separately.
`LandscapeRenderingInitialization` also renders a portrait through an OBS input
source and filter. It compares the current-frame alpha against the model
reference, exercises unchanged-input settings updates, and checks restoration
of the render target, color space and viewport.

## Model-owned asynchronous DirectML engine

The facade obtains OBS's native D3D11 texture and descriptor. The private Impl
owns the generated Runtime, shared D3D11/D3D12 texture, RGB packing shader,
command list, fences and output readback. The shared texture has the input's
DXGI resource format; SRV creation selects its typed interpretation. BGRA and
RGBA inputs are tested. RGB packing happens after copying, discards alpha,
and produces Float32 R/G/B planes without sRGB conversion. Unsupported
conversion formats are rejected before command submission.

The caller provides an active OBS graphics context for construction, submission,
result retrieval and destruction. All calls are serialized; there is one
outstanding submission and no worker thread.

- `runAsync(inputTexture)` submits the D3D11 copy, input-ready signal/flush,
  D3D12 queue wait, conversion, inference, readback copy and completion signal.
  It does not wait on the CPU. Resource preparation and command recording occur
  before submission; compiled operators and command lists are reused. The RGB-plane buffer starts
  in COPY_SOURCE and returns to that state on each execution; recording occurs
  only once, after the first input fixes the native format.
- `runAsync` returns `RunResult::NeedsRecreation` without submitting work when
  the native format changes after the first input. Pipeline owns Engine
  replacement and retries the current input; Engine never rebuilds itself.
  Successful submission returns `RunResult::Submitted`. Invalid inputs and
  D3D failures remain exceptions.
- `isDone()` checks the submitted completion value without flushing or blocking.
  Before submission and after result retrieval it returns false.
- `getResult(outputSpan)` accepts only a completed submission and the correct
  output size. It maps/copies/unmaps without waiting and releases the submission
  for reuse. A second submission before retrieval is rejected even when complete.
- `blockCpuUntilInferenceCompletes()` is explicit CPU blocking for the
  current submission. It returns immediately if already complete. No work is an
  error. Normal submissions never invoke it internally.

Pipeline submits after the existing image-difference decision, prepares settings
and draws the current output source while GPU work proceeds, then polls before
CPU mask refinement. Only if incomplete at that deadline does it explicitly
block. It always uses that frame's result; no previous-frame substitution is
introduced. Settings-only updates still refine retained output without forcing
inference. Its local render-state restoration also runs on early returns.

Submission failures make the Engine unusable; destruction drains queued work
before releasing resources. The per-frame consumed fence has been removed.
Completion waiting is needed only at the output deadline or during destruction.
`InferenceReadiness.CompletedBeforeDeadline` and SDK TraceLogging activities expose
the split in WPR. Function-wide activities use their C++ function names; named
regions include `Pipeline.IndependentRendering`, `Pipeline.GpuOutputWait`,
`MPLDEngine.Copy11To12` and `Pipeline.RefineMask`. Start/Stop events share an
ActivityID and use keyword 0x1 and verbose level 5. Measure duration from the
paired event timestamps; the old ScopeId/DurationMs payload is removed.
The provider is registered at plugin load and unregistered at unload. Test and
benchmark executables register and unregister it in main.

MediaPipeLandscapeDirectMLEngineTest compares BGRA and RGBA portrait
inputs in separate Engines with a fixed format per Engine against the Runtime CPU-input path (max absolute error below 2e-5).
A test-only shared fence holds the producer queue to deterministically verify
nonblocking submission, unfinished-result rejection and double-submission
rejection. It also checks idle/completed/consumed states, invalid inputs/output
sizes, reuse, format-change detection, owner replacement and destruction with an unread result.

## Waiting-placement benchmark

BgraLandscapeBenchmark compares `immediate-wait` and `deferred-wait` on the same
Engine, input and work, with alternate order, 20 warmups and 100 samples per
scenario. Both include the R8 gate, BGRA GPU inference, a 1280x720 source draw,
CPU Float32-to-U8 mask conversion and resize, alpha upload and final composition
submission. Refinement is disabled, as in the benchmark's default settings.
The immediate path retrieves output before the source draw; the deferred path
retrieves it after. Both validate current-frame output against the CPU-input
Runtime. CSV includes submission, independent work, deadline waiting, result
retrieval, refinement, drawing and the wait-required flag. Report mean, median
and p95, as well as the completion-before-deadline ratio.

These are CPU-side fixture frame timings, not live OBS camera/scene timings.
Final composition is submitted but its GPU completion is not part of the frame
CPU duration. The fixture cannot demonstrate overlap with unrelated scene work.
A historical `bgra-engine` versus `cpu-upload` benchmark excluded source drawing
and mask processing; do not compare its total directly with this benchmark.

On Intel UHD 630, the 2026-10-15 run measured mean fixture inference frames of
10.442 ms with immediate waiting and 10.414 ms with deferred waiting. Mean
explicit wait was 7.658 versus 7.564 ms; both needed waiting in 100/100 samples.
Submission averaged 0.137 ms. The independent draw was too short to hide the
inference latency. This verifies the execution structure, not a meaningful
speedup. Logs, raw samples and statistics are saved in
`build/async-performance-20261015`.

## MediaPipe Conv-ReLU fusion

GenerateModel.py fuses a biased Conv whose output is consumed only by one Relu,
with matching shape and type. A Conv output exposed as a graph output or shared
with another consumer is preserved. ConvTranspose is not fused by this pass.
The generated FusedNode contains BiasedConv2dRelu, and its comment records both
original ONNX node indices and names. The compiler maps this explicit type to
DML_CONVOLUTION_OPERATOR_DESC.FusedActivation with a ReLU descriptor.
The pinned model has 22 such pairs; the generated barrier plan is recalculated.

The ConvRelu and DepthwiseConvRelu CTest cases compare the fused DML operators
to unfused Conv/Relu ONNX models built with nanopb, using WinML CPU and GPU.
MediaPipeLandscape compares the generated full model against the original ONNX.

For a performance baseline, GenerateModel.py --no-fuse-conv-relu emits the same
model without this fusion. Benchmark before/after builds with the same portrait,
settings and BgraLandscapeBenchmark executable configuration. Keep each build's
samples.csv and log in separate files. This baseline option is for experiments;
normal regeneration enables fusion.

HardSwish gate experiment: the original ONNX HardSwish is lowered to
`HardSwishGate` (DML 1.0 HardSigmoid, alpha=1/6, beta=1/2) and Mul.
Generated comments identify each original node. `--no-fuse-hard-swish`
retains Add/Clip/Mul/Mul for a one-variable performance comparison.
`HardSwishFused` compares the two-operator implementation directly with
WinML running ONNX HardSwish, including inputs near -3 and 3.

The generator converts the pinned final ConvTranspose into ordinary
`BiasedConv2d` and `DepthToSpace` Nodes. The 1x1 convolution produces
[1,4,72,128], followed by block-size-two DCR rearrangement into [1,1,144,256].
Weights are rearranged into [4,16,1,1] and the scalar bias repeated four times
at generation time. Each generated Node identifies the original ONNX node.
Sigmoid remains separate. `--no-pointwise-projection` keeps the backward DML
Convolution baseline. `PointwiseConvTranspose` tests this two-Node graph against
WinML CPU/GPU ConvTranspose with constant, signed, boundary and corner inputs.

The Landscape generator emits FP32 input specifications, weights, intermediate
and output tensor types. The compiler uses DML_EXECUTION_FLAG_NONE. A generated
static_assert checks that all used tensor types are FP32.
Only the embedded byte payload uses FP16 (212808 bytes). Model initialization
widens it to 425616 FP32 bytes before Runtime compilation/upload, then releases
the temporary expanded vector after Runtime construction. InputSpec byte offsets
refer to this expanded FP32 region. Generation rejects weights that do not
round-trip through binary16 exactly. `--float32` remains a compatibility option;
it no longer changes the generated execution precision or payload format.

The engine converts texture RGB directly to FP32 planar input and copies FP32
readback without half conversion. Full-model CPU/GPU input paths require FP32.
The final projection remains separate from Sigmoid; generic runtime
FP16 operator support is still covered independently by the operator tests.

For PIX profiling, launch `WinMlComparison.exe` without arguments under PIX.
It selects the normally skipped `MediaPipeLandscapePixWorkload`, which performs
ten portrait inferences without WinML reference work on another GPU queue.
Initialization is present in the capture and must be excluded from inference
measurements. Explicit selection is also available with
`--test-case=MediaPipeLandscapePixWorkload --no-skip`.

## Explicit node types and input barriers

Each Node declares its `dataType`. InputItem also declares the type and must
match the referenced InputSpec. The compiler validates references, types and
operator/device support once during compilation, without inferring output types
or performing implicit conversion. Operators compile with `DML_EXECUTION_FLAG_NONE`.

The Python generator emits `std::array<std::int32_t, 8> uavBarriers[]`, with
one row per Node. Row i names producer Node outputs to synchronize immediately
before Node i; -1 marks unused slots, including holes. All eight slots can be
used. The generator rejects a row requiring more than eight entries. Reordering
Nodes requires regenerating this table. The graph can omit the table to request
no Node-boundary barriers; C++ never infers missing synchronization.

Instructions may refer to any preceding Node, not only the current Node's inputs.
The compiler resolves Node outputs to compiled tensors, preserving the given
row order and duplicates. Runtime batches each boundary's UAV barriers into one
ResourceBarrier call. A nonempty row on InputItem is retained as a barrier-only
step at that boundary. Synchronization inside an operator implementation remains
separate from this external schedule.

`testExplicitTypesAndBarriers` checks explicit type validation and empty/missing
tables. `testUavBarrierInstructions` checks moved synchronization, unrelated
producers, multiple resources, all eight entries, unused slots, duplicates,
invalid references, and InputItem/trailing boundaries, including GPU execution.
Runtime, GPU texture input and WinML comparisons exercise the generated table.
The full-model comparison retains the FP32 bound of 2e-5 absolute plus 2e-5
relative error against WinML.


The generator accepts `--uav-placement first-consumer` (the default) and
`--uav-placement early-batched` for a controlled placement experiment. The latter
moves each barrier to the earliest existing nonempty boundary after its producer
and at or before its first consumer, subject to the eight-entry row limit.
Generation validates that every required producer is synchronized exactly once
within that interval. Nodes, weights, internal barriers and dispatch order remain
identical. For the pinned fused Landscape model this moves Node 159's barrier
from before Node 165 to before Node 163, alongside Node 160's barrier. Both
schedules have 124 resource barriers at 123 boundaries. Interval scheduling gives
a lower bound of 123 boundaries even without the row capacity constraint; with
these immutable dedicated outputs and this dispatch order, removing further
barriers would remove a required read-after-write dependency.
