---
# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

description: Dependencies vcpkg

on:
  schedule: daily
  push:
    branches: [main]
  workflow_dispatch:

permissions:
  contents: read

safe-outputs:
  create-pull-request:
    title-prefix: "[dependencies vcpkg] "
    labels: [dependencies, vcpkg]
    draft: true
    fallback-as-issue: false
    allowed-files:
      - "vcpkg-triplets/*.cmake"
      - "vcpkg-configuration.json"

  push-to-pull-request-branch:
    target: "*"
    title-prefix: "[dependencies vcpkg] "
    labels: [dependencies, vcpkg]
    allowed-files:
      - "vcpkg-triplets/*.cmake"
      - "vcpkg-configuration.json"

checkout:
  fetch: ["*"]
  fetch-depth: 0

engine:
  id: copilot
  model: gpt-5-mini
---

# Vcpkg Update

<Context>
`kaito-tokyo/vcpkg-obs-kaito-tokyo` is the upstream repository for vcpkg binary packages.
</Context>

<Objective>
Keep the vcpkg configuration files in sync between this repository and `kaito-tokyo/vcpkg-obs-kaito-tokyo`.
</Objective>

<TargetVcpkgConfigurationFiles>
- `vcpkg-triplets/*.cmake`
- `vcpkg-configuration.json`
</TargetVcpkgConfigurationFiles>

<Goal>
Help maintainers of this repository to keep the vcpkg configuration files up to date by creating or updating a Pull Request when you find changes in the upstream repository.
</Goal>

<PullRequestTemplate>
{{#runtime-import .github/pull_request_template.md}}
</PullRequestTemplate>

<Constraints>
- You MUST keep only one open Pull Request at a time for this workflow.
- Pull Requests created by this workflow use the title prefix `[dependencies vcpkg] ` and are labeled with `dependencies` and `vcpkg`.
- If a matching open Pull Request already exists, you MUST push changes to that Pull Request's branch. Otherwise, you MUST create a new Pull Request.
- You MUST include the Pull Request template content in the Pull Request description.
- You MUST NOT change any fields other than `default-registry.baseline` or `registries[].baseline` in `vcpkg-configuration.json`.
</Constraints>
