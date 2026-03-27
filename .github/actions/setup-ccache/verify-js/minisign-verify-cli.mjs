#!/usr/bin/env node

// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file @kaito-tokyo/minisign-verify/verify-js/minisign-verify-cli.mjs
 * A command-line interface for Minisign verification for Node.js.
 * @version 0.1.3
 * @since 2026-03-29
 */

import { MinisignVerifier } from "./index.mjs";

function printHelp() {
  console.log("Usage: minisign-verify -V -P <pubkey> -m <message>");
}

if (process.argv.length !== 7) {
  printHelp();
  process.exit(64);
}

const pubkey = process.argv[4];
const messageFilepath = process.argv[6];

if (process.argv[2] !== "-V" || process.argv[3] !== "-P" || !pubkey || process.argv[5] !== "-m" || !messageFilepath) {
  printHelp();
  process.exit(64);
}

try {
  const verifier = await MinisignVerifier.create(pubkey);
  const verifyResult = await verifier.verifyFilepath(messageFilepath);
  if (verifyResult.ok) {
    console.log("Signature and comment signature verified");
    console.log(`Trusted comment: ${verifyResult.trustedComment}`);
  } else {
    console.error("Signature verification failed");
    process.exit(1);
  }
} catch (error) {
  if (error instanceof Error) {
    console.error(error.message);
  } else {
    console.error("An unknown error occurred");
  }
  process.exit(1);
}
