---
layout: ../../layouts/MarkdownLayout.astro
lang: en
title: How to develop OBS Background Removal on macOS
description: How to develop OBS Background Removal on macOS
---

# Step-by-Step Guide: Developing OBS Background Removal on macOS

Welcome! This guide will walk you through setting up your development environment for OBS Background Removal on macOS.

---

## 1. Install Xcode 16.4

You must use Xcode 16.4 (the App Store version will not work).  
Download it from [Apple Developer Downloads](https://developer.apple.com/download/applications/) (Apple ID required, Developer Program not needed):

```sh
export DEVELOPER_DIR=/Applications/Xcode_16.4.0.app
```

---

## 2. Install System Dependencies

Install [Homebrew](https://brew.sh/) if you haven't already.  
Then, install CMake:

```sh
brew install cmake
```

---

## 3. Clone the Source Code

Get the latest code from GitHub:

```sh
git clone https://github.com/royshil/obs-backgroundremoval.git
cd obs-backgroundremoval
```

---

## 4. Set Up vcpkg

Install vcpkg to manage dependencies:

```sh
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh
export VCPKG_ROOT=~/vcpkg
```

---

## 5. Install Build Dependencies

This step may take 10–20 minutes:

```sh
./.github/scripts/install-vcpkg-macos.bash
```

---

## 6. Download ONNX Runtime

Use CMake to download ONNX Runtime:

```sh
cmake -P cmake/DownloadOnnxruntime.cmake
```

---

## 7. Configure and Build the Project

Configure using the CI preset:

```sh
cmake --preset macos-ci
```

Build using the CI preset:

```sh
cmake --build --preset macos-ci
```

---

## 8. Test the Plugin with System OBS

Install the plugin locally:

```sh
cp -r build_macos/RelWithDebInfo/obs-backgroundremoval.plugin ~/Library/Application\ Support/obs-studio/plugins
```

---

## 9. Lint Your Code

Install the required tools and run linters:

```sh
brew install obsproject/tools/clang-format@19 obsproject/tools/gersemi
./build-aux/run-clang-format
./build-aux/run-gersemi
```

---

You're all set! Happy coding!
