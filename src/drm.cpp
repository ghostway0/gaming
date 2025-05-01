#include <ctime>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/utsname.h>

#include <absl/status/statusor.h>

#include "sunset/property_tree.h"
#include "config.h"

#include "sunset/drm.h"

#ifdef __linux__
#include <sys/utsname.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <linux/if_arp.h>
#elif _WIN32
#include <windows.h>
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#endif

std::string trim(const std::string &str) {
  size_t start = str.find_first_not_of(" \n\r\t");
  size_t end = str.find_last_not_of(" \n\r\t");
  if (start == std::string::npos || end == std::string::npos) {
    return "";
  }
  return str.substr(start, end - start + 1);
}

std::string getPlatformInfo() {
  std::stringstream system_info;

#ifdef _WIN32
  SYSTEM_INFO siSysInfo;
  GetSystemInfo(&siSysInfo);
  system_info << "Windows-" << siSysInfo.wProcessorArchitecture;

  IP_ADAPTER_INFO adapterInfo[16];
  DWORD dwBufLen = sizeof(adapterInfo);
  DWORD dwStatus = GetAdaptersInfo(adapterInfo, &dwBufLen);
  if (dwStatus == ERROR_SUCCESS) {
    system_info << "-";
    for (int i = 0; i < 6; i++) {
      system_info << std::hex << (int)adapterInfo[0].Address[i];
      if (i != 5) system_info << ":";
    }
  }

#elif __linux__
  struct utsname uname_data;
  if (uname(&uname_data) == 0) {
    system_info << "Linux-" << uname_data.machine;
  }

  std::ifstream machine_id_file("/etc/machine-id");
  std::string machine_id;
  if (machine_id_file.is_open()) {
    std::getline(machine_id_file, machine_id);
    system_info << "-" << trim(machine_id);
  }

#elif __APPLE__
#include <sys/sysctl.h>
  system_info << "Darwin-";
  FILE *fp_cpu = popen("sysctl -n machdep.cpu.brand_string", "r");
  char buffer[128];
  if (fp_cpu) {
    while (fgets(buffer, sizeof(buffer), fp_cpu) != NULL) {
      system_info << trim(buffer);
    }
    pclose(fp_cpu);
  }

  FILE *fp_uuid = popen(
      "system_profiler SPHardwareDataType | grep 'Hardware UUID'", "r");
  if (fp_uuid) {
    while (fgets(buffer, sizeof(buffer), fp_uuid) != NULL) {
      std::string uuid_str(buffer);
      size_t pos = uuid_str.find(":");
      if (pos != std::string::npos) {
        system_info << "-" << trim(uuid_str.substr(pos + 2));
      }
    }
    pclose(fp_uuid);
  }

#else
  system_info << "Unknown-Unknown-Unknown";
#endif

  return system_info.str();
}

absl::Status validateLicense(std::string filename) {
  LOG(INFO) << getPlatformInfo();
  std::ifstream input(filename, std::ios::binary);
  auto tree = readPropertyTree(input);
  if (!tree.ok()) {
    return tree.status();
  }

  absl::StatusOr<License> license = deserializeTree<License>(tree.value());
  if (!license.ok()) {
    return license.status();
  }

  if (getPlatformInfo() != license->device_id) {
    return absl::InternalError("License wrong device id");
  }

  if (std::time(nullptr) > license->expiration) {
    return absl::InternalError("License expired");
  }

  // TODO: validate signature

  return absl::OkStatus();
}
