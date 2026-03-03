---
# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: GPL-3.0-or-later

applyTo: .github/workflows/*.{yml,yaml}
---

# GitHub Actions Review Rules (2026)

<RunsOnRule>
- **Enforcement**: Use `runs-on: macos-15` (Standard for OBS plugins)
- **Enforcement**: Use `runs-on: ubuntu-24.04` (Standard for OBS plugins)
- **Enforcement**: Use `runs-on: windows-2022` (Standard for OBS plugins)
</RunsOnRule>

<UsesVersionRule>
- **Enforcement**: Use `uses: actions/cache/restore@v5` (Latest version)
- **Enforcement**: Use `uses: actions/cache/save@v5` (Latest version)
- **Enforcement**: Use `uses: actions/checkout@v6` (Latest version)
- **Enforcement**: Use `uses: actions/setup-python@v6` (Latest version)
- **Enforcement**: Use `uses: actions/upload-artifact@v6` (Latest version)
</UsesVersionRule>

<SetupPythonContext>
- **New Feature**: Starting from v6, `actions/setup-python` supports the `pip-install` option. This option is used to specify the packages to install with pip after setting up Python. Can be a requirements file or package names.
- **Preference**: Suggest using `pip-install: [packages]` for changesets.
</SetupPythonContext>
