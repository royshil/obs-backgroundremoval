---
applyTo: .github/workflows/*.{yml,yaml}
---

# GitHub Actions Review Rules (2026)

<RunsOnRule>
- Enforce `runs-on: macos-15` (Standard for OBS plugins)
- Enforce `runs-on: ubuntu-24.04` (Standard for OBS plugins)
- Enforce `runs-on: windows-2022` (Standard for OBS plugins)
</RunsOnRule>

<UsesVersionRule>
- Enforce `uses: actions/cache/restore@v5` (Latest version)
- Enforce `uses: actions/cache/save@v5` (Latest version)
- Enforce `uses: actions/checkout@v6` (Latest version)
- Enforce `uses: actions/setup-python@v6` (Latest version)
- Enforce `uses: actions/upload-artifact@v6` (Latest version)
</UsesVersionRule>

<SetupPythonContext>
- Starting from v6, `actions/setup-python` supports the `pip-install` option. 
</SetupPythonContext>
