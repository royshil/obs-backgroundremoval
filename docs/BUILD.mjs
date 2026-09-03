#!/usr/bin/env node
// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

import { cp, mkdir, readFile, rm, writeFile } from "node:fs/promises";

import { analyzeDownloadCounts, DownloadTrendChart } from "./lib/download_trends.mjs";
import { GitHubClient } from "./lib/github.mjs";
import { aggregateStargazersByMonth, calculateStarAxis, StarHistoryChart } from "./lib/star_history.mjs";

const USAGE = "Usage: BUILD.mjs [fetch-charts|generate-charts|site]";

const REPOSITORY = "royshil/obs-backgroundremoval";
const BUILDDIR_URL = new URL("./build/", import.meta.url);
const RELEASES_DATA_URL = new URL("./build/releases.json", import.meta.url);
const DOWNLOAD_TRENDS_CHART_URL = new URL("./download-trends.svg", import.meta.url);
const STARGAZERS_DATA_URL = new URL("./build/stargazers.json", import.meta.url);
const STAR_HISTORY_CHART_URL = new URL("./star-history.svg", import.meta.url);
const DOCS_URL = new URL("./", import.meta.url);
const SITE_DIST_URL = new URL("./dist/", import.meta.url);
const SITE_FILES = [
  "404.html",
  "addAVideoCaptureSource.jpg",
  "addBackgroundRemoval.jpg",
  "arch.html",
  "debian.html",
  "download-debian-amd64.svg",
  "download-debian-arm64.svg",
  "download-mac-apple.svg",
  "download-mac-intel.svg",
  "download-ubuntu-amd64.svg",
  "download-ubuntu-arm64.svg",
  "download-windows-x64.svg",
  "flatpak.html",
  "demo-av1.mp4",
  "demo.mp4",
  "demo.jpg",
  "captureFilters.jpg",
  "filterSettings.jpg",
  "filterSettingsAdvanced.jpg",
  "favicon.ico",
  "github-mark-white.svg",
  "index.html",
  "global.css",
  "macos.html",
  "obs-backgroundremoval-icon.jpg",
  "opensuse.html",
  "ubuntu.html",
  "usage.html",
  "windows.html",
  "windows/INSTALL.html",
];
const SITE_REDIRECTS = [
  ["arch/index.html", "../arch.html"],
  ["debian/index.html", "../debian.html"],
  ["flatpak/index.html", "../flatpak.html"],
  ["macos/index.html", "../macos.html"],
  ["opensuse/index.html", "../opensuse.html"],
  ["ubuntu/index.html", "../ubuntu.html"],
  ["usage/index.html", "../usage.html"],
  ["versions/index.html", "../versions.html"],
  ["windows/index.html", "../windows.html"],
];

/**
 * @param {unknown} value
 */
function escapeHtml(value) {
  return String(value)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#39;");
}

function formatBinaryBytes(bytes) {
  if (!Number.isFinite(bytes) || bytes < 0) {
    throw new TypeError("Release asset size is invalid");
  }
  const units = ["bytes", "KiB", "MiB", "GiB"];
  let value = bytes;
  let unitIndex = 0;
  while (value >= 1024 && unitIndex < units.length - 1) {
    value /= 1024;
    unitIndex++;
  }
  const digits = unitIndex === 0 || value >= 10 ? 0 : 1;
  return `${value.toFixed(digits)} ${units[unitIndex]}`;
}

function validateReleaseTag(tagName) {
  if (typeof tagName !== "string" || !/^[A-Za-z0-9._-]+$/.test(tagName)) {
    throw new TypeError(`Release tag cannot be used as a file name: ${tagName}`);
  }
  return tagName;
}

function renderFooter(indent = "  ") {
  return `${indent}<footer>\n${indent}  Copyright © 2021–2026 Roy Shilkrot and © 2023–2026 Kaito Udagawa.<br>\n${indent}  Licensed under the GNU General Public License v3.0 or later.\n${indent}</footer>`;
}

function renderVersionHistory(releases, latestRelease) {
  const releaseItems = releases.map((release) => {
    const tagName = validateReleaseTag(release.tag_name);
    const publishedAt = escapeHtml(release.published_at);
    const latest = tagName === latestRelease.tag_name ? " <strong>(Latest)</strong>" : "";
    return `      <li><a href="versions/${escapeHtml(tagName)}.html">${escapeHtml(tagName)}</a> <time datetime="${publishedAt}">${escapeHtml(release.published_at.slice(0, 10))}</time>${latest}</li>`;
  }).join("\n");

  return `<!doctype html>
<!--
SPDX-FileCopyrightText: 2021-2026 Roy Shilkrot <roy.shil@gmail.com>
SPDX-FileCopyrightText: 2023-2026 Kaito Udagawa <umireon@kaito.tokyo>

SPDX-License-Identifier: GPL-3.0-or-later
-->
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="icon" href="favicon.ico">
  <link rel="stylesheet" href="global.css" type="text/css">
  <title>Version History - OBS Background Removal</title>
</head>

<body>
  <main class="white-box">
    <h1>Version History</h1>
    <h2>OBS Background Removal</h2>

    <ul class="version-list">
${releaseItems}
    </ul>

    <nav><a href="index.html">Back to Top</a></nav>
  </main>

${renderFooter("  ")}
</body>
</html>
`;
}

