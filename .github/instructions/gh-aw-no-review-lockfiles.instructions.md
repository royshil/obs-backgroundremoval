---
# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: GPL-3.0-or-later

applyTo: .github/workflows/*.lock.yml
---

# Do not review lockfiles of GitHub Agentic Workflows

- **No reviews for lockfiles**: You MUST NOT review any lockfiles of GitHub Agentic Workflows because they are automatically generated and should not be modified by any means. You MUST NOT generate any review comments for any changes in these lockfiles.
- **No runner image warning**: You MUST NOT generate any warning comment about runs-on in lockfiles.
