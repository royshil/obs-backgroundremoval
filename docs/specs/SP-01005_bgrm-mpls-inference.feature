# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

@SP-01005
Feature: MediaPipe Selfie Segmenter inference
  The shader pipeline evaluates MediaPipe Selfie Segmenter (landscape, 256 x 144)
  as ONNX Runtime does.

  Background:
    Given the pinned model onnx-community/mediapipe_selfie_segmentation_landscape
    And the portrait test image scaled to 256 x 144

  @SP-01005.2
  Scenario: The foreground mask matches ONNX Runtime
    When the shader pipeline infers the foreground probability
    And ONNX Runtime infers it from the same input
    Then the foreground masks at threshold 0.5 have an IoU of at least 0.99