function renderReleasePage(release, latestRelease) {
  const tagName = validateReleaseTag(release.tag_name);
  const isLatest = tagName === latestRelease.tag_name;
  const latestBadge = isLatest ? '<span class="badge-latest">Latest</span>' : "";
  const prereleaseWarning = release.prerelease
    ? `\n    <aside class="alert-box" aria-label="Version stability warning"><p><strong>Pre-release:</strong> This version is unstable. Not recommended for use.</p></aside>\n`
    : "";
  const releaseBody = release.body || "No release notes provided.";
  const assets = Array.isArray(release.assets) ? release.assets : [];
  const assetItems = assets.map((asset) => {
    const digest = typeof asset.digest === "string"
      ? `\n        <dd><code>${escapeHtml(asset.digest)}</code></dd>`
      : "";
    return `        <dt><a href="${escapeHtml(asset.browser_download_url)}">${escapeHtml(asset.name)}</a></dt>\n        <dd>${escapeHtml(formatBinaryBytes(asset.size))}</dd>${digest}`;
  }).join("\n");
  const publishedAt = escapeHtml(release.published_at);

  return `<!doctype html>
<!--
SPDX-FileCopyrightText: 2021-2026 Roy Shilkrot <roy.shil@gmail.com>
SPDX-FileCopyrightText: 2023-2026 Kaito Udagawa <umireon@kaito.tokyo>

SPDX-License-Identifier: GPL-3.0-or-later
-->
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="icon" href="../favicon.ico">
  <link rel="stylesheet" href="../global.css" type="text/css">
  <title>OBS Background Removal ${escapeHtml(tagName)} - Release Notes</title>
</head>

<body>
  <main class="white-box">
    <article>
      <header class="version-header">
        <div class="version-title-row"><h1>Version ${escapeHtml(tagName)}</h1>${latestBadge}</div>
        <h2>OBS Background Removal</h2>
      </header>
${prereleaseWarning}
      <section class="material-button-section">
        <div class="github-button"><a href="${escapeHtml(release.html_url)}" target="_blank" rel="noopener noreferrer"><img src="../github-mark-white.svg" alt="" width="24" height="24">View on GitHub</a></div>
        <div class="video-tutorials-button"><a href="https://www.youtube.com/playlist?list=PLfd4SnaQQz_DVr_18OQozucYmiC56rRhy" target="_blank" rel="noopener noreferrer">▶ YouTube Tutorials</a></div>
      </section>

      <section class="release-notes">
        <h2>Release notes</h2>
        <pre>${escapeHtml(releaseBody)}</pre>
      </section>

      <section class="release-assets">
        <h2>Additional Information</h2>
        <dl>
          <dt>Publish date</dt>
          <dd><time datetime="${publishedAt}">${escapeHtml(release.published_at.slice(0, 10))}</time></dd>
${assetItems}
        </dl>
      </section>

      <nav class="page-nav" aria-label="Page navigation">
        <ul>
          <li><a href="../index.html">Back to Top</a></li>
          <li><a href="../versions.html">Back to Version List</a></li>
          <li><a href="https://github.com/royshil/obs-backgroundremoval" target="_blank" rel="noopener noreferrer">GitHub Repository</a></li>
        </ul>
      </nav>
    </article>
  </main>

${renderFooter("  ")}
</body>
</html>
`;
}

/**
 * @param {GitHubClient} client
 * @returns {Promise<object[]>}
 */
async function fetchReleases(client) {
  const releases = [];
  for (let page = 1; ; page++) {
    const response = await client.listReleases(page);
    releases.push(...response);
    if (response.length < 100) {
      break;
    }
  }
  return releases;
}

/**
 * @param {GitHubClient} client
 * @param {Date} generatedAt
 */
async function fetchStarHistory(client, generatedAt) {
  const stargazers = [];
  for (let page = 1; ; page++) {
    const response = await client.listStargazers(page);
    console.log(`Got ${response.length} stargazers at ${page}`)
    stargazers.push(...response);
    if (response.length < 100) {
      break;
    }
  }
  await writeFile(STARGAZERS_DATA_URL, JSON.stringify({ generatedAt, stargazers }), "utf8");
}

