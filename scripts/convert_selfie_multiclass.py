#!/usr/bin/env python3
"""
Script to convert MediaPipe Selfie Multiclass Segmentation model from TFLite to ONNX format.

The MediaPipe Selfie Multiclass model provides improved segmentation quality compared to 
the standard selfie segmentation model, especially for:
- Multiple people in frame
- Full-body segmentation
- Fine-grained segmentation (hair, clothes, body, etc.)

Model source: https://storage.googleapis.com/mediapipe-models/image_segmenter/selfie_multiclass_256x256/float32/latest/selfie_multiclass_256x256.tflite

Input: [1, 256, 256, 3] - 256x256 RGB image, float32, range [0, 1]
Output: [1, 256, 256, 6] - 256x256 with 6 classes:
    - Class 0: Background
    - Class 1: Hair
    - Class 2: Body-skin
    - Class 3: Face-skin  
    - Class 4: Clothes
    - Class 5: Others (accessories)

For background removal, we use argmax to find the class with highest confidence per pixel,
then create a binary mask where class > 0 (any non-background class) becomes foreground.

Conversion methods:
1. Using PINTO0309's tflite2tensorflow (recommended):
   docker run --rm -v $PWD:/workspace pinto0309/tflite2tensorflow:latest \
       --model_path /workspace/selfie_multiclass_256x256.tflite \
       --flatc_path /usr/local/bin/flatc \
       --schema_path /usr/local/bin/schema.fbs \
       --model_output_path /workspace/selfie_multiclass_saved_model \
       --output_onnx

2. Using online PINTO model zoo (if available):
   Visit: https://github.com/PINTO0309/PINTO_model_zoo
   Search for model ID 039 or selfie_multiclass

3. Manual conversion with tf2onnx (after converting TFLite to SavedModel):
   - Not recommended due to unsupported ops in this specific model
"""

import sys
import os

def main():
    print(__doc__)
    print("\n" + "="*80)
    print("CONVERSION INSTRUCTIONS")
    print("="*80)
    print("\nThis script provides instructions for converting the model.")
    print("Due to the complexity of the model architecture, Docker-based conversion")
    print("using PINTO0309's tools is recommended.\n")
    
    print("Step 1: Download the TFLite model")
    print("-------")
    print("wget https://github.com/valeragabriel/BodySegmentation-MediaPipe/raw/main/selfie_multiclass_256x256.tflite")
    print("OR")
    print("curl -L -o selfie_multiclass_256x256.tflite 'https://github.com/valeragabriel/BodySegmentation-MediaPipe/raw/main/selfie_multiclass_256x256.tflite'")
    print()
    
    print("Step 2: Convert using Docker (requires Docker installed)")
    print("-------")
    print("docker run --rm -v $PWD:/workspace pinto0309/tflite2tensorflow:latest \\")
    print("    --model_path /workspace/selfie_multiclass_256x256.tflite \\")
    print("    --flatc_path /usr/local/bin/flatc \\")
    print("    --schema_path /usr/local/bin/schema.fbs \\")
    print("    --model_output_path /workspace/selfie_multiclass_saved_model \\")
    print("    --output_onnx")
    print()
    
    print("Step 3: Copy the ONNX model to the models directory")
    print("-------")
    print("cp selfie_multiclass_saved_model/model_float32.onnx ../data/models/selfie_multiclass_256x256.onnx")
    print()
    
    print("Alternative: Download pre-converted ONNX from PINTO model zoo if available")
    print("-------")
    print("Check: https://github.com/PINTO0309/PINTO_model_zoo")
    print()

if __name__ == "__main__":
    main()
