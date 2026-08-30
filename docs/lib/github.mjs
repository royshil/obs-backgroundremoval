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

  return releases;
}

/**
 * @param {string} repository
 * @param {string} [token]
 * @param {string} [endpoint]
 * @returns {Promise<ListReleasesItem>}
 */
export async function getLatestRelease(repository, token = undefined, endpoint = DEFAULT_GITHUB_API_ENDPOINT) {
  const url = new URL(`${endpoint}/repos/${repository}/releases/latest`);
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
      `Get latest release API returned ${response.status} ${response.statusText}`,
    );
  }

  const release = await response.json();
  if (!release || typeof release !== "object" || Array.isArray(release)) {
    throw new TypeError("Get latest release API did not return an object");
  }

  return release;
}

/**
 * @typedef {Object} ListStargazersItem
 * @property {string} starred_at
 * @property {Object} user
 */

/**
 * @param {string} repository
 * @param {number} [page]
 * @param {string} [token]
 * @param {string} [endpoint]
 * @returns {Promise<ListStargazersItem[]>}
 */
export async function listStargazers(
  repository,
  page = 1,
  token = undefined,
  endpoint = DEFAULT_GITHUB_API_ENDPOINT,
) {
  const url = new URL(`${endpoint}/repos/${repository}/stargazers`);
  url.searchParams.set("per_page", "100");
  url.searchParams.set("page", page);

  const headers = {
    "Accept": "application/vnd.github.star+json",
    "X-GitHub-Api-Version": "2026-03-10",
    "User-Agent": "obs-backgroundremoval-document-generator"
  };

  if (token) {
    headers.Authorization = `Bearer ${token}`;
  }

  const response = await fetch(url, { headers });
  if (!response.ok) {
    throw new Error(
      `List stargazers API returned ${response.status} ${response.statusText}`,
    );
  }

  const stargazers = await response.json();
  if (!Array.isArray(stargazers)) {
    throw new TypeError("List stargazers API did not return an array");
  }

  return stargazers;
}

export class GitHubClient {
  /**
   * @param {string} repository
   * @param {string} [token]
   * @param {string} [endpoint]
   */
  constructor(repository, token = process.env.GITHUB_TOKEN, endpoint = DEFAULT_GITHUB_API_ENDPOINT) {
    /** @type {string} */
    this.repository = repository;

    /** @type {string} */
    this.endpoint = endpoint;

    /** @type {string|undefined} */
    this.token = token;
  }

  /**
   * @param {number} [page]
   * @returns {Promise<ListReleasesItem[]>}
   */
  listReleases(page = 1) {
    return listReleases(this.repository, page, this.token, this.endpoint);
  }

  /**
   * @returns {Promise<ListReleasesItem>}
   */
  getLatestRelease() {
    return getLatestRelease(this.repository, this.token, this.endpoint);
  }

  /**
   * @param {number} page
   * @returns {Promise<ListStargazersItem[]>}
   */
  listStargazers(page = 1) {
    return listStargazers(this.repository, page, this.token, this.endpoint);
  }
}
