# OBS Background Removal - Developer Guide for Coding Agents

## Project Overview

**Repository**: OBS Studio plugin for AI-powered background removal and low-light enhancement  
**Languages**: C17, C++17  
**Build System**: CMake 3.16+, vcpkg for dependencies  
**Target Platforms**: Windows (x64), macOS (Universal), Ubuntu 24.04+ (x86_64)  
**OBS Version**: Requires OBS Studio 31.1.1+  
**Key Dependencies**: ONNX Runtime 1.23.2, OpenCV4, CURL

This plugin uses neural networks to remove backgrounds from video sources without a green screen. Processing is local with CoreML (macOS), DirectML/CUDA/TensorRT (Windows), and CUDA/ROCm (Linux) acceleration support.

## Project Structure

**Root Configuration Files**:
- `buildspec.json` - Version and metadata (edit with `jq` only)
- `CMakeLists.txt` - Main build configuration
- `CMakePresets.json` - Platform-specific build presets
- `vcpkg.json` - Dependency manifest (opencv4, curl)
- `.clang-format` - C/C++ formatting (requires clang-format 19.1.1)
- `.gersemirc` - CMake formatting (gersemi 0.12.0+)

**Source Code** (`src/`):
- `plugin-main.c` - OBS plugin entry point
- `background-filter.{cpp,h}` - Main background removal filter implementation
- `enhance-filter.{cpp,h}` - Low-light enhancement filter
- `obs-utils/` - OBS integration utilities
- `ort-utils/` - ONNX Runtime session management
- `update-checker/` - Version checking utilities

**Build Infrastructure**:
- `.github/workflows/` - CI/CD pipelines (push.yaml, pr-pull.yaml, check-format.yaml)
- `.github/scripts/` - Build/package scripts (build-ubuntu, build-macos, Build-Windows.ps1)
- `.github/actions/` - Reusable GitHub Actions (build-plugin, package-plugin)
- `cmake/` - Platform-specific CMake modules and DownloadOnnxruntime.cmake
- `build-aux/` - Formatting scripts (run-clang-format, run-gersemi)

**Data** (`data/`):
- `models/` - Neural network model files (.onnx)
- `effects/` - Shader files
- `locale/` - Translation files

## Build & Test Instructions

### Prerequisites (Ubuntu CI Environment)
```bash
# System dependencies
sudo apt install build-essential cmake git curl

# Install formatting tools from OBS Linuxbrew repository
eval "$(/home/linuxbrew/.linuxbrew/bin/brew shellenv)"
brew install obsproject/tools/clang-format@19
brew install obsproject/tools/gersemi
```

### Building on Ubuntu (Local Development)

**Step 1: Set up vcpkg**
```bash
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh
export VCPKG_ROOT=~/vcpkg
```

**Step 2: Install dependencies (10-20 minutes)**
```bash
${VCPKG_ROOT}/vcpkg install --triplet x64-linux-obs
```

**Step 3: Download ONNX Runtime**
```bash
cmake -P cmake/DownloadOnnxruntime.cmake
```

**Step 4: Configure and build**
```bash
cmake --preset ubuntu-ci-x86_64
cmake --build --preset ubuntu-ci-x86_64
```

Alternative: Use the provided script:
```bash
./.github/scripts/build-ubuntu --target ubuntu-x86_64 --config RelWithDebInfo
```

**Step 5: Install locally for testing**
```bash
sudo cmake --install build_x86_64
```

### Code Formatting (CRITICAL - Always Run Before Committing)

**Format C/C++ files (clang-format 19.1.1 required)**:
```bash
./build-aux/run-clang-format
```
This formats all `*.c`, `*.cpp`, `*.h`, `*.hpp`, `*.m`, `*.mm` files in `src/`.

**Format CMake files (gersemi 0.12.0+ required)**:
```bash
./build-aux/run-gersemi
```
This formats `CMakeLists.txt` and all `*.cmake` files.

**Check formatting without modifying files**:
```bash
./build-aux/run-clang-format --check
./build-aux/run-gersemi --check
```

**IMPORTANT**: The CI will fail if formatting is incorrect. Always format before pushing.

### macOS Build Notes
- Requires Xcode 16.4 (not App Store version)
- Use `.github/scripts/install-vcpkg-macos.bash` for dependency installation
- Build: `cmake --preset macos-ci && cmake --build --preset macos-ci`
- Install locally: `cp -r build_macos/RelWithDebInfo/obs-backgroundremoval.plugin ~/Library/Application\ Support/obs-studio/plugins`

