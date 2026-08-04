// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

import { GITHUB_URL } from "./info.js";

export interface ReleaseDefinition {
  version: string;
  publishedAt: string;
  releaseUrl: string;
  downloads: readonly ReleaseDownload[];
}

export interface ReleaseDownload {
  label: string;
  file: string;
}

export const RELEASES = [
  {
    version: "1.4.0",
    publishedAt: "2026-04-07T13:10:43Z",
    releaseUrl: `${GITHUB_URL}/releases/tag/1.4.0`,
    downloads: [
      {
        label: "Windows x64",
        file: "obs-backgroundremoval-1.4.0-windows-x64.zip",
      },
      {
        label: "macOS Universal",
        file: "obs-backgroundremoval-1.4.0-macos-universal.pkg",
      },
      {
        label: "Ubuntu x86_64",
        file: "obs-backgroundremoval-1.4.0-x86_64-linux-gnu.deb",
      },
    ],
  },
  {
    version: "1.4.1",
    publishedAt: "2026-07-26T07:35:21Z",
    releaseUrl: `${GITHUB_URL}/releases/tag/1.4.1`,
    downloads: [
      {
        label: "Windows x64",
        file: "obs-backgroundremoval-1.4.1-windows-x64.zip",
      },
      {
        label: "macOS Universal",
        file: "obs-backgroundremoval-1.4.1-macos-universal.pkg",
      },
      {
        label: "Debian Forky amd64",
        file: "obs-backgroundremoval_1.4.1.forky_amd64.deb",
      },
      {
        label: "Debian Forky arm64",
        file: "obs-backgroundremoval_1.4.1.forky_arm64.deb",
      },
    ],
  },
] as const satisfies readonly ReleaseDefinition[];

export const LATEST_RELEASE = RELEASES.at(-1)!;

export function getReleaseDefinition(
  version: string,
): ReleaseDefinition | undefined {
  return RELEASES.find((release) => release.version === version);
}

export function getReleaseDownload(
  release: ReleaseDefinition,
  label: string,
): ReleaseDownload {
  const download = release.downloads.find(
    (candidate) => candidate.label === label,
  );
  if (!download) {
    throw new Error(
      `Release ${release.version} has no download labeled ${label}`,
    );
  }
  return download;
}

export function requiresReleaseDefinition(version: string): boolean {
  const match = /^v?(\d+)\.(\d+)\.(\d+)/.exec(version);
  if (!match) return false;

  const [major, minor, patch] = match.slice(1).map(Number);
  return (
    major > 1 ||
    (major === 1 && minor > 4) ||
    (major === 1 && minor === 4 && patch >= 0)
  );
}

export function getReleaseAssetUrl(version: string, assetName: string): string {
  return `${GITHUB_URL}/releases/download/${encodeURIComponent(version)}/${encodeURIComponent(assetName)}`;
}
