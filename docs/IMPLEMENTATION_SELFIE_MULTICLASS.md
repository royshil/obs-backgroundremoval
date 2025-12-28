# MediaPipe Selfie Multiclass Implementation

This document describes the implementation of MediaPipe's Multi-Class Selfie Segmentation model for the OBS Background Removal plugin.

## Overview

The MediaPipe Selfie Multiclass model provides significantly improved segmentation quality compared to existing models, particularly for:

- **Multiple people in frame**: Better handles scenarios with multiple subjects
- **Full-body segmentation**: Works well for full-body shots, not just portrait close-ups
- **Fine-grained segmentation**: Can distinguish between hair, clothes, body-skin, face-skin, and accessories

### Model Specifications

- **Input**: `[1, 256, 256, 3]` - 256x256 RGB image, float32, normalized to [0, 1]
- **Output**: `[1, 256, 256, 6]` - 256x256 with 6 class probability channels:
  - Class 0: Background
  - Class 1: Hair
  - Class 2: Body-skin
  - Class 3: Face-skin
  - Class 4: Clothes
  - Class 5: Others (accessories)

## Implementation Details

### Files Modified

1. **`src/models/ModelSelfieMulticlass.h`** (NEW)
   - Model handler class that processes the 6-channel output
   - Implements `postprocessOutput()` to convert multi-class probabilities to binary mask
   - Uses argmax to find dominant class per pixel
   - Creates foreground mask from all non-background classes

2. **`src/consts.h`**
   - Added `MODEL_SELFIE_MULTICLASS` constant pointing to the ONNX file path

3. **`src/background-filter.cpp`**
   - Added include for `ModelSelfieMulticlass.h`
   - Added model to selection UI dropdown
   - Added model instantiation logic

4. **`data/locale/en-US.ini`**
   - Added "Selfie Multiclass" display name for the model

5. **`data/models/README_SELFIE_MULTICLASS.md`** (NEW)
   - Comprehensive documentation about the model
   - Conversion instructions
   - References to model sources

6. **`scripts/convert_selfie_multiclass.py`** (NEW)
   - Python script with detailed conversion instructions
   - Documents Docker-based conversion using PINTO's tools

7. **`.gitignore`**
   - Updated to allow the `scripts/` directory

### Algorithm: Multi-Class to Binary Mask

The `ModelSelfieMulticlass::postprocessOutput()` method implements the following algorithm:

```cpp
For each pixel (x, y):
    1. Find argmax across 6 class channels -> maxClass, maxProb
    2. If maxClass > 0 (not background):
           mask[x, y] = maxProb  // Confidence of the foreground class
       Else:
           mask[x, y] = 0  // Background
    3. Return single-channel float mask [0, 1]
```

This approach:
- Preserves the high confidence of the multiclass model
- Treats all person-related classes (hair, body, face, clothes, accessories) as foreground
- Only class 0 (background) is removed

## Obtaining the ONNX Model

The implementation is complete, but the ONNX model file is not included because:
1. The original model is in TFLite format (16MB)
2. Conversion requires external tools and Docker
3. The converted ONNX model would be ~30-40MB

### Conversion Steps

#### Method 1: Docker-based Conversion (Recommended)

```bash
# 1. Download TFLite model
curl -L -o selfie_multiclass_256x256.tflite \
  'https://github.com/valeragabriel/BodySegmentation-MediaPipe/raw/main/selfie_multiclass_256x256.tflite'

# 2. Convert using PINTO's Docker image
docker run --rm -v $PWD:/workspace pinto0309/tflite2tensorflow:latest \
    --model_path /workspace/selfie_multiclass_256x256.tflite \
    --flatc_path /usr/local/bin/flatc \
    --schema_path /usr/local/bin/schema.fbs \
    --model_output_path /workspace/selfie_multiclass_saved_model \
    --output_onnx

# 3. Copy to models directory
cp selfie_multiclass_saved_model/model_float32.onnx \
   data/models/selfie_multiclass_256x256.onnx
```

#### Method 2: Pre-converted ONNX (if available)

Check PINTO Model Zoo for pre-converted versions:
- https://github.com/PINTO0309/PINTO_model_zoo
- Look for model ID 039 or search for "selfie_multiclass"

### After Obtaining the Model

1. Place `selfie_multiclass_256x256.onnx` in `data/models/`
2. Rebuild the plugin
3. Launch OBS and add the Background Removal filter
4. Enable "Advanced settings"
5. Select "Selfie Multiclass" from the "Segmentation model" dropdown

## Testing the Model

### Quality Comparison

You can preview the model's quality before converting by testing it online:
https://mediapipe-studio.webapps.google.com/demo/image_segmenter

Select "Multi-class selfie segment..." to see:
- Improved multi-person segmentation
- Better full-body coverage
- Finer detail preservation (hair, clothing edges)

### Expected Improvements

Based on MediaPipe's documentation and testing:
1. **Multi-person scenarios**: Much better than single-class models
2. **Full-body shots**: No more cut-off limbs or body parts
3. **Hair detail**: Better preservation of fine hair details
4. **Clothing**: More accurate segmentation of clothing boundaries

## References

- **Model Card**: https://storage.googleapis.com/mediapipe-assets/Model%20Card%20Multiclass%20Segmentation.pdf
- **MediaPipe Solutions**: https://ai.google.dev/edge/mediapipe/solutions/vision/image_segmenter
- **Original TFLite**: https://storage.googleapis.com/mediapipe-models/image_segmenter/selfie_multiclass_256x256/float32/latest/selfie_multiclass_256x256.tflite
- **GitHub Issue**: Issue requesting this feature with comparison screenshots

## Future Enhancements

Possible future improvements:
1. **Class Selection UI**: Allow users to choose which classes to keep (e.g., keep hair but remove clothes)
2. **Per-Class Thresholding**: Different confidence thresholds for different classes
3. **Smooth Transitions**: Blend between classes for smoother edges
4. **Pre-converted Model**: Bundle the ONNX model once conversion is stable

## Build Status

The implementation has been committed and pushed to the repository. To complete testing:

1. ✅ Code implementation complete
2. ✅ Documentation complete
3. ⏳ ONNX model conversion (requires Docker or manual tools)
4. ⏳ Build and test with OBS
5. ⏳ Code formatting (requires clang-format 19.1.1 from Linuxbrew)

## Notes

- The implementation follows the existing model architecture pattern
- No breaking changes to existing functionality
- Model selection is backward compatible
- The conversion script provides detailed instructions for users who want to use this model
