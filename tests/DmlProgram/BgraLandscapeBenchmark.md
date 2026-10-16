<!--
SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Landscape GPU gate benchmark

Build `BgraLandscapeBenchmark` in RelWithDebInfo or Release and put the OBS
binary directory on PATH. Use unique output directories; samples.csv is never
overwritten.

```powershell
$env:PATH = 'C:/Program Files/obs-studio/bin/64bit;' + $env:PATH
./build/tests/DmlProgram/RelWithDebInfo/BgraLandscapeBenchmark.exe `
    --output build/landscape-gpu-gate-run1 `
    --width 1280 --height 720 --warmup 20 --samples 100
```

The production Engine enables MetaCommands. Its dedicated depthwise CS remains
unchanged. The harness stages a 256x144 R8 luma difference once per iteration and
maps/sums/unmaps that single surface at the start of the next iteration. This
matches Pipeline's videoRender-to-videoTick scheduling. It records difference_sum
alongside timings. Model packing/inference is submitted only when requested.

Each scenario seeds the portrait and its retained mask before warmup:

- infer: alternate portrait/inverse, requiring inference every frame.
- skip: keep input/settings fixed; the CPU omits inference and mask correction.
- settings-only: keep input fixed, alternate thresholds 0.45/0.55; the CPU omits
  inference but recomputes the mask from retained probabilities.

After submission the harness draws an independent output-resolution source.
It queries completion at the mask deadline and blocks only if necessary, then
calls complete(). Skip-only iterations do not submit D3D12 Engine work or wait
for its completion; their gate Map still synchronizes the preceding Stage.
These isolated scenarios pass an explicit inference decision; they do not model
the Pipeline's previous-frame decision and continuation policy. Model output for
both fixtures is checked against the generated FP32 Runtime outside timing;
the maximum permitted absolute error is 2e-5. Separate CTest compares with WinML.

The Engine owns the final output-resolution shared R8 mask. Correction uses
packed U8 intermediates and fixed integer Gaussian weights on D3D12. Settings
changes do not force inference. Expansion and CPU mask readback/upload are absent.

samples.csv stores each frame; summary.csv reports mean/median/nearest-rank p95.
All reported intervals are CPU wall times:

| Column | Meaning |
| --- | --- |
| input_draw_ms | Fixture draw to the 256x144 BGRA input. |
| gate_ms | Previous difference Map/sum/Unmap plus current difference draw/Stage submission. |
| submit_ms | Inference or settings-only postprocess submission; no Engine work on skips. |
| independent_ms | Independent output-resolution source draw submission. |
| final_wait_ms | Completion query and any CPU wait for the entire GPU chain. |
| complete_ms | Engine completion handling. |
| refine_submit_ms | Zero: mask correction is part of the Engine submission. |
| draw_submit_ms | Final masked draw and Flush. |
| cpu_frame_ms | Input draw through final draw submission. |
| draw_drain_ms | Output staging/Map to drain draws, outside frame timing. |
| completed_frame_ms | Frame through drain, including benchmark-only readback. |

Neither Flush nor cpu_frame_ms implies GPU completion. The explicit drain prevents
queue accumulation across samples; production does not read the final image.
Sleep(10) and numeric verification occur outside measured intervals.

Engine blocking ratios use inference/settings-only submissions as their
denominator (skip-only has none). Compare cpu_frame_ms
and completed_frame_ms, not submit_ms or waits in isolation. GPU interval
attribution requires PIX.

For PIX launch with `--pix-engine`: this uses two samples per scenario, no
warmup, and omits the separate reference Runtime so PIX captures the Engine's
D3D12 device. Numeric reference verification is explicitly disabled in this
diagnostic mode; ordinary benchmark/CTest runs retain it. The mode is not used
for the before/after performance numbers.

The smoke CTest exercises all scenarios, Stage/Map and model results without
timing thresholds. This harness uses fixtures rather than OBS capture callbacks;
it does not include properties callbacks, background blur, or repeated-render
suppression. Preserve the old executable for controlled before/after comparisons.
