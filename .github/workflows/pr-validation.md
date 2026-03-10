---
# SPDX-FileCopyrightText: 2021-2026 Roy Shilkrot <roy.shil@gmail.com>
# SPDX-FileCopyrightText: 2023-2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: GPL-3.0-or-later

description: Validate if this Pull Request meets our project criteria (royshil/obs-backgroundremoval). COPILOT_GITHUB_TOKEN needs to be configured.

on:
  pull_request:
    types: [opened, synchronize, reopened]
    branches: [main]

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
        --paginate \
        --jq 'map({sha: .sha, message: .commit.message, verification: .commit.verification})'
    env:
      GH_TOKEN: ${{ secrets.GITHUB_TOKEN }}

safe-outputs:
  submit-pull-request-review: {}

engine:
  id: copilot
  model: gpt-5-mini
---

# Pull Request Validator

Validate if this Pull Request meets our project criteria (royshil/obs-backgroundremoval).

## Additional Inputs

<PullRequestText>
${{ needs.activation.outputs.text }}
</PullRequestText>

## Requirements

- **Commit Signing**
  - **Tooling**: Use the pull-request-commits safe input to fetch commit data of this Pull Request.
  - **Verification**: Inspect the `verification` object of every commit on this Pull Request, and verify if all commits on this Pull Request are properly signed.
  - **Context**: Refer to `<PROJECT_ROOT>/CONTRIBUTING.md` for this commit signing policy.

- **DCO (Developer’s Certificate of Origin)**
  - **Tooling**: Use the pull-request-commits safe input to fetch commit data of this Pull Request.
  - **Verification**: Inspect the `message` field of every commit on this Pull Request, and verify if all commits on this Pull Request have DCO.
  - **Context**: Refer to `<PROJECT_ROOT>/CONTRIBUTING.md` for this policy.

- **Pull Request Checklist**
  - **Verification**: Read the Pull Request text provided above, and verify if it contains the Pull Request template and all the items are checked.

## Outputs

- **Output Format**: Use Pull Request review.
- **Summary Line**: The first line of your comment MUST be a single-line summary of this validation, starting with either ✅ or 🚫.
- **Success**: If this Pull Request meets all criteria, submit an approval review, attaching this Pull Request's text including the checklist provided above as a code block.
- **Failure**: If this Pull Request fails to meet any criteria, submit a request-changes review that states what the problems are on this Pull Request.
