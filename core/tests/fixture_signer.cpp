// Test fixture for the macOS install end-to-end script: it creates an Ed25519 key pair
// and signs a package with the same Monocypher build the library verifies with. It is
// not part of the library or of any binding; it stands in for the `dazzle sign` command
// that ../specs/updater.md plans.
//
// Usage:
//   fixture_signer gen-key                prints "public <base64>" and "secret <base64>"
//   fixture_signer sign <secret> <file>   prints the Base64 signature of <file>
#include "../src/ed25519.h"

#include <stddef.h>
#include <stdint.h>

#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>

extern "C" {
#include "../third_party/monocypher/monocypher-ed25519.h"
}

namespace {

bool ReadFile(const std::string& path, std::string* data) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return false;
  }
  *data = std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
  return !file.bad();
}

bool RandomSeed(uint8_t seed[32]) {
  std::ifstream random("/dev/urandom", std::ios::binary);
  if (!random) {
    return false;
  }
  random.read(reinterpret_cast<char*>(seed), 32);
  return random.good();
}

}  // namespace

int main(int argc, char** argv) {
  if (argc == 2 && std::string(argv[1]) == "gen-key") {
    uint8_t seed[32];
    if (!RandomSeed(seed)) {
      std::fprintf(stderr, "fixture_signer: cannot read /dev/urandom\n");
      return 1;
    }
    uint8_t secret_key[64];
    uint8_t public_key[32];
    crypto_ed25519_key_pair(secret_key, public_key, seed);
    std::printf("public %s\n",
                dazzle::EncodeBase64(std::string(reinterpret_cast<char*>(public_key), 32)).c_str());
    std::printf("secret %s\n",
                dazzle::EncodeBase64(std::string(reinterpret_cast<char*>(secret_key), 64)).c_str());
    return 0;
  }
  if (argc == 4 && std::string(argv[1]) == "sign") {
    std::string secret_key;
    if (!dazzle::DecodeBase64(argv[2], &secret_key) || secret_key.size() != 64) {
      std::fprintf(stderr, "fixture_signer: the secret key is not a Base64 64-byte Ed25519 key\n");
      return 1;
    }
    std::string data;
    if (!ReadFile(argv[3], &data)) {
      std::fprintf(stderr, "fixture_signer: cannot read %s\n", argv[3]);
      return 1;
    }
    uint8_t signature[64];
    crypto_ed25519_sign(signature, reinterpret_cast<const uint8_t*>(secret_key.data()),
                        reinterpret_cast<const uint8_t*>(data.data()), data.size());
    std::printf("%s\n",
                dazzle::EncodeBase64(std::string(reinterpret_cast<char*>(signature), 64)).c_str());
    return 0;
  }
  std::fprintf(stderr,
               "usage: fixture_signer gen-key | fixture_signer sign <secret> <file>\n");
  return 2;
}
