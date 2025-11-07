# Development Guideline for OBS Background Removal

- Develop this project using C17 and C++17.
- After modifying C or C++ files, format them with `clang-format-19`.
- After modifying CMake files, format them with `gersemi`.
- The default branch is `main`.

## Release Automation

To start a new release, the user will instruct Copilot (e.g., "Start release"). Copilot must follow these steps:

### 1. Specify New Version

- **Show the current version** (from `buildspec.json`).
- **Ask the user** for the new version number (e.g., `1.0.0`, `1.0.0-beta1`).
- **Validate**: The version must follow Semantic Versioning (`MAJOR.MINOR.PATCH`).

### 2. Prepare & Update `buildspec.json`

- **Check**: Confirm you are on the `main` branch and it is up-to-date with the remote.
- **Create a new branch**: Name it `releases/bump-X.Y.Z` (replace with the new version).
- **Update the version** in `buildspec.json` using `jq`:
  ```
  jq '.version = "<new_version>"' buildspec.json > buildspec.json.tmp && mv buildspec.json.tmp buildspec.json
  ```
  *Do not manually edit the file; always use `jq`.*

### 3. Create & Merge Pull Request (PR)

- **Create a PR** for the version update branch.
- **Provide the PR URL** to the user.
- **Instruct the user** to merge the PR.
- **Wait** for user confirmation that the PR is merged.

### 4. Push Git Tag

- **Trigger**: Only proceed when the user confirms the PR is merged.
- **Switch to `main`** branch.
- **Sync with remote**.
- **Verify** the `buildspec.json` version matches the intended release.
- **Push the Git tag**: Tag must be `X.Y.Z` (no `v` prefix).
- *Pushing the tag triggers the automated release workflow.*

### 5. Finalize Release

- **Provide the GitHub releases URL**.
- **Instruct the user** to complete the release process on GitHub.

---
