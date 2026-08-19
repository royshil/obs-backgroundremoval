// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

import { mkdir, readFile, writeFile } from "node:fs/promises";

import { GitHubClient } from "./github.mjs";

/**
 * @typedef {Object} AnalyzeDownloadCountsItem
 * @property {number} newDownloadCount
 * @property {number} cumulativeDownloadCount
 * @property {Date} publishedAt
 */

/**
 * @param {import("./github.mjs").ListReleasesItem[]} releases
 * @returns {AnalyzeDownloadCountsItem[]}
 */
export function analyzeDownloadCounts(releases) {
  /** @type {AnalyzeDownloadCountsItem[]} */
  const results = [];

  const publishedReleases = releases.filter(({ draft, published_at }) => !draft && published_at);
  publishedReleases.sort((a, b) => (a.published_at.localeCompare(b.published_at)));

  let cumulativeDownloadCount = 0;
  for (let i = 0; i < publishedReleases.length; i++) {
    const { assets, published_at } = publishedReleases[i];
    const newDownloadCount = assets.reduce((sum, { download_count }) => sum + download_count, 0);
    cumulativeDownloadCount += newDownloadCount;
    results.push({ newDownloadCount, cumulativeDownloadCount, publishedAt: new Date(published_at) })
  }

  return results;
}

export class DownloadTrendChart {
  /**
   * @typedef {Object} DownloadTrendChartOptions
   * @property {number} left
   * @property {number} top
   * @property {number} right
   * @property {number} bottom
   * @property {Date} xStart
   * @property {Date} xEnd
   * @property {number} yEnd
   * @property {number[]} yTickValues
   * @property {string[]} yTickLabels
   *
   * @param {Date} capturedAt
   * @param {AnalyzeDownloadCountsItem[]} downloadCounts
   * @param {DownloadTrendChartOptions} options
   */
  constructor(capturedAt, downloadCounts, options) {
    if (downloadCounts.length < 2) {
      throw new Error("downloadCounts has too few samples");
    }

    /** @type {Date} */
    this.capturedAt = capturedAt;

    /** @type {AnalyzeDownloadCountsItem[]} */
    this.downloadCounts = downloadCounts;

    /** @type {DownloadTrendChartOptions} */
    this.options = options;
  }

  /**
   * @returns {{x: number, y: number}[]}
   */
  calculateChartPoints() {
    const { left, top, right, bottom, xStart, xEnd, yEnd } = this.options;
    const chartPoints = this.downloadCounts.map(({ publishedAt, cumulativeDownloadCount }) => ({
      x: left + ((publishedAt - xStart) / (xEnd - xStart)) * (right - left),
      y: bottom - (cumulativeDownloadCount / yEnd) * (bottom - top)
    }));
    return chartPoints;
  }

  /**
   * @param {{x: number, y: number}[]} chartPoints
   * @returns {string}
   */
  generateCurvePath(chartPoints) {
    if (chartPoints.length <= 1) {
      throw new Error("chartPoints too few")
    }

    const pathComponents = [`M${chartPoints[0].x.toFixed(1)} ${chartPoints[0].y.toFixed(1)}`];
    for (let i = 1; i < chartPoints.length - 1; i++) {
      const controlPoint = chartPoints[i];
      const nextPoint = chartPoints[i + 1];
      const midPointX = (controlPoint.x + nextPoint.x) / 2;
      const midPointY = (controlPoint.y + nextPoint.y) / 2;

      pathComponents.push(`Q${controlPoint.x.toFixed(1)} ${controlPoint.y.toFixed(1)}`);
      pathComponents.push(`${midPointX.toFixed(1)} ${midPointY.toFixed(1)}`);
    }
    pathComponents.push(`Q${chartPoints.at(-1).x.toFixed(1)} ${chartPoints.at(-1).y.toFixed(1)}`);
    pathComponents.push(`${chartPoints.at(-1).x.toFixed(1)} ${chartPoints.at(-1).y.toFixed(1)}`);

    return pathComponents.join(' ');
  }

