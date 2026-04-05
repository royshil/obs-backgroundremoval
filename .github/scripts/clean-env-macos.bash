# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

# file: .github/scripts/clean-env-macos.bash
# author: Kaito Udagawa <umireon@kaito.tokyo>
# version: 1.0.2
# date: 2026-04-02

filter_env_macos() {
  # Note: Stick to a Bash-3.2-compatible way for macOS support.
  local names=()
  while IFS= read -r line; do
    names+=("$line")
  done < <(compgen -e)

  local keepEnvVars=";${KEEP_ENV_VARS:-};"

  local name
  for name in "${names[@]}"; do
    case "$name" in
    # Pattern
    ACTIONS_* | CCACHE_* | GIT_* | GITHUB_* | RUNNER_*) ;;
    # Common
    CI | HOME | LANG | LC_ALL | LC_CTYPE | LOGNAME | PATH | PSModulePath | SHELL | TERM | TMPDIR | USER | XDG_CONFIG_HOME) ;;
    # macOS runner
    DEVELOPER_DIR | ImageOS | ImageVersion | XPC_FLAGS | XPC_SERVICE_NAME) ;;
    # Workflow-specific
    VCPKG_BINARY_SOURCES | VCPKG_ROOT | VCPKG_TARGET_TRIPLET) ;;
    *)
      if [[ "$keepEnvVars" != *";$name;"* ]]; then
        unset -v "$name" 2>/dev/null || true
      fi
      ;;
    esac
  done
}
filter_env_macos
unset -f filter_env_macos
