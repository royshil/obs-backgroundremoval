// SPDX-FileCopyrightText: 2021-2026 Roy Shilkrot <roy.shil@gmail.com>
// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

import { readFile, writeFile, glob } from "node:fs/promises";
import posthtml from "posthtml";

console.time("[CSP] Total time");

const args = process.argv.slice(2);
if (args.length < 1) {
  console.error("Usage: node add-csp-hashes.mjs <dist-dir>");
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

function updateCSPDirective(cspString, directiveName, hashes) {
  if (hashes.length === 0) return cspString;

  const directiveRegex = new RegExp(`(${directiveName}[^;]*)`);
  const hashesString = hashes.map((h) => `'${h}'`).join(" ");

  if (!directiveRegex.test(cspString)) {
    throw new Error(
      `CSP directive "${directiveName}" not found in CSP string.`,
    );
  }

  return cspString.replace(directiveRegex, `$1 ${hashesString}`);
}

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

  tree.match(
    { tag: "meta", attrs: { "http-equiv": "Content-Security-Policy" } },
    (node) => {
      let csp = node.attrs.content || "";

      if (scriptHashes.length > 0) {
        csp = updateCSPDirective(csp, "script-src", scriptHashes);
      }

      if (styleHashes.length > 0) {
        csp = updateCSPDirective(csp, "style-src", styleHashes);
      }

      node.attrs.content = csp;
      return node;
    },
  );

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

console.log(`[CSP] Updated CSP headers in ${processedCount} HTML files.`);
console.timeEnd("[CSP] Total time");