/**
 */
async function generateDownloadTrends() {
  const {
    generatedAt: dataGeneratedAt,
    releases,
  } = JSON.parse(await readFile(RELEASES_DATA_URL, "utf8"));

  if (typeof dataGeneratedAt !== "string" || !Array.isArray(releases)) {
    throw new Error("Invalid download trends data")
  }

  const capturedAt = new Date(dataGeneratedAt);
  if (!Number.isFinite(capturedAt.getTime())) {
    throw new TypeError("Download trends generatedAt is invalid");
  }

  const downloadCounts = analyzeDownloadCounts(releases);
  downloadCounts.at(-1).publishedAt = capturedAt;

  const chart = new DownloadTrendChart(capturedAt, downloadCounts, {
    left: 90,
    right: 772,
    top: 16,
    bottom: 226,
    xStart: downloadCounts[0].publishedAt,
    xEnd: downloadCounts.at(-1).publishedAt,
    yEnd: 2_500_000,
    yTickValues: [0, 500_000, 1_000_000, 1_500_000, 2_000_000, 2_500_000],
    yTickLabels: ["0", "0.5M", "1.0M", "1.5M", "2.0M", "2.5M"]
  });

  const originalChartPoints = chart.calculateChartPoints();
  const bucketCount = 30;
  const bucketWidth = (originalChartPoints.at(-1).x - originalChartPoints[0].x) / bucketCount;
  const chartPoints = [originalChartPoints[0]];
  let chartPointIndex = 0;

  for (let bucketIndex = 1; bucketIndex <= bucketCount; bucketIndex++) {
    const bucketEndX = originalChartPoints[0].x + bucketWidth * bucketIndex;
    const previousChartPointIndex = chartPointIndex;

    while (
      chartPointIndex + 1 < originalChartPoints.length &&
      originalChartPoints[chartPointIndex + 1].x <= bucketEndX
    ) {
      chartPointIndex++;
    }

    if (chartPointIndex > previousChartPointIndex) {
      chartPoints.push({
        x: bucketEndX,
        y: originalChartPoints[chartPointIndex].y,
      });
    }
  }

  const svg = chart.renderSvg(chartPoints);
  await writeFile(DOWNLOAD_TRENDS_CHART_URL, svg, "utf8");

  console.log(
    `Wrote ${DOWNLOAD_TRENDS_CHART_URL.pathname} from ${chartPoints.length} releases (${downloadCounts.at(-1).cumulativeDownloadCount} downloads)`,
  );
}

async function generateStarHistory() {
  const {
    generatedAt: dataGeneratedAt,
    stargazers,
  } = JSON.parse(await readFile(STARGAZERS_DATA_URL, "utf8"));
  if (typeof dataGeneratedAt !== "string" || !Array.isArray(stargazers)) {
    throw new TypeError("Invalid star history data");
  }

  const capturedAt = new Date(dataGeneratedAt);
  if (!Number.isFinite(capturedAt.getTime())) {
    throw new TypeError("Star history generatedAt is invalid");
  }

  const rows = aggregateStargazersByMonth(stargazers);
  if (rows.length < 2) {
    throw new Error("Star history data must contain at least two months");
  }

  const axis = calculateStarAxis(rows.at(-1).cumulativeStars);
  const chart = new StarHistoryChart(capturedAt, rows, {
    left: 90,
    right: 772,
    top: 16,
    bottom: 226,
    xStart: new Date(`${rows[0].month}-01T00:00:00Z`),
    xEnd: capturedAt,
    yEnd: axis.maximum,
    yTickValues: axis.values,
    yTickLabels: axis.labels,
  });
  const chartPoints = chart.calculateChartPoints();
  await writeFile(STAR_HISTORY_CHART_URL, chart.renderSvg(chartPoints), "utf8");
  console.log(`Wrote ${STAR_HISTORY_CHART_URL.pathname} from ${rows.length} months (${rows.at(-1).cumulativeStars} stars)`);
}

/**
 * @param {object[]} releases
 * @param {object} latestRelease
 */
