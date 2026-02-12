import { Octokit } from "@octokit/rest";

import { GITHUB_OWNER, GITHUB_REPO } from "../../lib/info.js";

export async function GET() {
  const octokit = new Octokit({ auth: import.meta.env.GITHUB_TOKEN });

  const latestRelease = await octokit.rest.repos.getLatestRelease({
    owner: GITHUB_OWNER,
    repo: GITHUB_REPO,
  });

  return new Response(latestRelease.data.tag_name, {
    status: 200,
    headers: {
      "Content-Type": "text/plain",
    },
  });
}
