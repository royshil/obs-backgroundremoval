#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

import platform
import runpy
import sys
from pathlib import Path

platform.machine = lambda: "AMD64"

sys.argv = sys.argv[1:]
sys.path.insert(0, str(Path(sys.argv[0]).resolve().parent))
runpy.run_path(sys.argv[0], run_name="__main__")
