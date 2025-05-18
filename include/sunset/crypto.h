#include <cstdint>
#include <vector>

bool signatureValid(const std::vector<uint8_t> &pubkey,
                     const std::vector<uint8_t> &signature,
                     const std::vector<uint8_t> &message);

std::vector<uint8_t> hashContent(const std::vector<uint8_t> &content);
