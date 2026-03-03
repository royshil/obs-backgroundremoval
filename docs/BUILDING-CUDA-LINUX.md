# Building with a newer version of CUDA (Linux)
This guide provides instructions for compiling the `obs-backgroundremoval` plugin on Linux, specifically for users who have a newer version of the NVIDIA CUDA Toolkit installed than what is officially supported by the ONNXRuntime pre-compiled binaries.

## Problem
The `obs-backgroundremoval` plugin relies on ONNXRuntime for AI processing. The pre-built versions of ONNXRuntime are often compiled for older CUDA versions. When you have a newer CUDA toolkit, this mismatch can cause errors in OBS, such as:

```
[ONNXRuntimeError] : 1 : FAIL : Failed to load library libonnxruntime_providers_tensorrt.so with error: libcublas.so.12: cannot open shared object file: No such file or directory
```

This error means that ONNXRuntime is looking for an older version of a CUDA library (`libcublas.so.12` in this example) and cannot find it because your system has a newer version. The solution is to compile ONNXRuntime from source against your specific CUDA version.


## 1. Prerequisites
Before you begin, you must have the necessary software installed and configured on your system. This guide assumes you have already installed:
*   CUDA Toolkit
*   cuDNN
*   TensorRT

Make sure these are installed correctly before compiling.

## 2. Compile ONNXRuntime from Source

We will now download the ONNXRuntime source code and build it specifically for your system's hardware and software configuration.

### Clone the ONNXRuntime Repository

First, open a terminal and clone the official ONNXRuntime repository from GitHub.
```bash
git clone --recursive https://github.com/microsoft/onnxruntime.git
cd onnxruntime
```

### Determine Your GPU's CUDA Architecture

You need to tell the compiler which CUDA architecture your GPU belongs to. You can find this with the following command:

```bash
nvidia-smi --query-gpu=compute_cap --format=csv,noheader
```

The command will output a version number, for example, `8.6` for an NVIDIA RTX 30-series GPU. You will need to use this value in the build command, but without the dot. For example, `8.6` becomes `86`.

Here are some common architecture values:
*   **Turing (RTX 20-series):** `75`
*   **Ampere (RTX 30-series):** `80`, `86`, `87`
*   **Ada Lovelace (RTX 40-series):** `89`
*   **Hopper:** `90`

### Locate CUDA and TensorRT Installation Paths

The build script needs to know where your CUDA and TensorRT installations are located.

*   **Find your CUDA path:**
    ```bash
    which nvcc
    ```
    The output will be similar to `/opt/cuda/bin/nvcc`. Your `CUDA_HOME` path is the part before `/bin/nvcc` (e.g., `/opt/cuda`).

*   **Find your TensorRT path:** You can search for a key TensorRT file to find its location.
    ```bash
    sudo find / -name "libnvinfer.so"
    ```
    A typical output might be `/usr/lib/libnvinfer.so`. The `TENSORRT_HOME` path would then be the root of this installation, which is `/usr` in this case.

### Build ONNXRuntime
Now you can run the build script. Replace the placeholder values in brackets `[]` with the paths and architecture version you found in the previous steps.
```bash
./build.sh --config Release --build_shared_lib --use_vcpkg --parallel \
--use_cuda --cuda_home [YOUR_CUDA_PATH] --cudnn_home [YOUR_CUDA_PATH] \
--use_tensorrt --tensorrt_home [YOUR_TENSORRT_PATH] \
--cmake_extra_defines CMAKE_CUDA_ARCHITECTURES=[YOUR_ARCH_VERSION] CMAKE_POSITION_INDEPENDENT_CODE=ON onnxruntime_BUILD_UNIT_TESTS=OFF
```

Example for an RTX 3060:
```bash
./build.sh --config Release --build_shared_lib --use_vcpkg --parallel \
--use_cuda --cuda_home /opt/cuda --cudnn_home /opt/cuda \
--use_tensorrt --tensorrt_home /usr \
--cmake_extra_defines CMAKE_CUDA_ARCHITECTURES=86 CMAKE_POSITION_INDEPENDENT_CODE=ON onnxruntime_BUILD_UNIT_TESTS=OFF
```

This process can take a significant amount of time depending on your computer.

### Install the Compiled Libraries

Once the build is complete, the new library files will be located in the `build/Linux/Release` directory.

Navigate to this directory:
```bash
cd build/Linux/Release
```

You should see several files, including `libonnxruntime.so`, `libonnxruntime.so.x.y.z` (where x.y.z is the version), and the provider libraries.

To make these libraries available to other applications on your system, copy them into a standard location and update the system's linker cache.

```bash
sudo cp libonnxruntime.so* /usr/local/lib/
sudo cp libonnxruntime_providers_*.so /usr/local/lib/
sudo ldconfig
```
The `ldconfig` command refreshes the shared library cache.

## 3. Compile obs-backgroundremoval

With your custom version of ONNXRuntime installed, you can now compile the OBS plugin.

### Clone and Build

```bash
git clone https://github.com/royshil/obs-backgroundremoval.git
cd obs-backgroundremoval
```

Now, run CMake to prepare the build and then install it.
```bash
cmake -B build -S . \
  -DCMAKE_BUILD_TYPE=Release \
  -DUSE_SYSTEM_ONNXRUNTIME=ON

sudo cmake --install build
```

## 4. Verification

If the steps were completed successfully, the `obs-backgroundremoval` plugin is now installed. Launch OBS Studio, and you should find the "Background Removal" filter available when you right-click on a video source and select "Filters". It should now function correctly without the previous CUDA-related errors.

---

> SPDX-FileCopyrightText: 2021-2026 Roy Shilkrot <roy.shil@gmail.com>  
> SPDX-FileCopyrightText: 2023-2026 Kaito Udagawa <umireon@kaito.tokyo>  
>
> SPDX-License-Identifier: GPL-3.0-or-later  
