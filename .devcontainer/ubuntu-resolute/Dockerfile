# syntax=docker/dockerfile:1

# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

FROM ubuntu:resolute

RUN apt-get update && apt-get install -y --no-install-recommends \
  build-essential \
  ca-certificates \
  clang-format-19 \
  cmake \
  curl \
  file \
  git \
  libcurl4-openssl-dev \
  libobs-dev \
  libonnxruntime-dev \
  libopencv-dev \
  ninja-build \
  python3.14-venv \
  sudo \
  tar \
  unzip \
  zip \
  && rm -rf /var/lib/apt/lists/*

COPY --chmod=400 <<'EOF' /etc/sudoers.d/vscode
vscode ALL=(ALL) NOPASSWD: ALL
EOF

RUN userdel ubuntu && rm -rf /home/ubuntu && groupadd -g 1000 vscode && useradd -u 1000 -g vscode -m vscode -s /bin/bash

USER vscode

RUN python3.14 -m venv /home/vscode/.venv && /home/vscode/.venv/bin/pip install --no-cache-dir gersemi

CMD ["/bin/bash"]

ENV PATH="/home/vscode/.venv/bin:$PATH"
