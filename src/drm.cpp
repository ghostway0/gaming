#include <ctime>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/utsname.h>

#include <absl/status/statusor.h>

#include "sunset/property_tree.h"
#include "config.h"

#include "sunset/drm.h"

std::string get_platform_info() {
  std::stringstream system_info;

#ifdef _WIN32
  system_info << "Windows-" << getenv("OS") << "-"
              << getenv("PROCESSOR_ARCHITECTURE");
#elif __linux__
  struct utsname uname_data;
  if (uname(&uname_data) == 0) {
    system_info << "Linux-" << uname_data.release << "-"
                << uname_data.machine;
  }
#elif __APPLE__
  struct utsname uname_data;
  if (uname(&uname_data) == 0) {
    system_info << "Darwin-" << uname_data.release << "-"
                << uname_data.machine;
  }
#else
  system_info << "Unknown-Unknown-Unknown";
#endif

  return system_info.str();
}

absl::Status validateLicense(std::string filename) {
  std::ifstream input(filename, std::ios::binary);
  auto tree = readPropertyTree(input);
  if (!tree.ok()) {
    return tree.status();
  }

  absl::StatusOr<License> license = deserializeTree<License>(tree.value());
  if (!license.ok()) {
    return license.status();
  }

  if (get_platform_info() != license->device_id) {
    return absl::InternalError("License wrong device id");
  }

  if (std::time(nullptr) > license->expiration) {
    return absl::InternalError("License expired");
  }

  // TODO: validate signature

  return absl::OkStatus();
}
