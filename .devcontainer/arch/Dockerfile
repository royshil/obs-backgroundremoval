# syntax=docker/dockerfile:1
# check=skip=FromPlatformFlagConstDisallowed

# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

FROM --platform=linux/amd64 archlinux:base-devel

RUN sed -i '/^\[options\]/a DisableSandbox' /etc/pacman.conf \
    && pacman --sync --refresh --sysupgrade --noconfirm \
    && pacman --sync --needed --noconfirm \
        ca-certificates \
        clang \
        cmake \
        curl \
        file \
        git \
        ninja \
        onnxruntime-cpu \
        opencv \
        obs-studio \
        pkgconf \
        python \
        python-pip \
        sudo \
        tar \
        unzip \
        zip \
    && pacman --sync --clean --clean --noconfirm

COPY --chmod=400 <<'EOF' /etc/sudoers.d/vscode
vscode ALL=(ALL) NOPASSWD: ALL
EOF

RUN groupadd -g 1000 vscode && useradd -u 1000 -g vscode -m vscode -s /bin/bash

USER vscode

RUN python -m venv /home/vscode/.venv && /home/vscode/.venv/bin/pip install --no-cache-dir gersemi==0.21.0

CMD ["/bin/bash"]

ENV PATH="/home/vscode/.venv/bin:$PATH"
