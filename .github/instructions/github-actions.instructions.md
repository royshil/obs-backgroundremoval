---
# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

applyTo: "**/*.{yml,yaml}"
---

# GitHub Actions Workflow Guidelines

<PreinstalledSoftwaresOnRunnerImages>
- xcbeautify is preinstalled on macos-15.
- jq is preinstalled on ubuntu-slim, ubuntu-24.04, macos-15, and windows-2022.
</PreinstalledSoftwaresOnRunnerImages>

<ObsStudioOnUbuntu>
- **PPA**: We use the PPA provided by the OBS project (ppa:obsproject/obs-studio) to install OBS Studio and its development headers on Ubuntu.
- **Development Headers**: The package named obs-studio includes both the OBS Studio application and its development headers.
</ObsStudioOnUbuntu>

<CcacheEnvironmentVariables>
- **CCACHE_DEPEND**: Set to `true` to enable depend mode. Other values than `true` are not permitted.
- **CCACHE_DIRECT**: Set to `true` to enable direct mode. Other values than `true` are not permitted.
- **CCACHE_NODEPEND**: Set to `true` to disable depend mode. Other values than `true` are not permitted.
- **CCACHE_NODIRECT**: Set to `true` to disable direct mode. Other values than `true` are not permitted.
</CcacheEnvironmentVariables>

<ActionsSetupPythonInputs>
- **python-version-file**: File containing the Python version to use. Example: .python-version
- **pip-install**: Used to specify the packages to install with pip after setting up Python. Can be a requirements file or package names.
</ActionsSetupPythonInputs>
