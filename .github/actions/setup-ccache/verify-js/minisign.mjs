// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file @kaito-tokyo/minisign-verify/verify-js/minisign.mjs
 * Self-contained Minisign signature verification library for ES Modules.
 * @version 0.1.3
 * @since 2026-03-29
 */

/**
 * @typedef {Object} Pubkey Public key of Minisign verification.
 * @property {CryptoKey} cryptoKey internal field
 * @property {'Ed'} sigAlg internal field
 * @property {Uint8Array<ArrayBuffer>} keynum internal field
 * @property {Uint8Array<ArrayBuffer>} pk internal field
 *
 * @typedef {Object} Sig Single signature data of Minisign verification.
 * @property {'Ed' | 'ED'} sigAlg internal field
 * @property {Uint8Array<ArrayBuffer>} keynum internal field
 * @property {Uint8Array<ArrayBuffer>} sig internal field
 *
 * @typedef {Object} SigFile Signature file of Minisign verification.
 * @property {string} comment internal field
 * @property {Sig} sig internal field
 * @property {string} trustedComment internal field
 * @property {Uint8Array<ArrayBuffer>} globalSig internal field
 *
 * @typedef {bigint} KeynumKey The key type for a Pubkey Map based on keynum.
 *
 * @typedef {Object} VerifyMinisignSuccessResult
 * @property {true} ok Overall verification result.
 * @property {string} trustedComment The verified trusted comment from the signature file.
 * @property {boolean} isPubkeyFound Whether the public key was found.
 * @property {boolean} isMessageValid The integrity of the data.
 * @property {boolean} isCommentValid The integrity of the signature file.
 *
 * @typedef {Object} VerifyMinisignFailureResult
 * @property {false} ok Overall verification result.
 * @property {boolean} isPubkeyFound Whether the public key was found.
 * @property {boolean} isMessageValid The integrity of the data.
 * @property {boolean} isCommentValid The integrity of the signature file.
 *
 * @typedef {VerifyMinisignSuccessResult | VerifyMinisignFailureResult} VerifyMinisignResult
 */

const KEYNUMBYTES = 8;
const SIGALG = "Ed";
const SIGALG_HASHED = "ED";
const COMMENT_PREFIX = "untrusted comment: ";
const TRUSTED_COMMENT_PREFIX = "trusted comment: ";

const crypto_sign_PUBLICKEYBYTES = 32;
const crypto_sign_BYTES = 64;

/**
 * Asynchronously reads a Minisign public key in the form of Base64-string.
 * @param {typeof import("./base64.mjs").arrayBufferToBase64url} $arrayBufferToBase64url
 * @param {typeof import("./base64.mjs").base64ToUint8Array} $base64ToUint8Array
 * @param {string} pubkeyString The Base64-string of the public key.
 * @return {Promise<Pubkey>}
 */
export async function readPubkey($arrayBufferToBase64url, $base64ToUint8Array, pubkeyString) {
  const bytes = $base64ToUint8Array(pubkeyString);

  if (bytes.length !== 2 + KEYNUMBYTES + crypto_sign_PUBLICKEYBYTES) {
    throw new Error("InvalidPubkeyLengthError");
  }

  const sigAlgBytes = bytes.subarray(0, 2);
  const keynum = bytes.subarray(2, 2 + KEYNUMBYTES);
  const pk = bytes.subarray(2 + KEYNUMBYTES);

  if (sigAlgBytes[0] !== /*E*/ 0x45 || sigAlgBytes[1] !== /*d*/ 0x64) {
    throw new Error("InvalidSigAlgError");
  }

  const cryptoKey = await crypto.subtle.importKey(
    "jwk",
    {
      kty: "OKP",
      crv: "Ed25519",
      x: await $arrayBufferToBase64url(pk),
    },
    { name: "Ed25519" },
    false,
    ["verify"],
  );

  return {
    cryptoKey,
    sigAlg: SIGALG,
    keynum,
    pk,
  };
}

/**
 * Parses a Minisign signature in the form of Base64-string.
 * @param {typeof import("./base64.mjs").base64ToUint8Array} $base64ToUint8Array
 * @param {string} sigString The Base64-string of the signature.
 * @return {Sig}
 */
export function parseSig($base64ToUint8Array, sigString) {
  const bytes = $base64ToUint8Array(sigString);

  if (bytes.length !== 2 + KEYNUMBYTES + crypto_sign_BYTES) {
    throw new Error("InvalidSigLengthError");
  }

  const sigAlgBytes = bytes.subarray(0, 2);
  const keynum = bytes.subarray(2, 2 + KEYNUMBYTES);
  const sig = bytes.subarray(2 + KEYNUMBYTES);

  /** @type {'Ed' | 'ED' | undefined} */
  let sigAlg;
  if (sigAlgBytes[0] === /*E*/ 0x45) {
    if (sigAlgBytes[1] === /*d*/ 0x64) {
      sigAlg = SIGALG;
    } else if (sigAlgBytes[1] === /*D*/ 0x44) {
      sigAlg = SIGALG_HASHED;
    }
  }

  if (!sigAlg) {
    throw new Error("InvalidSigAlgError");
  }

  return { sigAlg, keynum, sig };
}

