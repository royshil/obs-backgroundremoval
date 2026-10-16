<!--
SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Landscape intermediate precision dump

Build the opt-in `LandscapePrecisionDump` target with the default FP32-generated
Landscape model (FP16 embedded storage, expanded at initialization). This diagnostic does not modify normal runtime execution.
It takes a binary float32 planar RGB input `[1,3,144,256]` and an output directory:

```powershell
LandscapePrecisionDump.exe portrait-input.f32 output-directory
```

The input can be exported by the existing portrait WinML comparison using
`DML_COMPARISON_OUTPUT_DIR`. `GPU-input.f32` and `CPU-input.f32` are model inputs,
not masks. Use a new destination for every experiment.

For each of the 125 operator nodes, the tool replays the graph prefix from the
original input and writes the output. This avoids inserting diagnostic copies or
barriers into normal command recording. The two variants are FP32 and FP16 tensors, both compiled with NONE.

It also executes the isolated operator on the FP32 prefix's input values using
those two variants, then repeats FP32 with runtime operands explicitly rounded
to FP16 first. Filter/bias ownership and learned weights are retained. The diagnostic reads expanded FP32 weights and repacks them only when a test
explicitly requests FP16 tensors. The embedded FP16 values round-trip the
original pinned model exactly. Values are dumped as little-endian float32;
FP16 results are widened without additional loss.

File names contain graph node index, prefix/local, and precision mode. `nodes.csv`
records shapes and predecessor indices. There are five output files per node.
Comparing local FP16 against FP32-rounded-inputs separates input quantization from
computation/output-storage differences. Rounding that FP32 reference's output to
FP16 further isolates differences beyond output storage rounding. This cannot
prove the hardware's internal accumulation format or summation order.

Do not use this tool for timing. Prefix compilation and synchronous readback are
intentionally repeated. It is excluded from the default build and ordinary CTest.

## Storage-boundary isolation

```powershell
LandscapePrecisionDump.exe portrait-input.f32 output-directory --storage-boundaries
```

This mode first rounds the input to FP16 and widens it to FP32 once. Every
comparison uses those identical input values. Weights are identical as well.
The diagnostic executes each operation with FP32 tensors and NONE flags, then
optionally rounds only its output to FP16 and widens it before the next operation.
The before/after dumps locate the first storage rounding error independently of
input quantization and half-computation permissions.

It checks the sequential FP32 baseline against a full FP32 run and compares the
all-rounded storage experiment against a real FP16/NONE full-model run.
`checks.csv` records these controls. `single-boundary.csv` injects rounding at
only one producer at a time, feeding that producer to all its downstream
consumers in a full FP32 graph. It measures final-mask impact without conflating
it with pre-existing upstream errors. `storage-only-keep-first-conv.f32` leaves
only the first Conv output unrounded while all other outputs are rounded.

Single-boundary effects must not be added together: propagation is nonlinear,
and changing one rounding boundary can alter error cancellation elsewhere.

## Video screening

`--screen` accepts a directory of planar 256x144 RGB float32 `.f32` inputs and
writes a per-frame comparison plus both model outputs. Model instances are reused.
Both models receive the same FP16-rounded input values; the reference uses FP32
tensors/NONE and the candidate uses FP16 tensors/NONE. This isolates the question
of intermediate storage precision, rather than input precision or Allow flags.
The CSV includes whole-mask/boundary RMSE, raw 0.5 classification disagreements,
and differences after the pipeline's continuous 8bit mask conversion.

External video decoding/resizing is diagnostic preprocessing. It is not asserted
to be bit-identical to OBS's GPU sampling or to its final rendered composition.

## Cumulative RMSE storage policy

`--policy-select` and `--policy-check` take a directory of frame inputs and a
policy output directory. The reference and candidate both use FLOAT32 operators
with NONE flags and identical FP16-roundtripped input values. The candidate
inserts an explicit nearest-even FP16 roundtrip after selected producer outputs.
Weights and all mathematical operators are otherwise unchanged. GPU rounding
and intermediate readbacks exist only in the diagnostic build; the production
runtime has no additional work or public diagnostics methods.

Before policy selection, `--gpu-storage-check` compares every GPU intermediate
against the earlier CPU-storage-rounding diagnostic. The video worst-frame
control matched at all 125 operators.

The safety limit is the cumulative, elementwise absolute RMSE against the FP32
output of the same node: 0.001. Only the final projection (the 1x1 Conv and its DepthToSpace) is exempt; the final Sigmoid
is checked. At the first violation, selection tries keeping each still-rounded
ancestor in FP32 and chooses the best reduction of that violation. Policy updates
only add FP32 nodes. After a pass with changes, the entire input set is replayed
again; success requires a pass with a fixed policy and no changes/violations.
`priority.txt` can put difficult frame filenames first. `keep-fp32.txt` allows
resumption. The emitted node-maxima and per-frame CSVs record the actual maxima.

This finds a sufficient policy, not a proven minimum set. Backward pruning on
witness frames can remove choices that became redundant; every pruned policy
must then be revalidated on the full dataset. The artifacts distinguish these
stages. Passing the diagnostic is not yet a production mixed-storage
implementation: DirectML tensor-type constraints still need explicit conversion
at storage boundaries when integrating such a policy.

## Weight storage benchmark

`--weight-storage-benchmark` compares FP16 CPU storage expanded once to FLOAT32
against FLOAT32 CPU storage copied as-is. All GPU tensors, weights, inputs and
operator flags are identical (FLOAT32/NONE). This is not a mixed-type GPU Conv
experiment. It checks expanded weight bytes and model outputs for exact equality.

Preparation measures allocation plus SIMD half expansion or a FLOAT32 copy, with
2,000 alternating samples each. Runtime initialization excludes that preparation.
Eight ABBA/BAAB blocks each construct a runtime, warm up 20 times and measure 150
synchronous runs (600 samples per storage variant). Inference times are CPU wall
time including command recording, submission, completion wait and readback; they
are not isolated GPU kernel times. No storage-rounding diagnostic hooks are enabled.
The CSV retains block order to expose clock/thermal drift. Production is unchanged.