async function generateSite(releases, latestRelease) {
  if (!Array.isArray(releases)) {
    throw new TypeError("Invalid releases");
  }
  if (
    typeof latestRelease.tag_name !== "string" ||
    typeof latestRelease.published_at !== "string" ||
    typeof latestRelease.html_url !== "string"
  ) {
    throw new TypeError("Invalid latest release");
  }
  const publishedReleases = releases.filter(
    (release) =>
      release &&
      typeof release === "object" &&
      !release.draft &&
      typeof release.published_at === "string" &&
      typeof release.tag_name === "string" &&
      typeof release.html_url === "string",
  );
  if (publishedReleases.length === 0) {
    throw new Error("No published releases found");
  }

  const metadataUrl = new URL("./metadata/", SITE_DIST_URL);
  await rm(SITE_DIST_URL, { recursive: true, force: true });
  await mkdir(SITE_DIST_URL, { recursive: true });
  await Promise.all(
    SITE_FILES.map(async (file) => {
      const destinationUrl = new URL(file, SITE_DIST_URL);
      await mkdir(new URL("./", destinationUrl), { recursive: true });
      await cp(new URL(file, DOCS_URL), destinationUrl);
    }),
  );
  await Promise.all(
    SITE_REDIRECTS.map(async ([destination, target]) => {
      const destinationUrl = new URL(destination, SITE_DIST_URL);
      await mkdir(new URL("./", destinationUrl), { recursive: true });
      await writeFile(
        destinationUrl,
        `<!doctype html>\n<html><head><meta http-equiv="refresh" content="0; url=${target}"></head></html>\n`,
        "utf8",
      );
    }),
  );
  const versionsUrl = new URL("./versions/", SITE_DIST_URL);
  await mkdir(versionsUrl, { recursive: true });
  await Promise.all([
    writeFile(
      new URL("./versions.html", SITE_DIST_URL),
      renderVersionHistory(publishedReleases, latestRelease),
      "utf8",
    ),
    ...publishedReleases.flatMap((release) => {
      const tagName = validateReleaseTag(release.tag_name);
      const redirectUrl = new URL(`./${tagName}/index.html`, versionsUrl);
      return [
        writeFile(
          new URL(`./${tagName}.html`, versionsUrl),
          renderReleasePage(release, latestRelease),
          "utf8",
        ),
        mkdir(new URL("./", redirectUrl), { recursive: true }).then(() =>
          writeFile(
            redirectUrl,
            `<!doctype html>\n<html><head><meta http-equiv="refresh" content="0; url=../${tagName}.html"></head></html>\n`,
            "utf8",
          ),
        ),
      ];
    }),
  ]);
  const indexUrl = new URL("./index.html", SITE_DIST_URL);
  const html = (await readFile(indexUrl, "utf8"))
    .replaceAll("{{LATEST_RELEASE_URL}}", escapeHtml(latestRelease.html_url))
    .replace(
      "<h2>Download Latest</h2>",
      `<h2>Download Latest ${escapeHtml(latestRelease.tag_name)} (${escapeHtml(latestRelease.published_at.slice(0, 10))})</h2>`,
    );
  await mkdir(metadataUrl, { recursive: true });
  await Promise.all([
    writeFile(indexUrl, html, "utf8"),
    writeFile(
      new URL("./latest-version.txt", metadataUrl),
      `${latestRelease.tag_name}`,
      "utf8",
    ),
  ]);
  console.log(`Wrote ${SITE_DIST_URL.pathname} for ${latestRelease.tag_name}`);
}

async function fetchCharts() {
  const { GITHUB_TOKEN } = process.env;
  if (!GITHUB_TOKEN) {
    throw new Error("GITHUB_TOKEN is required for fetch-charts.");
  }
  const client = new GitHubClient(REPOSITORY, GITHUB_TOKEN);
  const generatedAt = new Date();
  await mkdir(BUILDDIR_URL, { recursive: true });
  const [releases] = await Promise.all([
    fetchReleases(client),
    fetchStarHistory(client, generatedAt),
  ]);
  await writeFile(RELEASES_DATA_URL, JSON.stringify({ generatedAt, releases }), "utf8");
}

async function fetchSiteData() {
  const { GITHUB_TOKEN } = process.env;
  if (!GITHUB_TOKEN) {
    throw new Error("GITHUB_TOKEN is required for site.");
  }
  const client = new GitHubClient(REPOSITORY, GITHUB_TOKEN);
  const [releases, latestRelease] = await Promise.all([
    fetchReleases(client),
    client.getLatestRelease(),
  ]);
  console.log(`Got ${releases.length} releases and latest release ${latestRelease.tag_name}`);
  return { releases, latestRelease };
}

async function generateCharts() {
  await Promise.all([
    generateDownloadTrends(),
    generateStarHistory(),
  ]);
}

const args = process.argv.slice(2);

if (args[0] === "--help") {
  console.log(USAGE);
} else if (args[0] === "fetch-charts") {
  await fetchCharts();
} else if (args[0] === "generate-charts") {
  await generateCharts();
} else if (args[0] === "site") {
  const { releases, latestRelease } = await fetchSiteData();
  await generateSite(releases, latestRelease);
} else {
  console.error(USAGE);
  process.exit(64);
}
