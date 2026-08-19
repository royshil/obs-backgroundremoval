#!/usr/bin/env node
// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

import { mkdir, readFile, writeFile } from "node:fs/promises";

import { GitHubClient } from "./lib/github.mjs";

const CHART = {
  left: 90,
  right: 772,
  top: 48,
  bottom: 310,
  maxDownloads: 2_500_000,
  tickInterval: 500_000,
};

const REPOSITORY = "royshil/obs-backgroundremoval";
const BUILDDIR_URL = new URL("./build/", import.meta.url);
const DOWNLOAD_TRENDS_DATA_URL = new URL("./build/download-trends-data.json", import.meta.url);
const DOWNLOAD_TRENDS_CHART_URL = new URL("./download-trends.svg", import.meta.url);

/**
 * @typedef {{cumulativeDownloadCount: number, publishedAt: Date}} AnalyzeDownloadCountsItem
 */

/**
 * @param {import("./lib/github.mjs").ListReleasesItem[]} releases
 * @returns {AnalyzeDownloadCountsItem[]}
 */
function analyzeDownloadCounts(releases) {
  /** @type {AnalyzeDownloadCountsItem[]} */
  const results = [];

  const publishedReleases = releases.filter(({ draft, published_at }) => !draft && published_at);
  publishedReleases.sort((a, b) => (a.published_at.localeCompare(b.published_at)));

  let cumulativeDownloadCount = 0;
  for (let i = 0; i < publishedReleases.length; i++) {
    const { assets, published_at } = publishedReleases[i];
    cumulativeDownloadCount += assets.reduce((sum, { download_count }) => sum + download_count, 0);
    results.push({ cumulativeDownloadCount, publishedAt: new Date(published_at) })
  }

  return results;
}

/**
 * @param {*} data
 * @returns {{downloads: number, publishedAt: Date}[]}
 */
function downloadHistoryPoints(data) {
  if (
    typeof data !== "object" ||
    data === null ||
    typeof data.capturedAt !== "string" ||
    !Array.isArray(data.downloadCounts)
  ) {
    throw new TypeError("Download history data has an invalid structure");
  }

  let previousCumulativeDownloadCount = 0;
  let previousPublishedAt = Number.NEGATIVE_INFINITY;
  return data.downloadCounts.map(
    ({ cumulativeDownloadCount, publishedAt }) => {
      const parsedPublishedAt = new Date(publishedAt);

      if (
        !Number.isSafeInteger(cumulativeDownloadCount) ||
        cumulativeDownloadCount < previousCumulativeDownloadCount ||
        !Number.isFinite(parsedPublishedAt.getTime()) ||
        parsedPublishedAt.getTime() < previousPublishedAt
      ) {
        throw new TypeError("Download history data contains an invalid release");
      }

      previousCumulativeDownloadCount = cumulativeDownloadCount;
      previousPublishedAt = parsedPublishedAt.getTime();
      return {
        downloads: cumulativeDownloadCount,
        publishedAt: parsedPublishedAt,
      };
    },
  );
}

function distanceFromSegment(point, start, end) {
  const deltaX = end.x - start.x;
  const deltaY = end.y - start.y;
  const lengthSquared = deltaX ** 2 + deltaY ** 2;

  if (lengthSquared === 0) {
    return Math.hypot(point.x - start.x, point.y - start.y);
  }

  const projection = Math.max(
    0,
    Math.min(
      1,
      ((point.x - start.x) * deltaX + (point.y - start.y) * deltaY) /
      lengthSquared,
    ),
  );
  const projectedX = start.x + projection * deltaX;
  const projectedY = start.y + projection * deltaY;
  return Math.hypot(point.x - projectedX, point.y - projectedY);
}

function simplifyChartPoints(points, tolerance) {
  if (points.length <= 2) {
    return points;
  }

  const start = points[0];
  const end = points.at(-1);
  let farthestIndex = 0;
  let farthestDistance = 0;

  for (let i = 1; i < points.length - 1; i++) {
    const distance = distanceFromSegment(points[i], start, end);
    if (distance > farthestDistance) {
      farthestDistance = distance;
      farthestIndex = i;
    }
  }

  if (farthestDistance <= tolerance) {
    return [start, end];
  }

  const left = simplifyChartPoints(points.slice(0, farthestIndex + 1), tolerance);
  const right = simplifyChartPoints(points.slice(farthestIndex), tolerance);
  return [...left.slice(0, -1), ...right];
}

