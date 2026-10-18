<!--
SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>

SPDX-License-Identifier: Apache-2.0
-->

# Windows benchmarks

Google Benchmark v1.10.0 is pinned in `vendor/google/benchmark`. These targets
are excluded from the default build and are never registered with CTest.

```powershell
cmake --build build --config RelWithDebInfo --target MPLD10Benchmark MPLD10CameraBenchmark
./build/tests/Benchmarks/RelWithDebInfo/MPLD10Benchmark.exe --benchmark_repetitions=5 --benchmark_out=build/landscape-benchmark.json
```

`Landscape/UploadInferenceReadback` measures synchronous full-mask inference
with upload and readback, using wall time after warmup. It is not isolated GPU
execution time, nor a native OBS camera performance measurement.

The [camera benchmark](MPLD10CameraBenchmark.md) retains the native OBS graphics
thread and PIX capture path. It records callback samples to CSV and uses Google
Benchmark manual timing to report those recorded render durations, with median
and p95 counters and `benchmark.json`. Those are callback durations, not GPU time.

Run performance analysis on the target GPU; normal CTest only checks correctness.
