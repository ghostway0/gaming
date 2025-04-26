#include <string>
#include <vector>
#include <cstdint>

#include <absl/status/statusor.h>

struct License {
  std::vector<uint8_t> file_hash;
  std::string device_id;
  uint64_t expiration;
};

absl::Status validateLicense(std::string filename);
