---
# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

name: bump-figures-pr
description: Regenerate the download trends and star history figures (docs/download-trends.svg and docs/star-history.svg) from the GitHub API and open a pull request with them. Use when a maintainer asks to bump, refresh or update the figures or charts in the README.
---

# Bump the figures in a pull request

The README shows two figures that `docs/BUILD.mjs` generates from the GitHub API:

- `docs/download-trends.svg` from the release assets' `download_count`
- `docs/star-history.svg` from `GET /repos/{owner}/{repo}/stargazers/history`

Both endpoints are public, but pass a token anyway so that the requests do not run into the rate limit for
unauthenticated clients.

The pull request is posted under the maintainer's name, so this skill never pushes or opens it without the
maintainer's explicit approval in the conversation (see `AGENTS.md`).

## 1. Start from the latest main

Make sure the working tree is clean (`git status --porcelain` prints nothing). If it is not, stop and ask the
maintainer what to do with the changes. Then create a branch from the latest `main`:

```bash
git fetch origin main
git switch -c "$(gh api user --jq .login)/bump-figures-$(date -u +%Y%m%d)" origin/main
```

## 2. Regenerate the figures

```bash
GITHUB_TOKEN="$(gh auth token)" node docs/BUILD.mjs fetch-charts
node docs/BUILD.mjs generate-charts
```

The data goes to `docs/build/`, which is ignored by Git. Keep the two `Wrote …` lines of the output: they give the
number of downloads and stars for the summary below.

## 3. Check the changes

`git status --porcelain` must list only `docs/download-trends.svg` and `docs/star-history.svg`. If anything else
changed, or a command failed, stop and report it. If neither figure changed apart from the timestamps, tell the
maintainer that there is nothing to bump and stop.

## 4. Commit

Commit with the DCO sign-off and a signature:

```bash
git add docs/download-trends.svg docs/star-history.svg
git commit -s -S -m "Bump the figures"
```

If signing fails, stop and ask the maintainer to fix their signing setup. Never commit without `-S`.

## 5. Ask before posting

Show the maintainer the branch name, the commit, and the download and star totals from step 2. Then ask whether to
push the branch and open the pull request. Do nothing more until they say yes.

## 6. Open the pull request

After the maintainer says yes, push and open the pull request:

```bash
git push -u origin HEAD
gh pr create --base main --title "Bump the figures" --body-file <file>
```

Build the body from `.github/pull_request_template.md`:

- The first line belongs to the maintainer. Ask them for it, or leave it out if they do not give one.
- Then an `## Overview` section that says the figures were regenerated, with the totals from step 2.
- Then the `## Pull Request Checklist` from the template, copied exactly, with every box unchecked.

The maintainer checks the boxes.

Report the pull request URL to the maintainer.
