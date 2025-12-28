# MediaPipe Selfie Multiclass Model

This directory should contain the converted ONNX model file: `selfie_multiclass_256x256.onnx`

## About the Model

The MediaPipe Selfie Multiclass Segmentation model provides superior segmentation quality compared to the standard selfie segmentation model. It excels at:

- **Multiple people in frame**: Better handles scenarios with multiple subjects
- **Full-body segmentation**: Works well for full-body shots, not just close-ups
- **Fine-grained segmentation**: Can distinguish between hair, clothes, body-skin, face-skin, and accessories

## Model Specifications

- **Input**: `[1, 256, 256, 3]` - 256x256 RGB image, float32, normalized to [0, 1]
- **Output**: `[1, 256, 256, 6]` - 256x256 with 6 class probability channels:
  - Class 0: Background
  - Class 1: Hair
  - Class 2: Body-skin
  - Class 3: Face-skin
  - Class 4: Clothes
  - Class 5: Others (accessories)

## Getting the Model

### Option 1: Download Pre-converted ONNX (if available)

Check the PINTO Model Zoo for a pre-converted version:
- https://github.com/PINTO0309/PINTO_model_zoo

### Option 2: Convert from TFLite

1. Download the original TFLite model:
   ```bash
   wget https://github.com/valeragabriel/BodySegmentation-MediaPipe/raw/main/selfie_multiclass_256x256.tflite
   ```

2. Convert using PINTO's Docker image:
   ```bash
   docker run --rm -v $PWD:/workspace pinto0309/tflite2tensorflow:latest \
       --model_path /workspace/selfie_multiclass_256x256.tflite \
       --flatc_path /usr/local/bin/flatc \
       --schema_path /usr/local/bin/schema.fbs \
       --model_output_path /workspace/selfie_multiclass_saved_model \
       --output_onnx
   ```

3. Copy the converted model:
   ```bash
   cp selfie_multiclass_saved_model/model_float32.onnx ./selfie_multiclass_256x256.onnx
   ```

## Alternative Conversion Script

See `../../scripts/convert_selfie_multiclass.py` for detailed conversion instructions.

## References

- Model Card: https://storage.googleapis.com/mediapipe-assets/Model%20Card%20Multiclass%20Segmentation.pdf
- MediaPipe Solutions: https://ai.google.dev/edge/mediapipe/solutions/vision/image_segmenter
- Original TFLite: https://storage.googleapis.com/mediapipe-models/image_segmenter/selfie_multiclass_256x256/float32/latest/selfie_multiclass_256x256.tflite

## Testing

You can test the model quality at:
https://mediapipe-studio.webapps.google.com/demo/image_segmenter

Select "Multi-class selfie segment..." from the model dropdown to see the improved segmentation quality.
