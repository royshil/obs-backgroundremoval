// SPDX-FileCopyrightText: 2021-2026 Roy Shilkrot <roy.shil@gmail.com>
// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

import fs from "node:fs";
import path from "node:path";
import crypto from "node:crypto";
import { execSync } from "node:child_process";
import process from "node:process";

const args = process.argv.slice(2);
if (args.length < 2) {
  console.error("Usage: node generate-provenance.mjs <dist-dir> <output-file>");
  process.exit(1);
}

const [distDir, outputFile] = args;
const PRODUCTION_BASE_URL =
  "https://royshil.github.io/obs-backgroundremoval";

function deepSort(value) {
  if (value === null || typeof value !== "object") {
    return value;
  }

  if (Array.isArray(value)) {
    return value.map(deepSort);
  }

  return Object.keys(value)
    .sort()
    .reduce((sorted, key) => {
      sorted[key] = deepSort(value[key]);
      return sorted;
    }, {});
}

try {
  const commitSha =
    process.env.GITHUB_SHA || execSync("git rev-parse HEAD").toString().trim();
  const repoUri = process.env.GITHUB_REPOSITORY
    ? `https://github.com/${process.env.GITHUB_REPOSITORY}`
    : "local";
  const workflowRef = process.env.GITHUB_WORKFLOW_REF || "local-manual-build";
  const runId = process.env.GITHUB_RUN_ID || "manual";

  const now = new Date();

  const subjects = fs
    .readdirSync(distDir, { recursive: true, withFileTypes: true })
    .filter((dirent) => dirent.isFile())
    .map((dirent) => {
      const fullPath = path.join(dirent.parentPath, dirent.name);
      const buffer = fs.readFileSync(fullPath);
      const hash = crypto.createHash("sha384").update(buffer).digest("hex");
      const size = Buffer.byteLength(buffer);

      const relativePath = path
        .relative(distDir, fullPath)
        .split(path.sep)
        .join("/");
      return {
        name: `${PRODUCTION_BASE_URL}/${relativePath}`,
        digest: { sha384: hash },
        size: size,
      };
    })
    .sort((a, b) => a.name.localeCompare(b.name));

  const manifestRaw = {
    _type: "https://in-toto.io/Statement/v1",
    subject: subjects,
    predicateType: "https://slsa.dev/provenance/v1",
    predicate: {
      buildDefinition: {
        buildType: "https://actions.github.io/buildtypes/workflow/v1",
        externalParameters: {
          source: {
            uri: repoUri,
            digest: { sha1: commitSha },
          },
          workflow: {
            ref: workflowRef,
            runId: runId,
          },
        },
        startedOn: now.toISOString(),
      },
      runDetails: {
        builder: {
          id: "https://github.com/actions/runner/github-hosted",
        },
      },
    },
  };

  const sortedManifest = deepSort(manifestRaw);
  fs.writeFileSync(outputFile, JSON.stringify(sortedManifest, null, 2));
} catch (error) {
  console.error(`Generation Failed: ${error.message}`);
  process.exit(1);
}
