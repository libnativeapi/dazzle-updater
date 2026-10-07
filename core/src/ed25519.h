#pragma once

#include <string>

// Internal Ed25519 signature verification for installers, on top of Monocypher (see
// core/third_party/monocypher). It is not listed in API_HEADERS, so codegen ignores it
// and it reaches no binding.

namespace dazzle_updater {

/**
 * @brief Whether `public_key` is a usable Ed25519 public key.
 *
 * True when it is Base64 for exactly 32 bytes and is not a degenerate value a
 * misconfiguration produces.
 *
 * Monocypher checks Ed25519 with the cofactored equation, which by design accepts
 * low-order points; an all-zero key would therefore verify forged signatures. dazzle_updater
 * rejects the all-zero key and the identity point here. The application embeds this key,
 * so a remote attacker cannot substitute it; the check guards against shipping a key
 * that was never configured.
 *
 * @param public_key The Base64 encoded key to check.
 * @return true when the key can be used to verify signatures.
 */
bool IsValidPublicKey(const std::string& public_key);

/**
 * @brief Verifies a Base64 Ed25519 signature over `data`.
 *
 * @param public_key The Base64 encoded 32-byte public key.
 * @param signature The Base64 encoded 64-byte signature.
 * @param data The signed bytes.
 * @return true only when the signature is valid for that key and data.
 */
bool VerifySignature(const std::string& public_key, const std::string& signature,
                     const std::string& data);

/**
 * @brief Verifies a Base64 Ed25519 signature over the bytes of a file.
 *
 * The file is read into memory, because Monocypher hashes the whole message, so a large
 * installer costs its own size in memory while the check runs.
 *
 * @param public_key The Base64 encoded 32-byte public key.
 * @param signature The Base64 encoded 64-byte signature.
 * @param path The file whose bytes were signed.
 * @return true only when the file exists and its signature is valid.
 */
bool VerifyFileSignature(const std::string& public_key, const std::string& signature,
                         const std::string& path);

/**
 * @brief Decodes standard Base64, with or without padding.
 *
 * @param text The text to decode; whitespace is ignored.
 * @param bytes The decoded bytes, replaced on success.
 * @return true when the text is well formed Base64.
 */
bool DecodeBase64(const std::string& text, std::string* bytes);

/**
 * @brief Encodes bytes as standard Base64 with padding.
 *
 * @param bytes The bytes to encode.
 * @return The Base64 text.
 */
std::string EncodeBase64(const std::string& bytes);

}  // namespace dazzle_updater