/**
 * Parses a `.minisig` file content.
 * @param {typeof parseSig} $parseSig
 * @param {typeof import("./base64.mjs").base64ToUint8Array} $base64ToUint8Array
 * @param {string} sigFileContent The content of the `.minisig` file as a string.
 * @return {SigFile}
 */
export function parseSigFile($parseSig, $base64ToUint8Array, sigFileContent) {
  const lines = sigFileContent.split(/\r?\n/);

  if (lines.length < 4) {
    throw new Error("InvalidSigContentError");
  }

  const [commentLine, sigString, trustedCommentLine, globalSigString] = lines;

  if (!commentLine.startsWith(COMMENT_PREFIX)) {
    throw new Error("InvalidUntrustedCommentError");
  }

  if (!trustedCommentLine.startsWith(TRUSTED_COMMENT_PREFIX)) {
    throw new Error("InvalidTrustedCommentError");
  }

  const comment = commentLine.substring(COMMENT_PREFIX.length);
  const sig = $parseSig($base64ToUint8Array, sigString);
  const trustedComment = trustedCommentLine.substring(
    TRUSTED_COMMENT_PREFIX.length,
  );
  const globalSig = $base64ToUint8Array(globalSigString);

  if (globalSig.length !== crypto_sign_BYTES) {
    throw new Error("InvalidGlobalSigLengthError");
  }

  return { comment, sig, trustedComment, globalSig };
}

/**
 * Gets the key for Map<PubkeyMapKey, Pubkey> from a Pubkey.
 * @param {Pubkey | Sig} pubkey
 * @return {KeynumKey}
 */
export function getKeynumKey(pubkey) {
  const { keynum } = pubkey;
  return new DataView(keynum.buffer, keynum.byteOffset, keynum.byteLength).getBigUint64(0, true);
}

/**
 * @param {Pubkey | Map<KeynumKey, Pubkey>} pubkeys The Minisign public key(s) from `readPubkey()`.
 *   Provide a Pubkey, or construct a map with `new Map(pubkeys.map(pk => [getKeynumKey(pk), pk]))`.
 * @param {SigFile} sigFile The `.minisig` content from `parseSigFile()`.
 * @param {(blake2b512Required: boolean) => Promise<ArrayBuffer | ArrayBufferView<ArrayBuffer>>} dataFunc
 *   Callback to get the data to verify. Called exactly once if no exception is thrown, and never called more than once.
 *   If `blake2b512Required` is `true`: Return BLAKE2b-512 hash of the data to verify.
 *   If `blake2b512Required` is `false`: Return the raw data.
 * @return {Promise<VerifyMinisignResult>} The result of the verification.
 *   Use its `ok` property to check if verification was successful.
 */
export async function verifyMinisign(pubkeys, sigFile, dataFunc) {
  const sigKeynumKey = getKeynumKey(sigFile.sig);
  const pubkey = pubkeys instanceof Map ? pubkeys.get(sigKeynumKey) : pubkeys;

  if (!pubkey || getKeynumKey(pubkey) !== sigKeynumKey) {
    return {
      ok: false,
      isPubkeyFound: false,
      isMessageValid: false,
      isCommentValid: false,
    };
  }

  const { cryptoKey } = pubkey;
  const { sig: { sigAlg, sig }, trustedComment, globalSig } = sigFile;

  if (sigAlg !== SIGALG_HASHED && sigAlg !== SIGALG) {
    throw new Error("UnsupportedSigAlgError");
  }

  const isMessageValid = await crypto.subtle.verify(
    { name: "Ed25519" },
    cryptoKey,
    sig,
    await dataFunc(sigAlg === SIGALG_HASHED),
  );

  const trustedCommentBytes = new TextEncoder().encode(trustedComment);
  const sigAndTrustedComment = new Uint8Array(
    sig.length + trustedCommentBytes.length,
  );
  sigAndTrustedComment.set(sig, 0);
  sigAndTrustedComment.set(trustedCommentBytes, sig.length);

  const isCommentValid = await crypto.subtle.verify(
    { name: "Ed25519" },
    cryptoKey,
    globalSig,
    sigAndTrustedComment,
  );

  const ok = isMessageValid && isCommentValid;
  if (ok) {
    return {
      ok,
      trustedComment,
      isPubkeyFound: true,
      isMessageValid,
      isCommentValid,
    };
  } else {
    return {
      ok,
      isPubkeyFound: true,
      isMessageValid,
      isCommentValid,
    };
  }
}
