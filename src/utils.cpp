#include "sunset/utils.h"

#include <cxxabi.h>
#include <string>
#include <memory>

std::string demangle(const char *mangled) {
  int status = 0;
  std::unique_ptr<char, void (*)(void *)> demangled(
      abi::__cxa_demangle(mangled, nullptr, nullptr, &status), std::free);
  return status == 0 ? demangled.get() : mangled;
}