function chartY(downloads) {
  return (
    CHART.bottom -
    (downloads / CHART.maxDownloads) * (CHART.bottom - CHART.top)
  );
}

function chartPath(points) {
  if (points.length === 0) {
    throw new Error("No published GitHub releases were found");
  }

  const firstDate = points[0].publishedAt;
  const lastDate = points.at(-1).publishedAt;
  const dateSpan = Math.max(lastDate - firstDate, 1);

  const chartPoints = simplifyChartPoints(
    points.map(({ publishedAt, downloads }) => {
      const x =
        CHART.left +
        ((publishedAt - firstDate) / dateSpan) * (CHART.right - CHART.left);
      const y = chartY(downloads);
      return { x, y };
    }),
    6,
  );

  const firstPoint = chartPoints[0];
  if (chartPoints.length === 1) {
    return `M${firstPoint.x.toFixed(1)} ${firstPoint.y.toFixed(1)}`;
  }

  let path = `M${firstPoint.x.toFixed(1)} ${firstPoint.y.toFixed(1)}`;
  for (let i = 1; i < chartPoints.length - 1; i++) {
    const controlPoint = chartPoints[i];
    const nextPoint = chartPoints[i + 1];
    const midpoint = {
      x: (controlPoint.x + nextPoint.x) / 2,
      y: (controlPoint.y + nextPoint.y) / 2,
    };

    path +=
      ` Q${controlPoint.x.toFixed(1)} ${controlPoint.y.toFixed(1)}` +
      ` ${midpoint.x.toFixed(1)} ${midpoint.y.toFixed(1)}`;
  }

  const lastPoint = chartPoints.at(-1);
  return (
    path +
    ` Q${lastPoint.x.toFixed(1)} ${lastPoint.y.toFixed(1)}` +
    ` ${lastPoint.x.toFixed(1)} ${lastPoint.y.toFixed(1)}`
  );
}

