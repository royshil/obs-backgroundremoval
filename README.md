# OBS Plugin: Portrait Background Removal / Virtual Green-screen and Low-Light Enhancement

<div align="center">

[![GitHub](https://img.shields.io/github/license/royshil/obs-backgroundremoval)](https://github.com/royshil/obs-backgroundremoval/blob/main/LICENSE)
[![GitHub Workflow Status](https://github.com/royshil/obs-backgroundremoval/actions/workflows/push.yaml/badge.svg)](https://github.com/royshil/obs-backgroundremoval/actions/workflows/push.yaml)
[![Total downloads](https://img.shields.io/github/downloads/royshil/obs-backgroundremoval/total)](https://github.com/royshil/obs-backgroundremoval/releases)
![Flathub](https://img.shields.io/flathub/downloads/com.obsproject.Studio.Plugin.BackgroundRemoval?label=Flathub%20Installs)
[![GitHub release (latest by date)](https://img.shields.io/github/v/release/royshil/obs-backgroundremoval)](https://github.com/royshil/obs-backgroundremoval/releases)

</div>

A plugin for [OBS Studio](https://obsproject.com/) that allows you to replace the background in portrait images and video, as well as enhance low-light scenes.

<p align="center">
  <a href="https://royshil.github.io/obs-backgroundremoval/">
    <b>⬇️ Download & Install OBS Background Removal ⬇️</b>
  </a>
</p>

Or, browse versions on [releases page](https://github.com/royshil/obs-backgroundremoval/releases).

## Usage

<div style="text-align:center;">
<video src="https://github.com/locaal-ai/obs-backgroundremoval/assets/1067855/5ba5aae2-7ea2-4c90-ad45-fba5ccde1a4e" width="320"></video>
</div>

Check out the [usage guide page](https://royshil.github.io/obs-backgroundremoval/usage/) for usage walkthrough and recommendations.

Additional tutorial videos:

- Usage tutorial: https://youtu.be/veqNEsMqEE0
- Depth of Field effect: https://youtu.be/jC3EKSpNjQk
- Low-light enhancement: https://youtu.be/WSBLYWFrn2Q
- Remove background from ANY object (not just human): https://youtu.be/N74VCDCToX8

## Introduction

This plugin is meant to make it easy to replace the background in portrait images and video.
It is using a neural network to predict the mask of the portrait and remove the background pixels.
It's easily composable with other OBS plugins to replace the background with e.g. an image or
a transparent color.

If you like this work, which is given to you completely free of charge, please consider supporting it by sponsoring us on GitHub:

- https://github.com/sponsors/royshil
- https://github.com/sponsors/umireon

### Support and Help

Reach out to us on [GitHub Discussions](https://github.com/royshil/obs-backgroundremoval/discussions) or the [OBS Plugins forum](https://obsproject.com/forum/resources/background-removal-portrait-segmentation.1260/) for online / immediate help.

If you found a bug or want to suggest a feature or improvement please open an [issue](https://github.com/royshil/obs-backgroundremoval/issues).

If you are looking for hands-on help or private consultation please select a [sponsorship tier](https://github.com/sponsors/royshil?frequency=one-time).

### Technical Details

<!--
GPU support:

- Currently on Windows we support DirectML, which should reduce CPU usage by 95% and effectively use the systems accelerators (GPUs if available).
- On Mac we support CoreML for acceleration, which is available on M1 and M2 (not Intel, sorry).
- CUDA is supported in this plugin through TensorRT, however it is supported only on Linux.
- The goal of this plugin is to be available for everyone on every system, even if they don't own a GPU.
-->

Number of CPU threads is controllable through the UI settings. A 2-thread setting works best.

The pretrained model weights used for portrait foreground segmentation are taken from:

- https://github.com/anilsathyan7/Portrait-Segmentation/tree/master/SINet
- https://github.com/PaddlePaddle/PaddleSeg/tree/release/2.7/contrib/PP-HumanSeg
- https://github.com/PINTO0309/PINTO_model_zoo/tree/main/082_MediaPipe_Meet_Segmentation
- https://github.com/PeterL1n/RobustVideoMatting
- https://github.com/PINTO0309/PINTO_model_zoo/tree/main/384_TCMonoDepth and https://github.com/yu-li/TCMonoDepth
- https://huggingface.co/briaai/RMBG-1.4

Image enhancement (low light) models are taken from:

- https://github.com/PINTO0309/PINTO_model_zoo/tree/main/213_TBEFN
- https://github.com/PINTO0309/PINTO_model_zoo/tree/main/372_URetinex-Net
- https://github.com/PINTO0309/PINTO_model_zoo/tree/main/370_Semantic-Guided-Low-Light-Image-Enhancement
- https://github.com/PINTO0309/PINTO_model_zoo/tree/main/243_Zero-DCE-improved

Some more information about how I built it: https://www.morethantechnical.com/2021/04/15/obs-plugin-for-portrait-background-removal-with-onnx-sinet-model/ and https://www.morethantechnical.com/2023/05/20/building-an-obs-background-removal-plugin-a-walkthrough/

### Code Walkthrough

This video on YouTube will take you through the major parts of the code and explain them.

<div align="center">
  <a href="https://youtu.be/iFQtcJg0Wsk" target="_blank">
    <img width="50%" src="https://img.youtube.com/vi/iFQtcJg0Wsk/maxresdefault.jpg"/>
  </a>
</div>

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/svg?repos=locaal-ai/obs-backgroundremoval&type=Date&theme=dark" />
  <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/svg?repos=locaal-ai/obs-backgroundremoval&type=Date" />
  <img alt="Star History Chart" src="https://api.star-history.com/svg?repos=locaal-ai/obs-backgroundremoval&type=Date" />
</picture>
