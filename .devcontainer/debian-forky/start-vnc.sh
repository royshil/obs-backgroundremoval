#!/bin/sh

# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

set -eu

display_number=1
vnc_directory="${HOME}/.config/tigervnc"
xstartup="${vnc_directory}/xstartup"

mkdir -p "${vnc_directory}"

printf '%s\n' \
  '#!/bin/sh' \
  'unset SESSION_MANAGER' \
  'unset DBUS_SESSION_BUS_ADDRESS' \
  'xterm -fa Monospace &' \
  'exec dbus-run-session -- openbox-session' \
  >"${xstartup}"
chmod 700 "${xstartup}"

if [ -S "/tmp/.X11-unix/X${display_number}" ]; then
  printf 'TigerVNC display :%s is already running.\n' "${display_number}"
  exit 0
fi

if command -v tigervncserver >/dev/null 2>&1; then
  vncserver=tigervncserver
else
  vncserver=vncserver
fi

# Authentication is intentionally disabled because the server only listens on
# the container loopback interface. Access it through VS Code's port forward.
"${vncserver}" ":${display_number}" \
  -localhost yes \
  -SecurityTypes None \
  -geometry 1920x1080 \
  -depth 24 \
  -AcceptSetDesktopSize=0

printf 'TigerVNC is available through the forwarded localhost port 5901.\n'
