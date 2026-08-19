// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

const DEFAULT_GITHUB_API_ENDPOINT = "https://api.github.com"

/**
 * @typedef {Object} ListReleasesAsset
 * @property {string} url
 * @property {string} browser_download_url
 * @property {number} id
 * @property {string} node_id
 * @property {string} name
 * @property {string|null} label
 * @property {"uploaded"|"open"} state
 * @property {string} content_type
 * @property {number} size
 * @property {string|null} digest
 * @property {number} download_count
 * @property {string} created_at
 * @property {string} updated_at
 * @property {Object|null} uploader
 */

/**
 * @typedef {Object} ListReleasesItem
 * @property {string} url
 * @property {string} html_url
 * @property {string} assets_url
 * @property {string} upload_url
 * @property {string|null} tarball_url
 * @property {string|null} zipball_url
 * @property {string} [discussion_url]
 * @property {number} id
 * @property {string} node_id
 * @property {string} tag_name
 * @property {string} target_commitish
 * @property {string|null} name
 * @property {string|null} body
 * @property {boolean} draft
 * @property {boolean} prerelease
 * @property {boolean} immutable
 * @property {string} created_at
 * @property {string|null} published_at
 * @property {Object|null} author
 * @property {ListReleasesAsset[]} assets
 */

/**
 * @param {string} repository
 * @param {number} [page]
 * @param {string} [token]
 * @param {string} [endpoint]
 * @returns {Promise<ListReleasesItem[]>}
 */
export async function listReleases(repository, page = 1, token = undefined, endpoint = DEFAULT_GITHUB_API_ENDPOINT) {
  const url = new URL(`${endpoint}/repos/${repository}/releases`);
  url.searchParams.set("per_page", "100");
  url.searchParams.set("page", page);

  const headers = {
    "Accept": "application/vnd.github+json",
    "X-GitHub-Api-Version": "2026-03-10",
    "User-Agent": "obs-backgroundremoval-document-generator"
  };

  if (token) {
    headers.Authorization = `Bearer ${token}`;
  }

  const response = await fetch(url, { headers });
  if (!response.ok) {
    throw new Error(
      `List releases API returned ${response.status} ${response.statusText}`,
    );
  }

  const releases = await response.json();
  if (!Array.isArray(releases)) {
    throw new TypeError("List releases API did not return an array");
  }

  if (releases.length < 100) {
    return releases;
  } else {
    return [...releases, ...await listReleases(repository, page + 1, token, endpoint)];
  }
}

export class GitHubClient {
  /**
   * @param {string} repository
   * @param {string} [token]
   * @param {string} [endpoint]
   */
  constructor(repository, token = process.env.GITHUB_TOKEN, endpoint = DEFAULT_GITHUB_API_ENDPOINT) {
    this.repository = repository;
    this.endpoint = endpoint;
    this.token = token;
  }

  /**
   * @returns {Promise<ListReleasesItem[]>}
   */
  listReleases() {
    return listReleases(this.repository, 1, this.token, this.endpoint);
  }
}
