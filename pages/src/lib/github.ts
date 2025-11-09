export interface Author {
  login: string;
  id: number;
  node_id: string;
  avatar_url: string;
  gravatar_id: string;
  url: string;
  html_url: string;
  followers_url: string;
  following_url: string;
  gists_url: string;
  starred_url: string;
  subscriptions_url: string;
  organizations_url: string;
  repos_url: string;
  events_url: string;
  received_events_url: string;
  type: string;
  site_admin: boolean;
}

export interface Uploader {
  login: string;
  id: number;
  node_id: string;
  avatar_url: string;
  gravatar_id: string;
  url: string;
  html_url: string;
  followers_url: string;
  following_url: string;
  gists_url: string;
  starred_url: string;
  subscriptions_url: string;
  organizations_url: string;
  repos_url: string;
  events_url: string;
  received_events_url: string;
  type: string;
  site_admin: boolean;
}

export interface Asset {
  url: string;
  id: number;
  node_id: string;
  name: string;
  label: string | null;
  uploader: Uploader;
  content_type: string;
  state: string;
  size: number;
  download_count: number;
  created_at: string;
  updated_at: string;
  browser_download_url: string;
}

export interface Release {
  url: string;
  assets_url: string;
  upload_url: string;
  html_url: string;
  id: number;
  author: Author;
  node_id: string;
  tag_name: string;
  target_commitish: string;
  name: string | null;
  draft: boolean;
  prerelease: boolean;
  created_at: string;
  published_at: string | null;
  assets: Asset[];
  tarball_url: string | null;
  zipball_url: string | null;
  body: string | null;
}

export async function getLatestRelease(): Promise<Release> {
  const headers: Record<string, string> = {};
  if (
    typeof process !== "undefined" &&
    process.env &&
    process.env.GITHUB_TOKEN
  ) {
    headers["Authorization"] = `Bearer ${process.env.GITHUB_TOKEN}`;
  }

  const response = await fetch(
    "https://api.github.com/repos/royshil/obs-backgroundremoval/releases/latest",
    { headers },
  );

  if (!response.ok) {
    let errorBody: any = {};
    try {
      errorBody = await response.json();
    } catch (e) {
      // ignore JSON parse errors
    }
    throw new Error(
      `GitHub API request failed: ${response.status} ${response.statusText}` +
        (errorBody && errorBody.message ? ` - ${errorBody.message}` : ""),
    );
  }
  return (await response.json()) as Release;
}
