---
layout: ../../layouts/MarkdownLayout.astro
lang: en
title: How to develop OBS Background Removal on Ubuntu
description: How to develop OBS Background Removal on Ubuntu
---

# Step-by-Step Guide: Developing OBS Background Removal on Ubuntu

Welcome! This guide will walk you through setting up your development environment for OBS Background Removal on Ubuntu.

---

## 1. Install System Dependencies

Open your terminal and run:

```sh
sudo apt install build-essential zsh cmake git curl zip unzip tar
```

---

## 2. Clone the Source Code

Get the latest code from GitHub:

```sh
git clone https://github.com/royshil/obs-backgroundremoval.git
cd obs-backgroundremoval
```

---

## 3. Set Up vcpkg

Install vcpkg to manage dependencies:

```sh
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh
export VCPKG_ROOT=~/vcpkg
```

---

## 4. Install Build Dependencies

This step may take 10–20 minutes:

```sh
${VCPKG_ROOT}/vcpkg install --triplet x64-linux-obs
```

---

## 5. Download ONNX Runtime

Use CMake to download ONNX Runtime:

```sh
cmake -P cmake/DownloadOnnxruntime.cmake
```

---

## 6. Build the Project

Build using the provided CI scripts:

```sh
./.github/scripts/build-ubuntu --target ubuntu-x86_64 --config RelWithDebInfo
```

---

## 7. Test the Plugin with System OBS

Install the plugin locally:

```sh
sudo cmake --install build_x86_64
```

---

## 8. Package the Plugin

Create a Debian package:

```sh
./.github/scripts/package-ubuntu --target ubuntu-x86_64 --config RelWithDebInfo --package
```

---

## 9. Test the Package Installation

Install the generated package:

```sh
sudo dpkg -i release/obs-backgroundremoval-*-x86_64-linux-gnu.deb
```

---

## 10. Lint Your Code

Install the required tools and run linters:

```sh
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
eval "$(/home/linuxbrew/.linuxbrew/bin/brew shellenv)"
brew install obsproject/tools/clang-format@19 obsproject/tools/gersemi
export PATH="/home/linuxbrew/.linuxbrew/opt/clang-format@19/bin:$PATH"
./build-aux/run-clang-format
./build-aux/run-gersemi
```

---

You're all set! Happy coding!
