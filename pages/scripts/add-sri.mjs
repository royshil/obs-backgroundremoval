#!/usr/bin/env node

// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

/// <reference types="node" />

import { glob, readFile, writeFile } from "node:fs/promises";
import { relative, resolve, sep } from "node:path";
import posthtml from "posthtml";

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
 * @param {string} url
 * @return {boolean}
 */
function isExternalUrl(url) {
  return (
    url.startsWith("http://") ||
    url.startsWith("https://") ||
    url.startsWith("//")
  );
}

/**
 * @param {string} filePath
 * @returns {Promise<string>}
 */
async function sha384(filePath) {
  const content = await readFile(filePath);
  const hash = await crypto.subtle.digest("SHA-384", content);
  return `sha384-${Buffer.from(hash).toString("base64")}`;
}

async function createIntegrityMap(distDir, productionBaseUrl) {
  /** @type {Map<string, string>} */
  const integrityMap = new Map();

  for await (const filePath of glob(`${distDir}/**/*.{js,css}`)) {
    const relativePath = toPosixPath(relative(distDir, filePath));
    const url = new URL(relativePath, productionBaseUrl);
    integrityMap.set(url.pathname, await sha384(filePath));
  }

  return integrityMap;
}

/**
 * @param {string} distDir
 * @param {string} productionBaseUrl
 * @param {Map<string, string>} integrityMap
 * @return {import("posthtml").Plugin}
 */
function addSriPlugin(distDir, productionBaseUrl, integrityMap) {
  /**
   * @param {import("posthtml").Node} tree
   * @return {Promise<import("posthtml").Node>}
   */
  return async function (tree) {
    if (!tree.options || !tree.options.htmlPath) {
      throw new Error("addSriPlugin requires htmlPath option");
    }

    const htmlRelativePath = toPosixPath(
      relative(distDir, resolve(tree.options.htmlPath)),
    );
    const htmlBaseUrl = new URL(htmlRelativePath, productionBaseUrl);

    tree.match(
      [{ tag: "script" }, { tag: "link", attrs: { rel: "stylesheet" } }],
      (node) => {
        const attrs = node.attrs ?? {};
        const assetRef = attrs.src ?? attrs.href;

        if (!assetRef || isExternalUrl(assetRef)) {
          return node;
        }

        const url = new URL(assetRef, htmlBaseUrl);
        const integrity = integrityMap.get(url.pathname);

        if (integrity) {
          node.attrs = {
            ...attrs,
            integrity,
            crossorigin: "anonymous",
          };
        }
        return node;
      },
    );
    return tree;
  };
}

if (process.argv.length !== 4) {
  console.error("Usage: node add-sri.mjs <production-base-url> <dist-dir>");
  process.exit(1);
}

console.time("[SRI] Total time");

const PRODUCTION_BASE_URL = toNormalizedURL(process.argv[2]);
const distDir = process.argv[3];

console.log("[SRI] Hashing JS/CSS assets...");

const integrityMap = await createIntegrityMap(distDir, PRODUCTION_BASE_URL);

console.log(`[SRI] Processing HTML with ${integrityMap.size} hashed assets...`);

const processor = posthtml([
  addSriPlugin(distDir, PRODUCTION_BASE_URL, integrityMap),
]);

let processedCount = 0;

for await (const htmlPath of glob(`${distDir}/**/*.html`)) {
  const html = await readFile(htmlPath, "utf8");
  const result = await processor.process(html, { htmlPath });

  await writeFile(htmlPath, result.html);
  processedCount++;
}

console.log(`[SRI] Added standard integrity to ${processedCount} HTML files.`);
console.timeEnd("[SRI] Total time");