### Windows Build Notes
- Requires Visual Studio 2022 with C++ workload and Windows SDK 10.0.22621
- Use vcpkg triplet: `x64-windows-static-md-obs`
- Build with: `.github/scripts/Build-Windows.ps1 -Target x64 -Configuration RelWithDebInfo`
- GPU build: Add `-DGPU=ON` when downloading ONNX Runtime

## CI/CD Pipeline

### On Pull Request (pr-pull.yaml)
1. **Format Check** - Runs clang-format and gersemi checks (must pass)
2. **Build** - Builds for all platforms (Windows x64, macOS Universal, Ubuntu x86_64)

### On Push to main (push.yaml)
1. **Format Check** - Same as PR
2. **Build** - Builds all platforms with packaging
3. **Create Release** - On tag push (format: `1.2.3` or `1.2.3-beta1`), creates draft GitHub release

### On Tag Push (version format: `X.Y.Z` or `X.Y.Z-beta`)
- Automatically packages and creates a draft release
- Uploads installers for all platforms
- Generates checksums

## Common Issues & Solutions

### Formatting Failures
**Problem**: CI fails with "requires formatting changes"
**Solution**: Run `./build-aux/run-clang-format` and `./build-aux/run-gersemi` locally before committing

### Build Failures - Missing Dependencies
**Problem**: CMake cannot find libobs, OpenCV, or curl
**Solution**: Ensure vcpkg dependencies are installed: `${VCPKG_ROOT}/vcpkg install --triplet x64-linux-obs`

### ONNX Runtime Not Found
**Problem**: Build fails with ONNX Runtime errors
**Solution**: Run `cmake -P cmake/DownloadOnnxruntime.cmake` before configuring CMake

### vcpkg Build Times Out
**Problem**: vcpkg install takes too long or times out
**Solution**: This is expected for first build (10-20 minutes). Subsequent builds use cache.

## Development Workflow

1. **Make changes** to C/C++ or CMake files
2. **Format code**: Run `./build-aux/run-clang-format` and `./build-aux/run-gersemi`
3. **Build**: `cmake --build --preset ubuntu-ci-x86_64` (or appropriate preset)
4. **Test**: Install locally and test with OBS Studio
5. **Commit**: Ensure formatting is correct before committing

## Key Facts

- **Default branch**: `main`
- **No unit tests**: This project has no automated test suite. Manual testing with OBS is required.
- **Language standards**: C17 for `.c` files, C++17 for `.cpp` files
- **Build time**: Initial vcpkg dependency build: 10-20 minutes. Incremental builds: 1-3 minutes.
- **Code style**: Follow existing patterns. Formatting is enforced via clang-format.
- **GPU Support**: Windows GPU build uses different ONNX Runtime (download with `-DGPU=ON`)

## Release Automation

To start a new release, the user will instruct Copilot (e.g., "Start release"). Copilot must follow these steps:

### 1. Specify New Version
- **Show the current version** (from `buildspec.json`)
- **Ask the user** for the new version number (e.g., `1.0.0`, `1.0.0-beta1`)
- **Validate**: The version must follow Semantic Versioning (`MAJOR.MINOR.PATCH`)

### 2. Prepare & Update `buildspec.json`
- **Check**: Confirm you are on the `main` branch and it is up-to-date with the remote
- **Create a new branch**: Name it `releases/bump-X.Y.Z` (replace with the new version)
- **Update the version** in `buildspec.json` using `jq`:
  ```bash
  jq '.version = "<new_version>"' buildspec.json > buildspec.json.tmp && mv buildspec.json.tmp buildspec.json
  ```
  *Do not manually edit the file; always use `jq`.*

### 3. Create & Merge Pull Request (PR)
- **Create a PR** for the version update branch
- **Provide the PR URL** to the user
- **Instruct the user** to merge the PR
- **Wait** for user confirmation that the PR is merged

### 4. Push Git Tag
- **Trigger**: Only proceed when the user confirms the PR is merged
- **Switch to `main`** branch
- **Sync with remote**
- **Verify** the `buildspec.json` version matches the intended release
- **Push the Git tag**: Tag must be `X.Y.Z` (no `v` prefix)
- *Pushing the tag triggers the automated release workflow*

### 5. Finalize Release
- **Provide the GitHub releases URL**
- **Instruct the user** to complete the release process on GitHub

---

**Trust these instructions**. Only search for additional information if these instructions are incomplete or incorrect.

<!--
SPDX-FileCopyrightText: 2021-2026 Roy Shilkrot <roy.shil@gmail.com>
SPDX-FileCopyrightText: 2023-2026 Kaito Udagawa <umireon@kaito.tokyo>

SPDX-License-Identifier: GPL-3.0-or-later
-->