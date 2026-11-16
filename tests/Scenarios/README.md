<!--
SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>

SPDX-License-Identifier: Apache-2.0
-->

# Acceptance test scenarios for obs-backgroundremoval

- **Debian:** Place the plugin where OBS can load it, then run
  `+run-debian.sh run-all`.
- **Windows:** ST-2000 runs only on GitHub Actions, from
  `.github/ci_scripts/run_scenario_tests_windows.ps1` in the Windows test job.
  The job installs the latest OBS Studio with winget, the script installs the
  plugin ZIP produced by the build job into `C:\ProgramData` and uses the
  runner's own OBS profile, and the job always uploads the OBS logs. Do not run
  the script on your own computer, because it overwrites your OBS profile and
  installed plugin.
