---
layout: ../../layouts/MarkdownLayout.astro
lang: en
title: How to develop OBS Background Removal on Ubuntu
description: How to develop OBS Background Removal on Ubuntu
---
# How to develop it on Ubuntu

## Install system dependencies

```
sudo apt install build-essential zsh cmake git curl zip unzip tar
```

## Clone the source
```
git clone https://github.com/royshil/obs-backgroundremoval.git
cd obs-backgroundremoval
```

## Install vcpkg

```
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh
export VCPKG_ROOT=~/vcpkg
```

## Install build dependencies with vcpkg
It takes 10-20 minutes.
```
${VCPKG_ROOT}/vcpkg install --triplet x64-linux-obs
``` 

## Download ONNX Runtime with CMake
```
cmake -P cmake/DownloadOnnxruntime.cmake
```


## Build with CI scripts

```
./.github/scripts/build-ubuntu --target ubuntu-x86_64 --config RelWithDebInfo
```

## Test plugin with system OBS

```
sudo cmake --install build_x86_64
```

## Package the plugin

```
./.github/scripts/package-ubuntu --target ubuntu-x86_64 --config RelWithDebInfo --package
```

## Test the package installatino

```
sudo dpkg -i release/obs-backgroundremoval-*-x86_64-linux-gnu.deb
```

## Lint

```
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
eval "$(/home/linuxbrew/.linuxbrew/bin/brew shellenv)"
brew install obsproject/tools/clang-format@19 obsproject/tools/gersemi
export PATH="/home/linuxbrew/.linuxbrew/opt/clang-format@19/bin:$PATH"
./build-aux/run-clang-format
./build-aux/run-gersemi
```
