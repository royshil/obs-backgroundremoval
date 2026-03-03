## Building on openSUSE

NOTE: These instructions are unofficial and we will appreciate any pull requests to improve them.

Please follow the steps to install obs-backgroundremoval plugin on OpenSUSE.

### Install OBS

https://obsproject.com/wiki/unofficial-linux-builds#opensuse

```
sudo zypper ar -cfp 90 http://ftp.gwdg.de/pub/linux/misc/packman/suse/openSUSE_Tumbleweed/ packman
sudo zypper dup --from packman --allow-vendor-change
sudo zypper in obs-studio
```

### Building this plugin

```
sudo zypper install -t pattern devel_basis
sudo zypper install zsh cmake Mesa-libGL-devel \
  ffmpeg-6-libavcodec-devel ffmpeg-6-libavdevice-devel ffmpeg-6-libavformat-devel \
  libcurl-devel Mesa-libEGL-devel \
  libpulse-devel libxkbcommon-devel
sudo zypper in cmake gcc12-c++ ninja obs-studio-devel opencv-devel qt6-base-devel zsh curl-devel jq

cmake . -B build_x86_64 \
  -DCMAKE_C_COMPILER=gcc-12 \
  -DCMAKE_CXX_COMPILER=g++-12 \
  -DQT_VERSION=6 \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DENABLE_FRONTEND_API=ON \
  -DENABLE_QT=ON
cmake --build build_x86_64
sudo cmake --install build_x86_64 --prefix /usr
```

---

> SPDX-FileCopyrightText: 2021-2026 Roy Shilkrot <roy.shil@gmail.com>  
> SPDX-FileCopyrightText: 2023-2026 Kaito Udagawa <umireon@kaito.tokyo>  
>
> SPDX-License-Identifier: GPL-3.0-or-later  
