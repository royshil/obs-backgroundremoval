# Contributing to OBS Background Removal

Thank you for your interest in contributing to OBS Background Removal! This document provides guidelines and instructions for contributing to the project.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [How Can I Contribute?](#how-can-i-contribute)
- [Development Setup](#development-setup)
  - [Prerequisites](#prerequisites)
  - [Building the Plugin](#building-the-plugin)
    - [macOS](#macos)
    - [Linux](#linux)
    - [Windows](#windows)
- [Code Formatting](#code-formatting)
  - [C/C++ Files](#cc-files)
  - [CMake Files](#cmake-files)
- [Development Workflow](#development-workflow)
- [Pull Request Process](#pull-request-process)
- [Additional Resources](#additional-resources)

## Code of Conduct

This project and everyone participating in it is governed by our commitment to providing a welcoming and inclusive environment. Please be respectful and constructive in all interactions.

## How Can I Contribute?

There are many ways to contribute to this project:

- **Report bugs**: Use the [bug report template](https://github.com/locaal-ai/obs-backgroundremoval/issues/new?template=bug_report.md)
- **Suggest features**: Use the [feature request template](https://github.com/locaal-ai/obs-backgroundremoval/issues/new?template=feature_request.md)
- **Submit pull requests**: Fix bugs, add features, or improve documentation
- **Improve documentation**: Help make our docs more clear and comprehensive
- **Help other users**: Answer questions in [discussions](https://github.com/locaal-ai/obs-backgroundremoval/discussions) or on [Discord](https://discord.gg/KbjGU2vvUz)

## Development Setup

### Prerequisites

This project uses:
- **C17** for C code
- **C++17** for C++ code
- **CMake** (version 3.16 or later) for building
- **OBS Studio** development libraries
- **ONNX Runtime** for machine learning inference
- **OpenCV** for image processing
- **libcurl** for network requests

### Building the Plugin

The plugin has been built and tested on macOS (Intel & Apple Silicon), Windows, and several Linux distributions. The CI pipeline scripts handle most of the complexity.

Start by cloning the repository:

```bash
git clone https://github.com/locaal-ai/obs-backgroundremoval.git
cd obs-backgroundremoval
```

#### macOS

The build script creates a universal binary for both Intel and Apple Silicon by default.

**Build the plugin:**

```bash
./.github/scripts/build-macos -c Release
```

To build for a specific architecture, see `.github/scripts/build-macos` for the `-arch` options.

**Install the plugin:**

The plugin files (e.g., `obs-backgroundremoval.plugin`) will be in `./release/Release`. Copy the `.plugin` file to the OBS plugins directory:

```bash
cp -r ./release/Release/obs-backgroundremoval.plugin ~/Library/Application\ Support/obs-studio/plugins/
```

**Create installer package:**

```bash
./.github/scripts/package-macos -c Release
```

> **Note**: If outputs are in the `Release` folder instead of `install`, you may need to rename `build_x86_64/Release` to `build_x86_64/install`.

#### Linux

##### Ubuntu

**Build the plugin:**

```bash
./.github/scripts/build-ubuntu
```

The build script will handle dependencies and create the plugin in the appropriate directory.

**Installation:**

If you installed OBS via:
- **Official PPA**: Download the `.deb` package from [releases](https://github.com/locaal-ai/obs-backgroundremoval/releases) and install it
- **FlatHub**: Run `flatpak install com.obsproject.Studio.Plugin.BackgroundRemoval`

##### Arch Linux

The community maintains AUR packages: [obs-backgroundremoval on AUR](https://aur.archlinux.org/packages/obs-backgroundremoval)

##### Fedora

**Install dependencies:**

```bash
sudo dnf group install development-tools
sudo dnf install cmake curl-devel gcc-c++ obs-studio-devel opencv-devel
```

**Build the plugin:**

```bash
cmake -B build_x86_64 \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DENABLE_FRONTEND_API=ON \
  -DENABLE_QT=OFF \
  -DUSE_SYSTEM_OPENCV=ON

cmake --build build_x86_64
```

**Install:**

```bash
sudo cmake --install build_x86_64 --prefix /usr
```

For more details, see [`docs/BUILDING-FEDORA.md`](docs/BUILDING-FEDORA.md).

##### openSUSE

**Install dependencies:**

```bash
sudo zypper install -t pattern devel_basis
sudo zypper install zsh cmake Mesa-libGL-devel \
  ffmpeg-6-libavcodec-devel ffmpeg-6-libavdevice-devel ffmpeg-6-libavformat-devel \
  libcurl-devel Mesa-libEGL-devel libpulse-devel libxkbcommon-devel
sudo zypper in cmake gcc12-c++ ninja obs-studio-devel opencv-devel qt6-base-devel zsh curl-devel jq
```

**Build the plugin:**

```bash
cmake . -B build_x86_64 \
  -DCMAKE_C_COMPILER=gcc-12 \
  -DCMAKE_CXX_COMPILER=g++-12 \
  -DQT_VERSION=6 \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DENABLE_FRONTEND_API=ON \
  -DENABLE_QT=ON

cmake --build build_x86_64
```

**Install:**

```bash
sudo cmake --install build_x86_64 --prefix /usr
```

For more details, see [`docs/BUILDING-OPENSUSE.md`](docs/BUILDING-OPENSUSE.md).

##### FlatHub

For other distributions, use FlatHub:

```bash
flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo
flatpak install flathub com.obsproject.Studio
flatpak install flathub com.obsproject.Studio.Plugin.BackgroundRemoval
```

#### Windows

**Build the plugin:**

Use PowerShell to run the build script:

```powershell
.\.github\scripts\Build-Windows.ps1 -Target x64 -CMakeGenerator "Visual Studio 17 2022"
```

**Install the plugin:**

The build output will be in the `./release` folder. Manually copy the files to your OBS Studio installation directory.

**Create installer package:**

```powershell
.\.github\scripts\Package-Windows.ps1 -Target x64
```

## Code Formatting

Proper code formatting is enforced in this project. **All code must be formatted before submitting a pull request.**

### C/C++ Files

We use **clang-format version 19** to format C and C++ files.

**Format all C/C++ files:**

```bash
find src -name "*.c" -o -name "*.cpp" -o -name "*.h" -o -name "*.hpp" | xargs clang-format-19 -i
```

**Format a specific file:**

```bash
clang-format-19 -i path/to/file.cpp
```

The formatting rules are defined in [`.clang-format`](.clang-format).

### CMake Files

We use **gersemi** to format CMake files.

**Install gersemi:**

```bash
pip install gersemi
```

**Format all CMake files:**

```bash
gersemi -i CMakeLists.txt $(find cmake -name "*.cmake")
```

**Format a specific file:**

```bash
gersemi -i path/to/CMakeLists.txt
```

The formatting configuration is in [`.gersemirc`](.gersemirc).

### Checking Format Before Committing

The CI pipeline automatically checks code formatting. To verify your changes locally before pushing:

**Check C/C++ formatting:**

```bash
find src -name "*.c" -o -name "*.cpp" -o -name "*.h" -o -name "*.hpp" | xargs clang-format-19 --dry-run --Werror
```

**Check CMake formatting:**

```bash
gersemi --check CMakeLists.txt $(find cmake -name "*.cmake")
```

## Development Workflow

1. **Fork the repository** and create a new branch from `main`
2. **Make your changes** following the coding standards
3. **Format your code** using the tools described above
4. **Test your changes** by building and running the plugin
5. **Commit your changes** with clear, descriptive commit messages
6. **Push to your fork** and submit a pull request

### Coding Standards

- Use **C17** for C code and **C++17** for C++ code
- Follow the existing code style in the project
- Write clear, self-documenting code with comments where necessary
- Keep functions focused and reasonably sized
- Add error handling where appropriate

### Testing

Before submitting a pull request:

1. **Build the plugin** on your platform to ensure it compiles without errors
2. **Test the plugin** in OBS Studio to verify it works as expected
3. **Check for regressions** by testing existing functionality
4. If adding new features, test edge cases and error conditions

## Pull Request Process

1. **Update documentation** if you're adding or changing features
2. **Format all code** using clang-format-19 and gersemi
3. **Ensure the build succeeds** on your local machine
4. **Write a clear PR description** explaining:
   - What changes you made
   - Why you made them
   - How to test them
5. **Link related issues** using keywords like "Fixes #123"
6. **Be responsive to feedback** and make requested changes promptly
7. **Keep PRs focused** - one feature or fix per PR when possible

### PR Checklist

Before submitting, verify:

- [ ] Code is properly formatted with clang-format-19 and gersemi
- [ ] Code compiles without warnings or errors
- [ ] Plugin has been tested in OBS Studio
- [ ] Documentation has been updated (if applicable)
- [ ] Commit messages are clear and descriptive
- [ ] PR description explains the changes

## Additional Resources

- **Documentation**: [docs/](docs/)
- **Bug Reporting Guide**: [docs/BUG-REPORTING.md](docs/BUG-REPORTING.md)
- **Uninstall Instructions**: [docs/UNINSTALL.md](docs/UNINSTALL.md)
- **Project Website**: https://locaal-ai.github.io/obs-backgroundremoval/
- **Discord Community**: https://discord.gg/KbjGU2vvUz
- **OBS Plugins Forum**: https://obsproject.com/forum/resources/background-removal-portrait-segmentation.1260/

## Questions?

If you have questions about contributing, feel free to:
- Open a [discussion](https://github.com/locaal-ai/obs-backgroundremoval/discussions)
- Ask in our [Discord server](https://discord.gg/KbjGU2vvUz)
- Check existing [issues](https://github.com/locaal-ai/obs-backgroundremoval/issues) and [pull requests](https://github.com/locaal-ai/obs-backgroundremoval/pulls)

Thank you for contributing to OBS Background Removal! 🎉
