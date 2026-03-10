#!/usr/bin/env node

// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

// Run this script with Node 24.x

/// <reference types="node" />

import { glob, readFile, writeFile } from "node:fs/promises";
import { join, relative, resolve, sep } from "node:path";

/**
 * @param {string} name
 * @returns {string}
 */
function readEnv(name) {
  const value = process.env[name];
  if (value === undefined) {
    throw new Error(`${name} is not defined`);
  }
  return value;
}

/**
 * @param {string} urlString
 * @returns {URL}
 */
function toNormalizedURL(urlString) {
  const url = new URL(`${urlString}/`);
  url.pathname = url.pathname.replace(/\/+$/, "/");
  return url;
}

/**
 * @param {string} filePath
 * @returns {string}
 */
function toPosixPath(filePath) {
  return filePath.split(sep).join("/");
}

/**
 * @param {string} dir
 * @returns {AsyncGenerator<Dirent, void, unknown>}
 */
async function* lsFiles(dir) {
  for await (const entry of glob(join(dir, "**/*"), { withFileTypes: true })) {
    if (entry.isFile()) {
      yield entry;
    }
  }
}

/**
 * @param {Buffer} content
 * @returns {Promise<string>}
 */
async function calculateSha384Hex(content) {
  const arrayHash = await crypto.subtle.digest("SHA-384", content);
  const uint8Array = new Uint8Array(arrayHash);
  let hex = "";
  for (let i = 0; i < uint8Array.length; i++) {
    hex += uint8Array[i].toString(16).padStart(2, "0");
  }
  return hex;
}

/**
 * @param {any} value
 * @return {any}
 */
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

if (globalThis.crypto === undefined) {
  throw new Error("WebCrypto is required to run this script");
}

const GITHUB_EVENT_NAME = readEnv("GITHUB_EVENT_NAME");
const GITHUB_REF = readEnv("GITHUB_REF");
const GITHUB_REPOSITORY_ID = readEnv("GITHUB_REPOSITORY_ID");
const GITHUB_REPOSITORY_OWNER_ID = readEnv("GITHUB_REPOSITORY_OWNER_ID");
const GITHUB_REPOSITORY = readEnv("GITHUB_REPOSITORY");
const GITHUB_RUN_ATTEMPT = readEnv("GITHUB_RUN_ATTEMPT");
const GITHUB_RUN_ID = readEnv("GITHUB_RUN_ID");
const GITHUB_SERVER_URL = readEnv("GITHUB_SERVER_URL");
const GITHUB_SHA = readEnv("GITHUB_SHA");
const GITHUB_WORKFLOW_REF = readEnv("GITHUB_WORKFLOW_REF");
const RUNNER_ENVIRONMENT = readEnv("RUNNER_ENVIRONMENT");

if (process.argv.length !== 5) {
  console.error(
    "Usage: node generate-provenance.mjs <production-base-url> <dist-dir> <output-file>",
  );
  process.exit(1);
}

const PRODUCTION_BASE_URL = toNormalizedURL(process.argv[2]);
const distDir = process.argv[3];
const outputFile = process.argv[4];

const repoUri = `${GITHUB_SERVER_URL}/${GITHUB_REPOSITORY}`;
const workflowPath = GITHUB_WORKFLOW_REF.replace(
  `${GITHUB_REPOSITORY}/`,
  "",
).split("@")[0];

/** @type {Array<{ name: string, digest: { sha384: string }, size: number }>} */
const subjects = [];

for await (const { name, parentPath } of lsFiles(distDir)) {
  const filePath = resolve(parentPath, name);
  const content = await readFile(filePath);
  const hexHash = await calculateSha384Hex(content);
  const size = Buffer.byteLength(content);

  subjects.push({
    name: new URL(
      toPosixPath(relative(distDir, filePath)),
      PRODUCTION_BASE_URL,
    ).toString(),
    digest: { sha384: hexHash },
    size,
  });
}

subjects.sort((a, b) => a.name.localeCompare(b.name));

const rawManifest = {
  _type: "https://in-toto.io/Statement/v1",
  subject: subjects,
  predicateType: "https://slsa.dev/provenance/v1",
  predicate: {
    buildDefinition: {
      buildType: "https://actions.github.io/buildtypes/workflow/v1",
      externalParameters: {
        workflow: {
          ref: GITHUB_REF,
          repository: repoUri,
          path: workflowPath,
        },
      },
      internalParameters: {
        github: {
          event_name: GITHUB_EVENT_NAME,
          repository_id: GITHUB_REPOSITORY_ID,
          repository_owner_id: GITHUB_REPOSITORY_OWNER_ID,
          runner_environment: RUNNER_ENVIRONMENT,
        },
      },
      resolvedDependencies: [
        {
          uri: `git+${repoUri}@${GITHUB_REF}`,
          digest: { sha1: GITHUB_SHA },
        },
      ],
    },
    runDetails: {
      builder: {
        id: `${GITHUB_SERVER_URL}/${GITHUB_WORKFLOW_REF}`,
      },
      metadata: {
        invocationId: `${repoUri}/actions/runs/${GITHUB_RUN_ID}/attempts/${GITHUB_RUN_ATTEMPT}`,
      },
    },
  },
};

const manifest = deepSort(rawManifest);

await writeFile(outputFile, JSON.stringify(manifest, null, 2));