  /**
   * @param {{x: number, y: number}[]} chartPoints
   * @returns {string}
   */
  renderSvg(chartPoints) {
    const { left, top, right, bottom, xStart, xEnd, yEnd } = this.options;

    const path = this.generateCurvePath(chartPoints);

    const yTicks = this.options.yTickValues.map((value, index) => {
      const label = this.options.yTickLabels[index];
      const y = bottom - (value / yEnd) * (bottom - top);
      return { label, y };
    });

    const areaPath = path.replace(/^M/, "L");
    const gridPath = yTicks
      .map(({ y }) => `M${left} ${y}H${right}`)
      .join(" ");
    const yTickLabels = yTicks
      .map(
        ({ label, y }) =>
          `  <text class="tick-label" x="${left - 8}" y="${y}" text-anchor="end" dominant-baseline="middle">${label}</text>`,
      )
      .join("\n");

    const capturedAt = new Date(this.capturedAt).toISOString();
    const firstReleaseDate = xStart.toISOString().slice(0, 7);
    const lastReleaseDate = xEnd.toISOString().slice(0, 7);
    const downloadCountLabel = this.downloadCounts.at(-1).cumulativeDownloadCount.toLocaleString("en-US");
    const lastPointY = chartPoints.at(-1).y.toFixed(1);

    return `<!--
SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>

SPDX-License-Identifier: Apache-2.0
-->
<svg xmlns="http://www.w3.org/2000/svg" width="800" height="292" viewBox="0 0 800 292" role="img" aria-labelledby="title description">
  <title id="title">Download trends</title>
  <desc id="description">Cumulative GitHub release asset downloads through each release date, captured at ${capturedAt}.</desc>
  <defs>
    <clipPath id="chart-clip">
      <rect x="${left}" y="${top}" width="${right - left}" height="${bottom - top}"/>
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
      .axis-title { fill: #24292f; font-family: ui-rounded, "SF Pro Rounded", "Segoe UI", "Noto Sans", sans-serif; font-size: 22px; font-style: italic; font-weight: bold; }
      .callout { fill: #1f2328; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", "Noto Sans", Helvetica, Arial, sans-serif; font-size: 14px; font-weight: bold; }
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
  <rect class="background" width="800" height="292" rx="6"/>
  <g fill="none" vector-effect="non-scaling-stroke">
    <path class="grid" d="${gridPath}"/>
  </g>
  <g clip-path="url(#chart-clip)">
    <path fill="url(#area)" d="M${left} ${bottom} ${areaPath} L${right} ${bottom}Z"/>
    <path class="trend" fill="none" stroke-width="3" stroke-linejoin="round" stroke-linecap="round" vector-effect="non-scaling-stroke" d="${path}"/>
  </g>
  <path class="axis" fill="none" vector-effect="non-scaling-stroke" d="M${left} ${top}V${bottom}H${right}"/>
${yTickLabels}
  <text class="generated-at" x="${left + 14}" y="${top + 20}">Generated at ${capturedAt}</text>
  <text class="date" x="${left}" y="${bottom + 20}" text-anchor="end" transform="rotate(-60 ${left} ${bottom + 20})">${firstReleaseDate}</text>
  <text class="date" x="${right}" y="${bottom + 20}" text-anchor="end" transform="rotate(-60 ${right} ${bottom + 20})">${lastReleaseDate}</text>
  <text class="axis-title" x="${(left + right) / 2 - 3}" y="${bottom + 46}" text-anchor="middle">Download trends for royshil/obs-backgroundremoval</text>
  <path class="callout-line" d="M${right} ${lastPointY}L${right - 120} ${top + 120}"/>
  <circle class="callout-point" cx="${right}" cy="${lastPointY}" r="5"/>
  <rect class="callout-box" x="${right - 280}" y="${top + 120}" width="160" height="24"/>
  <text class="callout" x="${right - 200}" y="${top + 137}" text-anchor="middle">${downloadCountLabel} downloads</text>
</svg>
`;
  }
}
