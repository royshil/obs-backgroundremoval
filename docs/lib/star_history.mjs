// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @typedef {Object} AggregateStargazersByMonthItem
 * @property {string} month
 * @property {number} newStars
 * @property {number} cumulativeStars
 */

/**
 * @param {import("./github.mjs").ListStargazersItem[]} stargazers
 * @returns {AggregateStargazersByMonthItem[]}
 */
export function aggregateStargazersByMonth(stargazers) {
  const stargazersByMonth = Object.groupBy(stargazers, ({ starred_at }) => {
    if (typeof starred_at !== "string") {
      throw new TypeError("Stargazer data does not contain starred_at");
    }

    const starredAt = new Date(starred_at);
    if (!Number.isFinite(starredAt.getTime())) {
      throw new TypeError(`Invalid starred_at value: ${starred_at}`);
    }

    return starredAt.toISOString().slice(0, 7);
  });

  const populatedMonths = Object.keys(stargazersByMonth).sort();
  if (populatedMonths.length === 0) {
    return [];
  }

  const firstMonth = new Date(`${populatedMonths[0]}-01T00:00:00Z`);
  const lastMonth = populatedMonths.at(-1);
  const rows = [];
  let cumulativeStars = 0;

  for (
    const month = firstMonth;
    month.toISOString().slice(0, 7) <= lastMonth;
    month.setUTCMonth(month.getUTCMonth() + 1)
  ) {
    const label = month.toISOString().slice(0, 7);
    const newStars = stargazersByMonth[label]?.length ?? 0;
    cumulativeStars += newStars;
    rows.push({ month: label, newStars, cumulativeStars });
  }

  return rows;
}

/**
 * @param {number} maximum
 * @param {number} [tickCount]
 * @returns {{maximum: number, values: number[], labels: string[]}}
 */
export function calculateStarAxis(maximum, tickCount = 5) {
  if (!Number.isSafeInteger(maximum) || maximum <= 0) {
    throw new RangeError("maximum must be a positive safe integer");
  }

  const roughStep = maximum / tickCount;
  const magnitude = 10 ** Math.floor(Math.log10(roughStep));
  const normalizedStep = roughStep / magnitude;
  const multiplier = normalizedStep <= 1 ? 1 : normalizedStep <= 2 ? 2 : normalizedStep <= 5 ? 5 : 10;
  const step = multiplier * magnitude;
  const axisMaximum = Math.ceil(maximum / step) * step;
  const values = Array.from({ length: axisMaximum / step + 1 }, (_, index) => index * step);

  return {
    maximum: axisMaximum,
    values,
    labels: values.map((value) => value.toLocaleString("en-US")),
  };
}

export class StarHistoryChart {
  /**
   * @typedef {Object} StarHistoryChartOptions
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
   * @param {AggregateStargazersByMonthItem[]} rows
   * @param {StarHistoryChartOptions} options
   */
  constructor(capturedAt, rows, options) {
    if (rows.length < 2) {
      throw new Error("Star history has too few samples");
    }

    this.capturedAt = capturedAt;
    this.rows = rows;
    this.options = options;
  }

  calculateChartPoints() {
    const { left, top, right, bottom, xStart, xEnd, yEnd } = this.options;
    return this.rows.map(({ month, cumulativeStars }, index) => {
      const capturedAt = index === this.rows.length - 1
        ? xEnd
        : new Date(`${month}-01T00:00:00Z`);
      return {
        x: left + ((capturedAt - xStart) / (xEnd - xStart)) * (right - left),
        y: bottom - (cumulativeStars / yEnd) * (bottom - top),
      };
    });
  }

  generateLinePath(chartPoints) {
    const pathComponents = chartPoints.map(({ x, y }, index) => {
      const command = index === 0 ? "M" : "L";
      return `${command}${x.toFixed(1)} ${y.toFixed(1)}`;
    });
    return pathComponents.join(" ");
  }