function renderSvg(
  path,
  capturedAt,
  firstReleaseDate,
  lastReleaseDate,
  downloadCountLabel,
  lastPointY,
) {
  const areaPath = path.replace(/^M/, "L");
  const ticks = Array.from(
    { length: CHART.maxDownloads / CHART.tickInterval },
    (_, index) => {
      const downloads = (index + 1) * CHART.tickInterval;
      return { downloads, y: chartY(downloads).toFixed(1) };
    },
  );
  const gridPath = ticks
    .map(({ y }) => `M${CHART.left} ${y}H${CHART.right}`)
    .join(" ");
  const tickLabels = ticks
    .map(
      ({ downloads, y }) =>
        `  <text class="tick-label" x="82" y="${y}" text-anchor="end" dominant-baseline="middle">${(downloads / 1_000_000).toFixed(1)}M</text>`,
    )
    .join("\n");

  return `<!--s
SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>

SPDX-License-Identifier: Apache-2.0
-->
<svg xmlns="http://www.w3.org/2000/svg" width="800" height="440" viewBox="0 0 800 440" role="img" aria-labelledby="title description">
  <title id="title">Download trends</title>
  <desc id="description">Cumulative GitHub release asset downloads through each release date, captured at ${capturedAt}.</desc>
  <defs>
    <clipPath id="chart-clip">
      <rect x="90" y="0" width="710" height="310"/>
    </clipPath>
    <linearGradient id="area" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#0969da" stop-opacity="0.32"/>
      <stop offset="1" stop-color="#0969da" stop-opacity="0.03"/>
    </linearGradient>
    <style>
      .background { fill: #ffffff; }
      .grid { shape-rendering: crispEdges; stroke: #d0d7de; stroke-opacity: 0.55; }
      .axis { shape-rendering: crispEdges; stroke: #8c959f; }
      .date { fill: #57606a; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", "Noto Sans", Helvetica, Arial, sans-serif; font-size: 12px; }
      .tick-label { fill: #57606a; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", "Noto Sans", Helvetica, Arial, sans-serif; font-size: 11px; }
      .generated-at { fill: #57606a; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", "Noto Sans", Helvetica, Arial, sans-serif; font-size: 10px; }
      .axis-title { fill: #24292f; font-family: ui-rounded, "SF Pro Rounded", "Segoe UI", "Noto Sans", sans-serif; font-size: 22px; font-style: italic; font-weight: 650; letter-spacing: 0.02em; }
      .callout { fill: #1f2328; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", "Noto Sans", Helvetica, Arial, sans-serif; font-size: 14px; font-weight: 600; }
      .callout-box { fill: #ffffff; stroke: #cf222e; stroke-width: 2; }
      .callout-line { fill: none; stroke: #cf222e; stroke-width: 1.5; }
      .callout-point { fill: #ffffff; stroke: #cf222e; stroke-width: 2; }
      .trend { stroke: #0969da; }
      @media (prefers-color-scheme: dark) {
        .background { fill: #0d1117; }
        .grid { stroke: #30363d; stroke-opacity: 0.8; }
        .axis { stroke: #6e7681; }
        .date { fill: #8c959f; }
        .tick-label { fill: #8c959f; }
        .generated-at { fill: #8c959f; }
        .axis-title { fill: #c9d1d9; }
        .callout { fill: #f0f6fc; }
        .callout-box { fill: #0d1117; stroke: #ff7b72; }
        .callout-line { stroke: #ff7b72; }
        .callout-point { fill: #0d1117; stroke: #ff7b72; }
        .trend { stroke: #58a6ff; }
        #area stop:first-child { stop-color: #58a6ff; }
        #area stop:last-child { stop-color: #58a6ff; }
      }
    </style>
  </defs>
  <rect class="background" width="800" height="440" rx="6"/>
  <g fill="none" vector-effect="non-scaling-stroke">
    <path class="grid" d="${gridPath}"/>
  </g>
  <g clip-path="url(#chart-clip)">
    <path fill="url(#area)" d="M90 310 ${areaPath} L772 310Z"/>
    <path class="trend" fill="none" stroke-width="3" stroke-linejoin="round" stroke-linecap="round" vector-effect="non-scaling-stroke" d="${path}"/>
  </g>
  <path class="axis" fill="none" vector-effect="non-scaling-stroke" d="M90 47.5V310H772"/>
${tickLabels}
  <text class="tick-label" x="82" y="310" text-anchor="end" dominant-baseline="middle">0</text>
  <text class="generated-at" x="104" y="68">Generated at ${capturedAt}</text>
  <text class="date" x="90" y="330" text-anchor="end" transform="rotate(-60 90 330)">${firstReleaseDate}</text>
  <text class="date" x="772" y="330" text-anchor="end" transform="rotate(-60 772 330)">${lastReleaseDate}</text>
  <text class="axis-title" x="422" y="356" text-anchor="middle">Download trends for royshil/obs-backgroundremoval</text>
  <path class="callout-line" d="M772 ${lastPointY}L652 168"/>
  <circle class="callout-point" cx="772" cy="${lastPointY}" r="5"/>
  <rect class="callout-box" x="492" y="168" width="160" height="24"/>
  <text class="callout" x="572" y="185" text-anchor="middle">${downloadCountLabel} downloads</text>
</svg>
`;
}

if (process.argv.length <= 2) {
  console.log(`Usage: GENERATE_CHARTS.mjs [fetch|generate]`);
} else if (process.argv[2] == "fetch") {
  const client = new GitHubClient(REPOSITORY);
  const releases = await client.listReleases();
  const downloadCounts = analyzeDownloadCounts(releases);
  const capturedAt = new Date().toISOString();

  await mkdir(BUILDDIR_URL, { recursive: true });
  await writeFile(DOWNLOAD_TRENDS_DATA_URL, JSON.stringify({ capturedAt, downloadCounts }), "utf8");
} else if (process.argv[2] == "generate") {
  const data = JSON.parse(await readFile(DOWNLOAD_TRENDS_DATA_URL, "utf8"));
  const points = downloadHistoryPoints(data);
  points[points.length - 1].publishedAt = new Date(data.capturedAt);
  const path = chartPath(points);
  const capturedAt = new Date(data.capturedAt).toISOString();
  const firstReleaseDate = points[0].publishedAt.toISOString().slice(0, 10);
  const lastReleaseDate = points.at(-1).publishedAt.toISOString().slice(0, 10);
  const downloadCountLabel = points.at(-1).downloads.toLocaleString("en-US");
  const lastPointY = chartY(points.at(-1).downloads).toFixed(1);

  await writeFile(
    DOWNLOAD_TRENDS_CHART_URL,
    renderSvg(
      path,
      capturedAt,
      firstReleaseDate,
      lastReleaseDate,
      downloadCountLabel,
      lastPointY,
    ),
    "utf8",
  );
  console.log(
    `Wrote ${DOWNLOAD_TRENDS_CHART_URL.pathname} from ${points.length} releases (${points.at(-1).downloads} downloads)`,
  );
}
