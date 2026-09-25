#include "test_support.h"

#include "../src/ed25519.h"

#include <string>

using dazzle::DecodeBase64;
using dazzle::IsValidPublicKey;
using dazzle::VerifySignature;

namespace {

// RFC 8032, section 7.1, test 1: the empty message.
const char* const kPublicKey1 =
    "11qYAYKxCrfVS/7TyWQHOg7hcvPapiMlrwIaaPcHURo=";
const char* const kSignature1 =
    "5VZDAMNgrHKQhuLMgG6CioSHfx645dl02HPgZSJJAVVfuIIVkKM7rMYeOXAc+bRr0lv18FlbviRlUUFDjnoQCw==";

// RFC 8032, section 7.1, test 2: the one-byte message 0x72.
const char* const kPublicKey2 =
    "PUAXw+hDiVqStwqnTRt+vJyYLM8uxJaMwM1V8Sr0Zgw=";
const char* const kSignature2 =
    "kqAJqfDUyrhyDoILX2QlQKKye1QWUD+Ps3YiI+vbadoIWsHkPhWZbkWPNhPQ8R2MOHsurrQwKu6wDSkWErsMAA==";

const char* const kZeroKey =
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=";
const char* const kIdentityKey =
    "AQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=";

}  // namespace

DAZZLE_TEST(base64_decodes_padded_and_unpadded_text) {
  std::string bytes;
  EXPECT_TRUE(DecodeBase64("TWFu", &bytes));
  EXPECT_EQ(bytes, std::string("Man"));
  EXPECT_TRUE(DecodeBase64("TWE=", &bytes));
  EXPECT_EQ(bytes, std::string("Ma"));
  EXPECT_TRUE(DecodeBase64("TWE", &bytes));
  EXPECT_EQ(bytes, std::string("Ma"));
  EXPECT_TRUE(DecodeBase64("TQ==", &bytes));
  EXPECT_EQ(bytes, std::string("M"));
  EXPECT_TRUE(DecodeBase64("", &bytes));
  EXPECT_EQ(bytes, std::string(""));
  EXPECT_TRUE(DecodeBase64(" T W F u ", &bytes));  // Whitespace is ignored.
  EXPECT_EQ(bytes, std::string("Man"));
}

DAZZLE_TEST(base64_rejects_malformed_text) {
  std::string bytes;
  EXPECT_FALSE(DecodeBase64("TWF!", &bytes));
  EXPECT_FALSE(DecodeBase64("TWFu=", &bytes));
  EXPECT_FALSE(DecodeBase64("TWFu==", &bytes));
  EXPECT_FALSE(DecodeBase64("TWFu===", &bytes));
  EXPECT_FALSE(DecodeBase64("T", &bytes));
}

DAZZLE_TEST(public_keys_are_checked) {
  EXPECT_TRUE(IsValidPublicKey(kPublicKey1));
  EXPECT_TRUE(IsValidPublicKey(kPublicKey2));
  EXPECT_FALSE(IsValidPublicKey(""));
  EXPECT_FALSE(IsValidPublicKey("AAAA"));        // Too short for a key.
  EXPECT_FALSE(IsValidPublicKey(kZeroKey));      // The key was never configured.
  EXPECT_FALSE(IsValidPublicKey(kIdentityKey));  // A low-order point.
  EXPECT_FALSE(IsValidPublicKey("not base64!"));
}

DAZZLE_TEST(ed25519_verifies_known_vectors) {
  EXPECT_TRUE(VerifySignature(kPublicKey1, kSignature1, ""));
  EXPECT_TRUE(VerifySignature(kPublicKey2, kSignature2, std::string(1, '\x72')));
}

DAZZLE_TEST(ed25519_rejects_forgeries) {
  // The right signature over the wrong message.
  EXPECT_FALSE(VerifySignature(kPublicKey1, kSignature1, "x"));
  // A tampered signature, still well formed Base64.
  std::string tampered = kSignature2;
  tampered[0] = tampered[0] == 'k' ? 'l' : 'k';
  EXPECT_FALSE(VerifySignature(kPublicKey2, tampered, std::string(1, '\x72')));
  // The right signature checked against another key.
  EXPECT_FALSE(VerifySignature(kPublicKey1, kSignature2, std::string(1, '\x72')));
  // A degenerate key never verifies, even with a real signature.
  EXPECT_FALSE(VerifySignature(kZeroKey, kSignature1, ""));
  EXPECT_FALSE(VerifySignature(kIdentityKey, kSignature1, ""));
  // A truncated signature.
  EXPECT_FALSE(VerifySignature(kPublicKey1, "AAAA", ""));
}
