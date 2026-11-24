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
 * @typedef {Object} GetStarHistoryItem
 * @property {number} week Start of the calendar week in Unix seconds
 * @property {number} total Stars received during the week
 * @property {number[]} days Stars received on each day of the week, starting on Sunday
 */

/**
 * Weeks are returned newest first, so concatenating pages produces one continuous series back to the repository's
 * creation week.
 *
 * @param {string} repository
 * @param {number} [page]
 * @param {string} [token]
 * @param {string} [endpoint]
 * @returns {Promise<GetStarHistoryItem[]>}
 */
export async function getStarHistory(
  repository,
  page = 1,
  token = undefined,
  endpoint = DEFAULT_GITHUB_API_ENDPOINT,
) {
  const url = new URL(`${endpoint}/repos/${repository}/stargazers/history`);
  url.searchParams.set("per_page", "30");
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
      `Get star history API returned ${response.status} ${response.statusText}`,
    );
  }

  const weeks = await response.json();
  if (!Array.isArray(weeks)) {
    throw new TypeError("Get star history API did not return an array");
  }

  return weeks;
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
   * @param {number} [page]
   * @returns {Promise<GetStarHistoryItem[]>}
   */
  getStarHistory(page = 1) {
    return getStarHistory(this.repository, page, this.token, this.endpoint);
  }
}
