# D3D12 mask refinement

`MaskRefinerDirect3D` consumes the packed FP32 probability buffer produced by the
DirectML engine on the same D3D12 device and queue. The input is in COPY_DEST
state, remains alive until completion, and is restored to COPY_DEST after copying
into the retained probability buffer. A null input reuses the retained result;
before the first inference, it produces the initial all-background mask.

The compute passes perform background inversion, optional thresholding, a
separable nine-tap Gaussian (sigma 1.7, epsilon 0.001 footprint), half-pixel
bilinear resizing, optional rethresholding, and horizontal/vertical morphology.
Positive expansion erodes the background; negative expansion dilates it. Contour
processing is not part of this Windows path. Intermediate values remain FP32;
only the final R8_UNORM texture is quantized. Consequently this path intentionally
is not bit-identical to the former intermediate-uint8 implementation near edges
and thresholds.

The output texture is shared with D3D11 and wrapped by OBS using
`gs_texture_wrap_obj`. D3D11 signals readiness after prior reads, D3D12 waits
before writing, and D3D11 waits for completion before subsequent reads. The
texture is COMMON at API boundaries. Calls are serialized by the caller with
the OBS graphics context. There is one outstanding submission; `complete()`
consumes it. `runAsync()` never waits for CPU completion. The pipeline waits only
at its final mask deadline, after drawing the independent source image.

Only a four-byte non-finite-input validation flag is read back. No mask pixels
are read back or uploaded in the production pipeline. Standalone tests read
pixels solely to validate results against an independent CPU reference.

CTest includes WARP, native hardware, model-output integration, and pipeline
rendering checks. The opt-in isolated benchmark compares CPU wall time for GPU
submission through completion with the former CPU operators plus dynamic-texture
upload (without forcing completion of that CPU upload):

```powershell
build/tests/DmlProgram/RelWithDebInfo/MaskRefinerDirect3DTest.exe --hardware --benchmark
```

The benchmark reports mean, median, p95 and final binary mask differences. It
excludes model inference. `Pipeline.RefineMask` now measures CPU submission;
`Pipeline.GpuOutputWait` covers the final model-and-refinement deadline, with
`MaskRefiner.CpuFenceWait` identifying an actual blocking mask wait.
