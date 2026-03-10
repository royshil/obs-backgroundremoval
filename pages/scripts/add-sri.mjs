// SPDX-FileCopyrightText: 2021-2026 Roy Shilkrot <roy.shil@gmail.com>
// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

import { readFile, writeFile, glob } from "node:fs/promises";
import { relative, sep } from "node:path";
import { createHash } from "node:crypto";
import posthtml from "posthtml";

console.time("[SRI] Total time");

const args = process.argv.slice(2);
if (args.length < 1) {
  console.error("Usage: node add-sri.mjs <dist-dir>");
  process.exit(1);
}

const distDir = args[0];
const integrityMap = new Map();

console.log("[SRI] Hashing JS/CSS assets...");

for await (const filePath of glob(`${distDir}/**/*.{js,css}`)) {
  const content = await readFile(filePath);
  const hash = createHash("sha384").update(content).digest("base64");

  const relativePath = relative(distDir, filePath).split(sep).join("/");
  integrityMap.set(relativePath, `sha384-${hash}`);
}

console.log(`[SRI] Processing HTML with ${integrityMap.size} hashed assets...`);

const parser = posthtml([
  (tree) => {
    tree.match(
      [{ tag: "script" }, { tag: "link", attrs: { rel: "stylesheet" } }],
      (node) => {
        const attrs = node.attrs || {};
        const url = attrs.src || attrs.href;

        if (!url || url.startsWith("http") || url.startsWith("//")) return node;

        const key = url.startsWith("/") ? url.slice(1) : url;
        const integrity = integrityMap.get(key);

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
  },
]);

let processedCount = 0;

for await (const htmlPath of glob(`${distDir}/**/*.html`)) {
  const html = await readFile(htmlPath, "utf-8");
  const result = await parser.process(html);

  await writeFile(htmlPath, result.html);
  processedCount++;
}

console.log(`[SRI] Added standard integrity to ${processedCount} HTML files.`);
console.timeEnd("[SRI] Total time");