  renderSvg(chartPoints) {
    const { left, top, right, bottom, xStart, xEnd, yEnd } = this.options;
    const path = this.generateLinePath(chartPoints);
    const areaPath = path.replace(/^M/, "L");
    const ticks = this.options.yTickValues.map((value, index) => ({
      label: this.options.yTickLabels[index],
      y: bottom - (value / yEnd) * (bottom - top),
    }));
    const gridPath = ticks.map(({ y }) => `M${left} ${y}H${right}`).join(" ");
    const tickLabels = ticks.map(
      ({ label, y }) => `  <text class="tick-label" x="${left - 8}" y="${y}" text-anchor="end" dominant-baseline="middle">${label}</text>`,
    ).join("\n");
    const capturedAt = this.capturedAt.toISOString();
    const firstDate = xStart.toISOString().slice(0, 7);
    const lastDate = xEnd.toISOString().slice(0, 7);
    return `<!--
SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>

SPDX-License-Identifier: Apache-2.0
-->
<svg xmlns="http://www.w3.org/2000/svg" width="800" height="292" viewBox="0 0 800 292" role="img" aria-labelledby="title description">
  <title id="title">GitHub star history</title>
  <desc id="description">Cumulative GitHub stars by month, captured at ${capturedAt}.</desc>
  <defs>
    <clipPath id="chart-clip"><rect x="${left}" y="${top}" width="${right - left}" height="${bottom - top}"/></clipPath>
    <linearGradient id="area" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#e4c663" stop-opacity="0.32"/>
      <stop offset="1" stop-color="#e4c663" stop-opacity="0.03"/>
    </linearGradient>
    <style>
      .background { fill: #ffffff; }
      .grid { shape-rendering: crispEdges; stroke: #d0d7de; stroke-opacity: 0.55; }
      .axis { shape-rendering: crispEdges; stroke: #8c959f; }
      .date, .tick-label, .generated-at { fill: #57606a; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", "Noto Sans", Helvetica, Arial, sans-serif; }
      .date { font-size: 12px; } .tick-label { font-size: 11px; } .generated-at { font-size: 10px; }
      .axis-title { fill: #24292f; font-family: ui-rounded, "SF Pro Rounded", "Segoe UI", "Noto Sans", sans-serif; font-size: 22px; font-style: italic; font-weight: bold; }
      .trend { stroke: #e4c663; }
      @media (prefers-color-scheme: dark) {
        .background { fill: #0d1117; } .grid { stroke: #30363d; stroke-opacity: 0.8; } .axis { stroke: #6e7681; }
        .date, .tick-label, .generated-at { fill: #8c959f; } .axis-title { fill: #c9d1d9; }
        .trend { stroke: #dcb556; }
        #area stop:first-child, #area stop:last-child { stop-color: #dcb556; }
      }
    </style>
  </defs>
  <rect class="background" width="800" height="292" rx="6"/>
  <path class="grid" fill="none" vector-effect="non-scaling-stroke" d="${gridPath}"/>
  <g clip-path="url(#chart-clip)">
    <path fill="url(#area)" d="M${left} ${bottom} ${areaPath} L${right} ${bottom}Z"/>
    <path class="trend" fill="none" stroke-width="3" stroke-linejoin="round" stroke-linecap="round" vector-effect="non-scaling-stroke" d="${path}"/>
  </g>
  <path class="axis" fill="none" vector-effect="non-scaling-stroke" d="M${left} ${top}V${bottom}H${right}"/>
${tickLabels}
  <text class="generated-at" x="${left + 14}" y="${top + 20}">Generated at ${capturedAt}</text>
  <text class="date" x="${left}" y="${bottom + 20}" text-anchor="end" transform="rotate(-60 ${left} ${bottom + 20})">${firstDate}</text>
  <text class="date" x="${right}" y="${bottom + 20}" text-anchor="end" transform="rotate(-60 ${right} ${bottom + 20})">${lastDate}</text>
  <text class="axis-title" x="${(left + right) / 2 - 3}" y="${bottom + 46}" text-anchor="middle">Star history for royshil/obs-backgroundremoval</text>
</svg>
`;
  }
}
