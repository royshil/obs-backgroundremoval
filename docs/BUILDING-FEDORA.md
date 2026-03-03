## Instructions for building the plugin on Fedora

NOTE: These instructions are unofficial and we will appreciate any pull requests to improve them.

Please follow the steps to install obs-backgroundremoval plugin on Fedora.

First, you have to install the development tools:

```
sudo dnf group install development-tools
```

Then, make sure you have the dependencies of this plugin installed:

```
sudo dnf install \
  cmake \
  curl-devel \
  gcc-c++ \
  obs-studio-devel \
  opencv-devel
```

Run the following command to compile the plugin:  

```
cmake -B build_x86_64 \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DENABLE_FRONTEND_API=ON \
  -DENABLE_QT=OFF \
  -DUSE_SYSTEM_OPENCV=ON
cmake --build build_x86_64
```

Finally, install the necessary files into the system directories, by issuing this command:

```
sudo cmake --install build_x86_64 --prefix /usr
```

---

> SPDX-FileCopyrightText: 2021-2026 Roy Shilkrot <roy.shil@gmail.com>  
> SPDX-FileCopyrightText: 2023-2026 Kaito Udagawa <umireon@kaito.tokyo>  
>
> SPDX-License-Identifier: GPL-3.0-or-later  
