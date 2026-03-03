---
# SPDX-FileCopyrightText: 2021-2026 Roy Shilkrot <roy.shil@gmail.com>
# SPDX-FileCopyrightText: 2023-2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: GPL-3.0-or-later

on:
  pull_request:
    types: [opened, synchronize]
    branches: [main]

description: "Validate if this Pull Request meets our project criteria (royshil/obs-backgroundremoval). COPILOT_GITHUB_TOKEN needs to be configured."

permissions:
  contents: read
  pull-requests: read

safe-inputs:
  pull-request-commits:
    description: Returns the JSON from the GitHub API to list commits on a specified pull request
    inputs:
      prnumber:
        type: string
        required: true
        description: The number of Pull Request
    run: |
      gh api \
        "/repos/$GITHUB_REPOSITORY/pulls/$INPUT_PRNUMBER/commits" \
        -H "Accept: application/vnd.github+json" \
        -H "X-GitHub-Api-Version: 2022-11-28" \
        --paginate
    env:
      GH_TOKEN: ${{ secrets.GITHUB_TOKEN }}

safe-outputs:
  add-comment:
    hide-older-comments: true
    discussions: false

engine:
  id: copilot
  model: gpt-5.1-mini
---

# Pull Request Validator

Validate if this Pull Request meets our project criteria (royshil/obs-backgroundremoval)

## Requirements

- **Commit Signing**
  - **Tooling**: Use the pull-request-commits safe input to fetch commit data of this Pull Request.
  - **Verification**: Inspect the `verification` object of every commit on this Pull Request, and verify if all commits on this Pull Request are properly signed.
  - **Context**: Refer to `<PROJECT_ROOT>/CONTRIBUTING.md` for this commit signing policy.

- **DCO (Developer’s Certificate of Origin)**
  - **Tooling**: Use the pull-request-commits safe input to fetch commit data of this Pull Request.
  - **Verification**: Inspect the `message` field of every commit on this Pull Request, and verify if all commits on this Pull Request have DCO.
  - **Context**: Refer to `<PROJECT_ROOT>/CONTRIBUTING.md` for this policy.

- **License Header**
  - **Tooling**: Use `git` command.
  - **Condition**: Run this check when the changeset has any files with a C/C++ source or header extensions (.c, .h, .cpp, and .hpp).
  - **Verification**: All C/C++ source and header files newly added by this Pull Request MUST have a license header.
  - **Context**: Refer to `<PROJECT_ROOT>/CONTRIBUTING.md` and `<PROJECT_ROOT>/.github/instructions/license-header-c-cpp.instructions.md` for this policy.

<!-- end list -->

## Outputs

- **Output Format**: Add a comment as the output of this validation to this Pull Request using safe-output.
- **Summary Line**: The first line of your comment MUST be a single-line summary of this validation, starting with either ✅ or 🚫.
- **Success**: If this Pull Request meets all criteria, add a comment stating that this Pull Request is ready to merge.
- **Failure**: If this Pull Request fails to meet any criteria, add a comment that explains what the problems are on this Pull Request, and provides instructions on how Author can fix them.
