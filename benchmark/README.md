# Benchmark

Micro-benchmark for the `obs-backgroundremoval` pre-inference pipeline.
It exercises the same code path the plugin runs in `video_tick` before
model inference: cloning the input frame and running a PSNR similarity
check on full-size 1920x1080 BGRA frames.

## Prerequisites

- A **C++17** compiler
- **CMake** 3.16+
- **OpenCV** (core)
- **ONNX Runtime** (optional) — enables the CPU utilisation test

## Build

Run from the repository root:

```bash
cmake -B build benchmark
cmake --build build
```

If ONNX Runtime is found by CMake, the CPU utilisation test is compiled in
automatically.

## Usage

```
./build/bench [-n <frames>] [-m <model_path>]
```

| Flag | Default | Description |
|------|---------|-------------|
| `-n` | 660 | Number of synthetic frames to process |
| `-m` | *(none)* | Path to an ONNX model (e.g. `benchmark/tiny.onnx`). Enables the ORT CPU utilisation test (Linux only). |

## What it measures

- **Per-frame timing** — mean, median, and p95 latency in microseconds.
- **Hardware perf counters** (Linux only) — cache references, cache misses,
  LLC store misses via `perf_event_open`.
- **ORT CPU utilisation** (requires `-m`) — runs 90 frames at 30 fps with a
  real ONNX Runtime session and reports wall time vs CPU time, showing the
  effect of thread-pool spin-waiting.

## Example output

```
Generated 60 synthetic frames (1920x1080 BGRA)

=== obs-backgroundremoval benchmark ===
Frames: 60 @ 1920x1080 BGRA (7.9 MB/frame)
  mean:   2309.6 us/frame
  median: 2012.1 us/frame
  p95:    4637.1 us/frame
  total:  138.6 ms (60 frames)
  cache-misses:     29067554 (84.0% of 34591781 refs)
  LLC-store-misses: 8461194 (86.8% of 9749379 stores)
===
```
