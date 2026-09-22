#include "string_utils_c.h"

#include <cstring>
#include <new>

namespace dazzle {

char* to_c_str(const std::string& str) {
  if (str.empty())
    return nullptr;

  char* result = new (std::nothrow) char[str.length() + 1];
  if (result) {
    std::memcpy(result, str.c_str(), str.length() + 1);
  }
  return result;
}

}  // namespace dazzle

void dazzle_free_c_str(char* str) {
  delete[] str;
}
