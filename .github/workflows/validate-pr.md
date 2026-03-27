---
# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

strict: true

description: Validate if this Pull Request meets our project criteria. COPILOT_GITHUB_TOKEN needs to be configured.

metadata:
  author: Kaito Udagawa
  version: 1.1.1
  date: 2026-03-29

engine:
  id: copilot
  model: gpt-5-mini

on:
  label_command:
    name: validate
    events: [pull_request]

  status-comment: false

if: startsWith(github.ref, 'refs/pull/') && github.event.label.name == 'validate'

permissions:
  contents: read
  pull-requests: read

steps:
  - name: Fetch Pull Request commits
    shell: bash --noprofile --norc -euo pipefail -O nullglob {0}
    env:
      GH_TOKEN: ${{ github.token }}
    run: |
      ARGS=(
        "repos/$GITHUB_REPOSITORY/pulls/${GITHUB_REF_NAME%/merge}/commits"
        -H "Accept: application/vnd.github+json"
        -H "X-GitHub-Api-Version: 2026-03-10"
        --paginate
        --jq '[.[] | {sha: .sha, message: .commit.message, verification: .commit.verification}]'
      )
      gh api "${ARGS[@]}" > /tmp/gh-aw/agent/pr_commits.json

  - name: Extract items on Pull Request Checklist
    shell: bash --noprofile --norc -euo pipefail -O nullglob {0}
    env:
      GH_TOKEN: ${{ github.token }}
    run: |
      VALID_ITEMS=(
        'I have read the latest CONTRIBUTING.md.'
        'I have acknowledged the licensing and patent grant policy.'
        'I have included the required license headers.'
        'I am confident with my code integrity.'
        'I have signed off and verified all my commits.'
      )

      PR_NUMBER="${GITHUB_REF_NAME%/merge}"

      GH_PR_VIEW_ARGS=(
        --json body
        --jq '[.body | splits("\r?\n") | try match("^- \\[[ xX]\\] (.*)$") catch empty]'
      )

      gh pr view "$PR_NUMBER" "${GH_PR_VIEW_ARGS[@]}" > /tmp/raw_pr_checklist.json

      {
        echo '## Pull Request Checklist (Listing all the valid items, including not found errors)'
        echo
        for item in "${VALID_ITEMS[@]}"; do
          if ! jq -e -r --arg item "$item" '.[] | select(.captures[0].string == $item) | .string' /tmp/raw_pr_checklist.json; then
            echo "# ERROR: No lines found for '$item'"
          fi
        done
      } >/tmp/gh-aw/agent/pr_checklist.md

safe-outputs:
  add-comment:

  messages:
    append-only-comments: true

  jobs:
    accept_validate_pr:
      name: Accept validate-pr
      description: Decide if the result of this workflow is acceptable.

      inputs:
        commit_signing:
          description: A boolean value to represent if commit signing check is passed or not.
          required: true
          type: boolean
        dco:
          description: A boolean value to represent if DCO check is passed or not.
          required: true
          type: boolean
        pull_request_checklist:
          description: A boolean value to represent if Pull Request Checklist check is passed or not.
          required: true
          type: boolean

      runs-on: ubuntu-slim

      permissions: {}

      steps:
        - name: Accept or Reject
          shell: bash --noprofile --norc -euo pipefail -O nullglob {0}
          run: |
            if ! [[ -f "$GH_AW_AGENT_OUTPUT" ]]; then
              echo "No agent output found."
              exit 1
            fi

            COUNT="$(jq '[.items[] | select(.type == "accept_validate_pr")] | length' "$GH_AW_AGENT_OUTPUT")"
            if [[ "$COUNT" -ne 1 ]]; then
              echo "ERROR: $COUNT inputs received, only one was expected."
              exit 1
            fi

            COMMIT_SIGNING="$(jq -r '.items[] | select(.type == "accept_validate_pr") | .commit_signing' "$GH_AW_AGENT_OUTPUT")"
            DCO="$(jq -r '.items[] | select(.type == "accept_validate_pr") | .dco' "$GH_AW_AGENT_OUTPUT")"
            PULL_REQUEST_CHECKLIST="$(jq -r '.items[] | select(.type == "accept_validate_pr") | .pull_request_checklist' "$GH_AW_AGENT_OUTPUT")"

            rejected=0
            messages=()

            if [[ "$COMMIT_SIGNING" != "true" ]]; then
              messages+=("Commit Signing check was rejected.")
              rejected=1
            fi

            if [[ "$DCO" != "true" ]]; then
              messages+=("DCO check was rejected.")
              rejected=1
            fi

            if [[ "$PULL_REQUEST_CHECKLIST" != "true" ]]; then
              messages+=("Pull Request Checklist check was rejected.")
              rejected=1
            fi

            if [[ "$rejected" -eq 1 ]]; then
              messages+=("ERROR: $GITHUB_JOB rejected this check.")
            else
              messages+=("$GITHUB_JOB accepted the overall result.")
            fi

            for m in "${messages[@]}"; do
              printf '%s\n' "$m"
            done

            exit "$rejected"

  activation-comments: false
  mentions:
    allow-team-members: false
    allow-context: false
  report-failure-as-issue: false
  staged: false
---

# Validate Pull Request

Validate if this Pull Request meets our project criteria.

## Trigger

This workflow is triggered by label command on Pull Request.

## Checks

- **Commit Signing**
  - **Input**: Read `/tmp/gh-aw/agent/pr_commits.json` for summarized commit objects.
  - **Verification**: Inspect the `verification` object of every commit on this Pull Request, and verify if all commits on this Pull Request are properly signed.
  - **Context**: Refer to `CONTRIBUTING.md` for this commit signing policy.

- **DCO (Developer’s Certificate of Origin)**
  - **Input**: Read `/tmp/gh-aw/agent/pr_commits.json` for summarized commit objects.
  - **Verification**: Inspect the `message` field of every commit on this Pull Request, and verify if all commits on this Pull Request contain a valid `Signed-off-by:` trailer for DCO compliance.
  - **Context**: Refer to `CONTRIBUTING.md` for this policy.

- **Pull Request Checklist**
  - **Input**: Read `/tmp/gh-aw/agent/pr_checklist.md` for extracted and sanitized Pull Request checklist using whitelist.
  - **Verification:** Read the extracted Pull Request checklist, and verify if it contains the Pull Request template and all the items are checked.

## Outputs

You MUST add a single pull request comment and use the accept tool once as described below.

**Pull Request Comment**:

- **Condition**: Always add a summary comment, regardless of the check result.
- **Output Format**: You MUST add a summary comment that describes what you verify on the Pull Request.
- **Summary Line**: The first line of your comment MUST be a single-line summary of this validation, starting with either ✅ or 🚫.

**Accept tool**:

- **Condition**: Always call the `accept_validate_pr` tool, regardless of the check result.
- **Output Format**: You MUST send the check result to the `accept_validate_pr` tool to control merge admittance, providing the three boolean fields: `commit_signing`, `dco`, and `pull_request_checklist`.
