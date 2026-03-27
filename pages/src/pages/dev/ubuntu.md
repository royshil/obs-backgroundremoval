---
# SPDX-FileCopyrightText: 2021-2026 Roy Shilkrot <roy.shil@gmail.com>
# SPDX-FileCopyrightText: 2023-2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: GPL-3.0-or-later

layout: ../../layouts/MarkdownLayout.astro
pathname: dev/ubuntu
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
sudo apt install build-essential zsh cmake ninja-build git curl zip unzip tar python3 python3-pip
```

Then add the OBS Studio PPA and install the required OBS and Qt6 development packages:

```sh
sudo add-apt-repository ppa:obsproject/obs-studio
sudo apt-get update
sudo apt-get install -y \
  libgles2-mesa-dev \
  libqt6svg6-dev \
  libsimde-dev \
  obs-studio \
  qt6-base-dev \
  qt6-base-private-dev
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
${VCPKG_ROOT}/vcpkg install --x-install-root=./.deps_vendor/vcpkg_installed --triplet x64-linux-obs
```

---

## 5. Build ONNX Runtime

Install Python build requirements, then build ONNX Runtime from source (this may take a significant amount of time):

```sh
pip install --user -r requirements-build.txt
./scripts/build_ort_ubuntu.sh
```

---

## 6. Build the Project

Configure and build using the provided CMake preset:

```sh
cmake --preset ubuntu-x86_64
cmake --build --preset ubuntu-x86_64
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
cd build_x86_64
cpack -G DEB
cd ..
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
