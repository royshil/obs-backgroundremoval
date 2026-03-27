// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file @kaito-tokyo/minisign-verify/verify-js/base64.mjs
 * Base64 utilities for Modern JavaScript environments.
 * @version 0.1.3
 * @since 2026-03-29
 */

/**
 * Encodes an ArrayBuffer to a Base64url-string.
 * @template {ArrayBufferLike} TArrayBuffer
 * @param {TArrayBuffer | ArrayBufferView<TArrayBuffer>} arrayBuffer
 * @return {string}
 */
export function arrayBufferToBase64url(arrayBuffer) {
  const uint8Array = arrayBufferToUint8Array(arrayBuffer);

  if ('toBase64' in uint8Array && typeof uint8Array.toBase64 === "function") {
    return uint8Array.toBase64({ alphabet: "base64url", omitPadding: true });
  } else if (typeof Buffer !== "undefined") {
    return Buffer.from(uint8Array).toString("base64url");
  } else {
    throw new Error("Base64EncodingNotSupportedError");
  }
}

/**
 * Returns a Uint8Array from ArrayBuffer efficiently.
 * @template {ArrayBufferLike} TArrayBuffer
 * @param {TArrayBuffer | ArrayBufferView<TArrayBuffer>} arrayBuffer
 * @returns {Uint8Array<TArrayBuffer>}
 */
export function arrayBufferToUint8Array(arrayBuffer) {
  if (ArrayBuffer.isView(arrayBuffer)) {
    return new Uint8Array(arrayBuffer.buffer, arrayBuffer.byteOffset, arrayBuffer.byteLength);
  } else {
    return new Uint8Array(arrayBuffer);
  }
}

/**
 * Decodes a Base64-string to a Uint8Array.
 * @param {string} base64String
 * @return {Uint8Array<ArrayBuffer>}
 */
export function base64ToUint8Array(base64String) {
  if ('fromBase64' in Uint8Array && typeof Uint8Array.fromBase64 === "function") {
    return Uint8Array.fromBase64(base64String);
  } else if (typeof Buffer !== "undefined") {
    return Buffer.from(base64String, "base64");
  } else {
    throw new Error("Base64DecodingNotSupportedError");
  }
}
