interface GitHubLatestReleaseAsset {
	readonly browser_download_url: string;
	readonly name: string;
}

export interface GitHubLatestReleaseResult {
	readonly assets: [GitHubLatestReleaseAsset];
}

function getHeaders(): Record<string, string> {
	const { GITHUB_TOKEN } = process.env;
	if (GITHUB_TOKEN) {
		return {
			authorization: `Bearer ${GITHUB_TOKEN}`
		};
	} else {
		return {};
	}
}

export async function getLatestReleaseMetadata(
	owner: string = 'royshil',
	repo: string = 'obs-backgroundremoval'
): Promise<GitHubLatestReleaseResult> {
	const url = `https://api.github.com/repos/${owner}/${repo}/releases/latest`;
	const response = await fetch(url, { headers: getHeaders() });
	const json = await response.json();
	if (!response.ok) {
		console.error(json);
		throw new Error(`The latest release of ${owner}/${repo} cannot be retrieved!`);
	}
	return json;
}
