# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

import ctypes
import os
import re
import subprocess
import time
from collections.abc import Callable, Iterator
from pathlib import Path

import pyatspi


DEFAULT_TIMEOUT = 30.0
SCREENSHOT_DELAY = 0.25
_screenshot_sequence = 0


def capture_screenshot(operation: str) -> None:
    global _screenshot_sequence

    _screenshot_sequence += 1
    slug = re.sub(r"[^a-z0-9]+", "-", operation.lower()).strip("-") or "operation"
    screenshots = Path(os.environ["GUI_SCENARIO_ARTIFACTS"]) / "screenshots"
    screenshots.mkdir(parents=True, exist_ok=True)
    destination = screenshots / f"{_screenshot_sequence:03d}-{slug}.jpg"

    time.sleep(SCREENSHOT_DELAY)
    subprocess.run(
        [
            "/usr/bin/ffmpeg",
            "-hide_banner",
            "-loglevel",
            "error",
            "-f",
            "x11grab",
            "-video_size",
            "1920x1080",
            "-i",
            os.environ["DISPLAY"],
            "-frames:v",
            "1",
            "-q:v",
            "12",
            "-y",
            str(destination),
        ],
        check=True,
        timeout=10,
    )


def descendants(root=None) -> Iterator:
    if root is None:
        root = pyatspi.Registry.getDesktop(0)

    yield root
    try:
        children = list(root)
    except (LookupError, RuntimeError, ValueError):
        return

    for child in children:
        yield from descendants(child)


def role_name(node) -> str:
    try:
        return node.getRoleName()
    except (LookupError, RuntimeError, ValueError):
        return ""


def node_name(node) -> str:
    try:
        return node.name or ""
    except (LookupError, RuntimeError, ValueError):
        return ""


def find(root=None, *, role: str | None = None, name: str | None = None, contains: bool = False):
    for node in descendants(root):
        if role is not None and role_name(node) != role:
            continue
        if name is not None:
            actual_name = node_name(node)
            if (contains and name not in actual_name) or (not contains and name != actual_name):
                continue
        return node
    return None


def process_is_running(pid: int) -> bool:
    try:
        with open(f"/proc/{pid}/stat", encoding="utf-8") as process_stat:
            fields = process_stat.read().split()
        return len(fields) > 2 and fields[2] != "Z"
    except (FileNotFoundError, PermissionError):
        return False


def wait_for(description: str, predicate: Callable, *, timeout: float = DEFAULT_TIMEOUT, pid: int | None = None):
    deadline = time.monotonic() + timeout
    last_error = None
    while time.monotonic() < deadline:
        if pid is not None and not process_is_running(pid):
            raise RuntimeError(f"OBS exited while waiting for {description}")
        try:
            result = predicate()
            if result:
                return result
        except (LookupError, RuntimeError, ValueError) as error:
            last_error = error
        time.sleep(0.1)

    detail = f": {last_error}" if last_error else ""
    raise TimeoutError(f"Timed out after {timeout:g}s waiting for {description}{detail}")


def invoke(node, preferred_actions=("click", "press")) -> None:
    action = node.queryAction()
    names = [action.getName(index) for index in range(action.nActions)]
    for preferred in preferred_actions:
        for index, name in enumerate(names):
            if name.lower() == preferred:
                if not action.doAction(index):
                    raise RuntimeError(f"AT-SPI action {name!r} failed for {node_name(node)!r}")
                capture_screenshot(f"{name}-{node_name(node)}")
                return
    if action.nActions == 1 and action.doAction(0):
        capture_screenshot(f"{names[0]}-{node_name(node)}")
        return
    raise RuntimeError(f"No usable AT-SPI action for {node_name(node)!r}; available actions: {names!r}")


def press_x11_key(key: str) -> None:
    x11 = ctypes.CDLL("libX11.so.6")
    xtst = ctypes.CDLL("libXtst.so.6")
    x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
    x11.XOpenDisplay.restype = ctypes.c_void_p
    x11.XStringToKeysym.argtypes = [ctypes.c_char_p]
    x11.XStringToKeysym.restype = ctypes.c_ulong
    x11.XKeysymToKeycode.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
    x11.XKeysymToKeycode.restype = ctypes.c_ubyte
    x11.XFlush.argtypes = [ctypes.c_void_p]
    x11.XCloseDisplay.argtypes = [ctypes.c_void_p]
    xtst.XTestFakeKeyEvent.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_int, ctypes.c_ulong]

    display = x11.XOpenDisplay(None)
    if not display:
        raise RuntimeError("Unable to open the X display for keyboard input")
    try:
        keysym = x11.XStringToKeysym(key.encode("ascii"))
        keycode = x11.XKeysymToKeycode(display, keysym)
        if not keysym or not keycode:
            raise RuntimeError(f"Unable to resolve X11 key {key!r}")
        if not xtst.XTestFakeKeyEvent(display, keycode, True, 0):
            raise RuntimeError(f"Unable to press X11 key {key!r}")
        if not xtst.XTestFakeKeyEvent(display, keycode, False, 0):
            raise RuntimeError(f"Unable to release X11 key {key!r}")
        x11.XFlush(display)
    finally:
        x11.XCloseDisplay(display)
    capture_screenshot(f"key-{key}")


def is_checked(node) -> bool:
    return node.getState().contains(pyatspi.STATE_CHECKED)


def text_content(root) -> str:
    parts = []
    for node in descendants(root):
        name = node_name(node).strip()
        if name:
            parts.append(name)
        try:
            text = node.queryText()
            value = text.getText(0, text.characterCount).strip()
            if value and value != name:
                parts.append(value)
        except (LookupError, NotImplementedError, RuntimeError, ValueError):
            pass
    return "\n".join(parts)


def dump_tree(root=None) -> str:
    if root is None:
        root = pyatspi.Registry.getDesktop(0)
    lines = []

    def visit(node, depth: int) -> None:
        lines.append(f"{'  ' * depth}{role_name(node)}: {node_name(node)!r}")
        try:
            children = list(node)
        except (LookupError, RuntimeError, ValueError):
            return
        for child in children:
            visit(child, depth + 1)

    visit(root, 0)
    return "\n".join(lines) + "\n"


def obs_pid() -> int:
    return int(os.environ["OBS_PID"])
