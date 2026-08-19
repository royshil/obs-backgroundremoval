#!/usr/bin/env node
// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

import { mkdir, readFile, writeFile } from "node:fs/promises";

import { analyzeDownloadCounts, DownloadTrendChart } from "./lib/download_trends.mjs";
import { GitHubClient } from "./lib/github.mjs";
import { aggregateStargazersByMonth, calculateStarAxis, StarHistoryChart } from "./lib/star_history.mjs";

const USAGE = "Usage: BUILD.mjs [fetch|generate]";

const REPOSITORY = "royshil/obs-backgroundremoval";
const BUILDDIR_URL = new URL("./build/", import.meta.url);
const RELEASES_DATA_URL = new URL("./build/releases.json", import.meta.url);
const DOWNLOAD_TRENDS_CHART_URL = new URL("./download-trends.svg", import.meta.url);
const STARGAZERS_DATA_URL = new URL("./build/stargazers.json", import.meta.url);
const STAR_HISTORY_CHART_URL = new URL("./star-history.svg", import.meta.url);

/**
 * @param {GitHubClient} client
 * @param {Date} generatedAt
 */
async function fetchDownloadTrends(client, generatedAt) {
  const releases = [];
  for (let page = 1; ; page++) {
    const response = await client.listReleases(page);
    console.log(`Got ${response.length} releases at ${page}`)
    releases.push(...response);
    if (response.length < 100) {
      break;
    }
  }
  await writeFile(RELEASES_DATA_URL, JSON.stringify({ generatedAt, releases }), "utf8");
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

const args = process.argv.slice(2);

if (args[0] === "--help") {
  console.log(USAGE);
} else if (args[0] === "fetch") {
  const { GITHUB_TOKEN } = process.env;
  if (!GITHUB_TOKEN) {
    throw new Error("GITHUB_TOKEN is required for fetch.");
  }
  const client = new GitHubClient(REPOSITORY, process.env.GITHUB_TOKEN);
  const generatedAt = new Date();
  await mkdir(BUILDDIR_URL, { recursive: true });
  await Promise.all([
    fetchDownloadTrends(client, generatedAt),
    fetchStarHistory(client, generatedAt),
  ]);
} else if (args[0] === "generate") {
  await Promise.all([
    generateDownloadTrends(),
    generateStarHistory(),
  ]);
} else {
  console.error(USAGE);
  process.exit(64);
}
