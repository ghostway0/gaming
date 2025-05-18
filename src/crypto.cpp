#include <vector>

#include <sodium.h>

#include "sunset/crypto.h"

bool signatureValid(const std::vector<uint8_t> &pubkey,
                    const std::vector<uint8_t> &signature,
                    const std::vector<uint8_t> &message) {
  int result = crypto_sign_verify_detached(
      reinterpret_cast<const uint8_t *>(signature.data()),
      reinterpret_cast<const uint8_t *>(message.data()), message.size(),
      reinterpret_cast<const uint8_t *>(pubkey.data()));

  return result == 0;
}

std::vector<uint8_t> hashContent(const std::vector<uint8_t> &content) {
  std::vector<uint8_t> hash(crypto_generichash_BYTES);
  crypto_generichash(
      hash.data(), hash.size(),
      reinterpret_cast<const unsigned char *>(content.data()),
      content.size(), nullptr, 0);

  return hash;
}
