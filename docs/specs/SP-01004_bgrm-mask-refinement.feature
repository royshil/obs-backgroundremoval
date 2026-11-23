# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

@SP-01004
Feature: Mask refinement
  The refined mask is the share of the foreground in the 5 x 5 window around
  each texel of the probability, counting only texels inside the mask. A texel
  is foreground when its probability is at least 0.5, the threshold the model
  is trained for; nothing adjusts it.

  @SP-01004.1
  Scenario: Interior texel
    Given a probability mask where 13 of the 25 texels around an interior texel reach the threshold
    When the mask is refined
    Then the refined value of that texel is 13 / 25
    And the texel is foreground

  @SP-01004.2
  Scenario: Corner texel
    Given a probability mask where 5 of the 9 texels around the top-left corner texel, inside the mask, reach the threshold
    When the mask is refined
    Then the refined value of the corner texel is 5 / 9
    And padding outside the mask is not counted

  @SP-01004.4
  Scenario: Agreement with the reference
    Given the foreground probability of the portrait test image from ONNX Runtime
    When it is refined by the shader and by a NumPy reference of the same definition
    Then the foreground decisions agree on at least 99.5% of the frame's pixels after upscaling to 1280 x 720
