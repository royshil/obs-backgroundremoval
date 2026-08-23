#!/bin/bash

# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

set -euo pipefail

usage() {
  printf 'Usage: %s [run_all]\n' "$0"
}

if [[ ! -f VERSION ]]; then
  printf 'ERROR: This script MUST be run from the root of the source tree.\n' >&2
  usage >&2
  exit 1
fi

### Helper functions

require_command() {
  if ! command -v "$1" >/dev/null 2>&1; then
    printf 'ERROR: Required command not found: %s\n' "$1" >&2
    exit 69
  fi
}

report_test_event() {
  event=$1
  test_name=$2
  printf 'TEST %s: %s\n' "${event}" "${test_name}"
  printf 'TEST %s: %s\n' "${event}" "${test_name}" >>"${GUI_SCENARIO_ARTIFACTS}/test-results.txt"
}

capture_results() {
  status=$1
  mkdir -p "${GUI_SCENARIO_ARTIFACTS}"
  printf 'exit_code=%d\n' "${status}" >"${GUI_SCENARIO_ARTIFACTS}/result.txt"
  cp -- "${GUI_SCENARIO_WORKDIR}"/obs-*.log "${GUI_SCENARIO_ARTIFACTS}/" 2>/dev/null || true
  cp -R -- "${XDG_CONFIG_HOME}" "${GUI_SCENARIO_ARTIFACTS}/config" 2>/dev/null || true
  if ((status == 0)); then
    return
  fi
  timeout 10s /usr/bin/python3 "${GUI_SCENARIO_HELPERS}/dump_accessibility.py" \
    >"${GUI_SCENARIO_ARTIFACTS}/accessibility-tree.txt" 2>&1 || true
  timeout 10s ffmpeg -hide_banner -loglevel error -f x11grab -video_size 1920x1080 \
    -i "${DISPLAY:?}" -frames:v 1 -q:v 12 -y "${GUI_SCENARIO_ARTIFACTS}/screen.jpg" || true
}

session_cleanup() {
  status=$?
  trap - EXIT
  if ((status != 0)); then
    report_test_event 'RESULT' "${GUI_SCENARIO_TEST_NAME}: FAILED"
  fi
  capture_results "${status}"
  rm -rf -- "${GUI_SCENARIO_WORKDIR}"
  exit "${status}"
}

### Subcommands

run_scenario() {
  require_command ffmpeg
  require_command obs
  require_command timeout
  require_command python3

  workdir=$(mktemp -d -t obs-backgroundremoval-gui.XXXXXX)
  artifacts_root="${PLUGIN_BUILD_DIR:-${PWD}}/build_test_reports"
  mkdir -p "${artifacts_root}"
  artifacts_dir=$(mktemp -d \
    "${artifacts_root}/$(date -u +%Y%m%dT%H%M%SZ)-run.XXXXXX")
  printf 'GUI scenario test run directory: %s\n' "${artifacts_dir}"

  export GUI_SCENARIO_ARTIFACTS="${artifacts_dir}"
  export GUI_SCENARIO_HELPERS="${PWD}/tests/Scenarios/helpers"
  export GUI_SCENARIO_TEST_NAME=2000_linux-about-dialog-on-startup.test.py
  export GUI_SCENARIO_TEST_PROGRAM="${PWD}/tests/Scenarios/${GUI_SCENARIO_TEST_NAME}"
  export GUI_SCENARIO_VERSION
  GUI_SCENARIO_VERSION=$(<VERSION)
  export GUI_SCENARIO_WORKDIR="${workdir}"
  export LANG=C.UTF-8
  export LC_ALL=C.UTF-8
  export LIBGL_ALWAYS_SOFTWARE=1
  export NO_AT_BRIDGE=0
  export QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1
  export XDG_CONFIG_HOME="${workdir}/config"

  trap session_cleanup EXIT

  if [[ "${CI+set}" != 'set' ]]; then
    cmake --install "${PLUGIN_BUILD_DIR:-${PWD:?}}/build" \
      --prefix "${XDG_CONFIG_HOME}/obs-studio/plugins/obs-backgroundremoval" \
      --component UserPlugin >/dev/null 2>&1
  fi

  mkdir -p \
    "${XDG_CONFIG_HOME}/obs-studio/basic/profiles/obs-backgroundremoval-dev-720p"
  cp -- tests/Scenarios/0010-profile-720p-basic.ini \
    "${XDG_CONFIG_HOME}/obs-studio/basic/profiles/obs-backgroundremoval-dev-720p/basic.ini"
  cp -- tests/Scenarios/0011-720p-user.ini \
    "${XDG_CONFIG_HOME}/obs-studio/user.ini"

  report_test_event 'START' "${GUI_SCENARIO_TEST_NAME}"
  printf '%s\n' "${GUI_SCENARIO_TEST_NAME}" >>"${GUI_SCENARIO_ARTIFACTS}/tests.txt"
  /usr/bin/python3 "${GUI_SCENARIO_TEST_PROGRAM}"
  report_test_event 'RESULT' "${GUI_SCENARIO_TEST_NAME}: PASSED"
  printf 'GUI scenario test run passed (1 test).\n'
}

run_all() {
  require_command dbus-run-session

  if [[ -n "${DISPLAY:-}" ]]; then
    printf 'Using existing X display: %s\n' "${DISPLAY}"
    dbus-run-session -- /bin/bash --noprofile --norc "$0" run_scenario
  else
    require_command xvfb-run

    printf 'DISPLAY is unset; starting Xvfb.\n'
    dbus-run-session -- xvfb-run -a -s '-screen 0 1920x1080x24' \
      /bin/bash --noprofile --norc "$0" run_scenario
  fi
}

if (($# == 0)); then
  run_all
  exit 0
fi

case "${1:-}" in
run_all)
  shift
  run_all "$@"
  ;;
run_scenario)
  shift
  run_scenario "$@"
  ;;
--help)
  usage
  exit 0
  ;;
*)
  printf 'ERROR: Unknown subcommand: %s\n' "$1" >&2
  usage >&2
  exit 64
  ;;
esac
