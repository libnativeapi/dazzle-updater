#pragma once

#include "common_c.h"

#ifdef __cplusplus
#include <string>

namespace dazzle_updater {

/**
 * Copy a C++ string into a newly allocated C string.
 * @return nullptr if str is empty or allocation failed; otherwise a string
 * the caller frees with dazzle_free_c_str().
 */
char* to_c_str(const std::string& str);

}  // namespace dazzle_updater
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Free a string returned by the dazzle_updater C API.
 * @param str The string to free (can be NULL)
 */
DAZZLE_EXPORT
void dazzle_free_c_str(char* str);

#ifdef __cplusplus
}
#endif
