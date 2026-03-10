#!/usr/bin/env node

// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

/// <reference types="node" />

import { readFile, writeFile, glob } from "node:fs/promises";
import posthtml from "posthtml";

console.time("[CSP] Total time");

const args = process.argv.slice(2);
if (args.length < 1) {
  console.error("Usage: node add-csp-hashes.mjs <dist-dir>");
  console.error("Run this script with Node.js 24.x or later");
  process.exit(1);
}

const distDir = args[0];

async function generateHash(content) {
  const encoder = new TextEncoder();
  const data = encoder.encode(content);

  const hashBuffer = await crypto.subtle.digest("SHA-256", data);

  const base64String = Buffer.from(hashBuffer).toString("base64");

  return `sha256-${base64String}`;
}

/**
 * @param {string} cspString
 * @returns {Generator<{name: string, values: string[]}, void, unknown>}
 */
function* parseCsp(cspString) {
  function* splitDirectives(cspString) {
    for (const rawToken of cspString.split(/;/)) {
      const token = rawToken.trim();
      if (token === "") continue;
      const m = token.match(/^([^ ]+)(.*)$/);
      if (m == null || m.length !== 3) continue;
      const name = m[1].toLowerCase();
      const values = m[2]
        .trim()
        .split(/ +/)
        .filter((v) => v !== "");
      yield { name, values };
    }
  }

  /** @type {Set<string>} */
  const processedDirectives = new Set();

  for (const { name, values } of splitDirectives(cspString)) {
    if (!processedDirectives.has(name)) {
      processedDirectives.add(name);
      yield { name, values };
    }
  }
}

/**
 * @param {string} cspString
 * @param {string} directiveName
 * @param {string[]} hashes
 * @return {string}
 */
function updateCSPDirective(cspString, directiveName, hashes) {
  const hashValues = hashes.map((h) => `'${h}'`);

  const csp = Array.from(parseCsp(cspString));
  const index = csp.findIndex(({ name }) => name === directiveName);
  if (index >= 0) {
    csp[index].values = Array.from(
      new Set([...csp[index].values, ...hashValues]),
    ).toSorted();
  } else {
    csp.push({ name: directiveName, values: hashValues.toSorted() });
  }

  return (
    csp.map(({ name, values }) => `${name} ${values.join(" ")}`).join("; ") +
    ";"
  );
}

/**
 * @param {import("posthtml").Node} tree
 * @returns {Promise<import("posthtml").Node>}
 */
const cspHashPlugin = async (tree) => {
  const scriptsToHash = [];
  const stylesToHash = [];

  tree.walk((node) => {
    if (node.tag === "script" && !node.attrs?.src && node.content) {
      const content = node.content.join("");
      if (content.trim()) {
        scriptsToHash.push(content);
      }
    }

    if (node.tag === "style" && node.content) {
      const content = node.content.join("");
      if (content.trim()) {
        stylesToHash.push(content);
      }
    }
    return node;
  });

  const scriptHashes = await Promise.all(scriptsToHash.map(generateHash));
  const styleHashes = await Promise.all(stylesToHash.map(generateHash));

  tree.match({ tag: "meta" }, (node) => {
    if (!node.attrs) {
      return node;
    }

    const httpEquiv = node.attrs["http-equiv"];
    if (!httpEquiv || httpEquiv.toLowerCase() !== "content-security-policy") {
      return node;
    }

    let csp = node.attrs.content ?? "";

    if (scriptHashes.length > 0) {
      csp = updateCSPDirective(csp, "script-src", scriptHashes);
    }

    if (styleHashes.length > 0) {
      csp = updateCSPDirective(csp, "style-src", styleHashes);
    }

    node.attrs.content = csp;
    return node;
  });

  return tree;
};

let processedCount = 0;
console.log(`[CSP] Scanning HTML files in ${distDir} using Web Crypto API...`);

for await (const htmlPath of glob(`${distDir}/**/*.html`)) {
  const html = await readFile(htmlPath, "utf-8");

  const result = await posthtml([cspHashPlugin]).process(html);

  await writeFile(htmlPath, result.html);
  processedCount++;
}

console.log(`[CSP] Updated CSP meta tags in ${processedCount} HTML files.`);
console.timeEnd("[CSP] Total time");
