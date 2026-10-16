<!--
SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Isolated Conv MetaCommand probe

`ConvMetaCommandProbe` bypasses the model compiler and uses public DirectML APIs
with a single biased 1x1 Conv. It does not change production compilation flags.
CTest checks the outputs; PIX inspection checks which implementation was selected.
The test does not assert MetaCommand availability, which is device-dependent.

## Matrix

The 68 cases vary these parameters independently:

- Shape: input `[1,8,4,4]`, filter `[8,8,1,1]`, output `[1,8,4,4]`;
  or Landscape node 169 input `[1,128,18,32]`, filter `[24,128,1,1]`,
  output `[1,24,18,32]`.
- Tensor type: FLOAT32 or FLOAT16 for input, filter, bias and output.
- Execution flags: NONE, ALLOW_HALF_PRECISION_COMPUTATION;
  DISABLE_META_COMMANDS is an additional NCHW control case.
- Physical layout: packed NCHW or NHWC (logical sizes remain NCHW).
- Guaranteed base offset alignment: 0 or 16. Actual bindings have offset zero.
- Filter and bias ownership: OWNED_BY_DML or ordinary bound tensors.

All cases use forward cross-correlation, stride/dilation `[1,1]`, zero padding,
group count 1, and no fused activation. Sizes in bytes are rounded up to four.
Each initializer runs before its three inference dispatches. All outputs are
checked using exactly representable constant inputs/filter/bias. This is an
execution/descriptor check, not an accuracy test of arbitrary half computations.
`probe-cases.csv` records dispatch order and parameters in the working directory.

## Running

`--predication` runs an opt-in capability diagnostic: the minimal FP32/NCHW
Conv with NONE and DISABLE_META_COMMANDS, each with predicate values 0,1,0,1.
Only RecordDispatch is predicated (EQUAL_ZERO suppresses a zero predicate).
Each iteration unconditionally initializes all 128 output elements to -123,
then unconditionally reads them back after disabling predication. Expected
executed output is 1.125. Thus a predicated output copy cannot hide execution.
The initializer and input uploads are outside predication. The eight-byte
predicate is an upload buffer in GENERIC_READ, which includes PREDICATION.
Each iteration completes before updating the predicate or resetting commands.
Results are written to `predication-results.csv`; exit status 1 indicates
that at least one predicate did not suppress Conv. This is a diagnostic,
not a portable assertion that every DirectML implementation supports predication.

Observed on 2026-10-17, Intel UHD Graphics 630, driver 27.20.100.8681:

| Compile flag | PIX implementation | Predicate 0 | Predicate 1 |
| --- | --- | --- | --- |
| NONE | ExecuteMetaCommand | 1.125 (executed) | 1.125 (executed) |
| DISABLE_META_COMMANDS | Dispatch | -123 (skipped) | 1.125 (executed) |

Both repetitions agreed for every element. PIX 2603.25 capture/export confirmed
four ExecuteMetaCommand calls for NONE and four Dispatch calls for the control,
each surrounded by SetPredication/disable; debug-layer replay reported no title
warnings. Event presence alone does not establish execution: the unconditional
numeric readback establishes that the MetaCommand ignores the zero predicate
on this configuration. This result does not establish behavior on other drivers.
The ordinary descriptor-matrix CTest remains unchanged and passed.

Artifacts: `build/conv-predication-20261017/{results.txt,predication-results.csv,
probe.wpix,events.csv,export,debug-layer.log}`. No production flags were changed.

Build the `ConvMetaCommandProbe` target, then run:

```powershell
ctest --test-dir build -C RelWithDebInfo -R '^ConvMetaCommandProbe.DescriptorMatrix$' --output-on-failure
```

For PIX, launch the executable using `--captureFromStart`, `take-capture --open`,
and `save-capture`. Set `--working-directory` to a unique artifact directory.
Replay the capture with `save-event-list events.csv --counters="*Duration*"`.
GPU timing collection requires an elevated PIX process. Compare the root
`DML_OPERATOR_CONVOLUTION` regions in case order, three per row of the manifest;
exclude initializers and do not double-count nested ExecuteMetaCommand/Dispatch.

## Observed result, 2026-10-15

Intel UHD Graphics 630, driver 27.20.0100.8681, PIX 2603.25.
All 68 cases completed with the expected output, and the capture contained all
204 inference regions. Under NCHW, both shapes, both alignment declarations and
both ownership choices showed the same selection:

| Tensor type | NONE | ALLOW_HALF_PRECISION_COMPUTATION |
| --- | --- | --- |
| FLOAT32 | MetaCommand | DirectCompute |
| FLOAT16 | DirectCompute | MetaCommand |

The tested NHWC cases and DISABLE_META_COMMANDS controls used DirectCompute.
Thus FP16 MetaCommands are supported in this environment. The production
compiler's unconditional NONE flag prevents this tested FP16 Conv from using
the available MetaCommand implementation. NONE remains a valid API call; it
has different computation-precision permissions. This is not malformed tensor
metadata, and the permission flag is not safe to apply indiscriminately to FP32.

For the node-169 shape, NCHW, OWNED_BY_DML, alignment declaration 0, three PIX
replay samples averaged 0.162694 ms with FLOAT16/NONE and 0.063027 ms with
FLOAT16/ALLOW_HALF_PRECISION_COMPUTATION. These short measurements establish the
path change, not an end-to-end model speedup. The full model must still be
checked against WinML after any production precision-policy change.

Production descriptor comparison: logical sizes, element strides, ownership,
convolution stride/dilation/padding/group/fusion settings are unchanged by the
FP16 generator; tensor types change and byte counts halve (with four-byte
padding). Both variants currently compile with NONE, and neither disables
MetaCommands. GuaranteedBaseOffsetAlignment is zero. The isolated node-169
baseline reproduces those descriptor choices.

References:

- [Execution flags](https://learn.microsoft.com/en-us/windows/win32/api/directml/ne-directml-dml_execution_flags)
- [Buffer tensor descriptor](https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_buffer_tensor_desc)
- [Convolution descriptor](https://learn.microsoft.com/en-us/windows/win32/api/directml/ns-directml-dml_convolution_operator_desc)

## Driver MetaCommand contract diagnosis

`--enumerate-meta` prints the installed driver's command GUIDs, names, creation/
initialization/execution parameter names, offsets, primitive types and sizes.
`--probe-meta-creation` attempts creation of enumerated Conv and GEMM versions
using independent input/filter/bias/output type codes 0..2, precision codes 0..2,
two shapes, planar/channel-last strides for Conv, and BindFlags 0..3.
`--probe-meta-static` additionally checks static weights and omitted bias/C;
input/output types remain equal in that sweep. `--probe-dml-types` independently
checks the public DirectML Conv API for every FLOAT16/FLOAT32 tensor combination.

The MetaCommand ABI is exploratory: field offsets and primitive sizes are obtained
from the driver; enum candidates use published NVAPI ML definitions as a starting
point, not as an Intel ABI guarantee. Modern descriptor names are handled through
enumeration. Uniform-type controls must succeed; all raw numeric codes are retained.
Creating a MetaCommand does not verify execution correctness, accumulation precision
or speed. Rejection applies only to the tested descriptors and installed driver.
No raw driver MetaCommand path is added to the production runtime.
