// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file @kaito-tokyo/minisign-verify/verify-js/index.mjs
 * Easy-to-use wrapper around the Minisign verifier for Node.js and compatible environments.
 * @version 0.1.3
 * @since 2026-03-29
 * @license Apache-2.0
 */

import { arrayBuffer, text } from 'node:stream/consumers';
import { createHash } from "node:crypto";
import { createReadStream } from "node:fs";
import { pipeline } from "node:stream/promises";
import { readFile } from "node:fs/promises";
import { Readable } from "node:stream";
import { ReadableStream } from "node:stream/web";

import {
  readPubkey,
  parseSig,
  parseSigFile,
  getKeynumKey,
  verifyMinisign,
} from "./minisign.mjs";

import {
  arrayBufferToBase64url,
  base64ToUint8Array,
} from "./base64.mjs";

/**
 * MinisignVerifier is a helper class to verify Minisign signature on Node.js or its compatible environments.
 *
 * @example
 * ```js
 * import { MinisignVerifier } from "@kaito-tokyo/minisign-verify";
 *
 * const pubkeyStrings = ["PUBKEYBASE64STRING"];
 * const verifier = await MinisignVerifier.create(pubkeyStrings);
 *
 * const verifyResult = await verifier.verifyFilepath("test_fixtures/test.dat");
 * if (verifyResult.ok) {
 *   console.log("Verification succeeded!");
 * } else {
 *   throw new Error("Verification failed!");
 * }
 * ```
 */
export class MinisignVerifier {
  /**
   * Creates a MinisignVerifier instance with Minisign public key string(s).
   * @param {string | string[]} pubkeyStrings Minisign public key string(s).
   * @returns {Promise<MinisignVerifier>} A MinisignVerifier instance with the provided public key(s) loaded.
   */
  static async create(pubkeyStrings) {
    const verifier = new MinisignVerifier();

    if (!Array.isArray(pubkeyStrings)) {
      pubkeyStrings = [pubkeyStrings];
    }

    await Promise.all(pubkeyStrings.map(verifier.loadPubkeyString.bind(verifier)));

    return verifier;
  }

  constructor() {
    this.readPubkey = readPubkey.bind(null, arrayBufferToBase64url, base64ToUint8Array);
    this.parseSig = parseSig.bind(null, base64ToUint8Array);
    this.parseSigFile = parseSigFile.bind(null, parseSig, base64ToUint8Array);

    /** @type {Map<import("./minisign.mjs").KeynumKey, import("./minisign.mjs").Pubkey>} */
    this.pubkeys = new Map();
  }

  /**
   * @param {string} pubkeyString
   */
  async loadPubkeyString(pubkeyString) {
    const pubkey = await this.readPubkey(pubkeyString);
    this.pubkeys.set(getKeynumKey(pubkey), pubkey);
  }

  /**
   * Verifies a Minisign signature.
   * @param {Readable | import("stream/web").ReadableStream | Parameters<Readable.from>[0]} message
   * @param {string | Parameters<import("node:stream/consumers").text>[0]} sigFileContent
   * @param {object} [options]
   * @param {AbortSignal} [options.signal]
   * @return {Promise<import("./minisign.mjs").VerifyMinisignResult>}
   */
  async verify(message, sigFileContent, options = undefined) {
    const signal = options?.signal;

    if (!(message instanceof Readable)) {
      if (message instanceof ReadableStream) {
        message = Readable.fromWeb(message);
      } else {
        message = Readable.from(message);
      }
    }

    if (typeof sigFileContent !== "string") {
      sigFileContent = await text(sigFileContent);
    }

    const sigFile = this.parseSigFile(sigFileContent);

    return verifyMinisign(this.pubkeys, sigFile, async (blake2b512Required) => {
      if (blake2b512Required) {
        const hash = createHash("blake2b512");
        await pipeline(message, hash, { signal });
        return hash.digest();
      } else {
        return arrayBuffer(message);
      }
    });
  }

  /**
   * Convenience method to verify a file on the filesystem.
   * @param {string} filepath The file path to the data to verify.
   * @param {string} [signatureFilepath] The file path to the `.minisig` file. Defaults to `${filepath}.minisig`.
   */
  async verifyFilepath(filepath, signatureFilepath = undefined) {
    const sigFileContent = await readFile(signatureFilepath ?? `${filepath}.minisig`, "utf-8");
    return this.verify(createReadStream(filepath), sigFileContent);
  }
}
