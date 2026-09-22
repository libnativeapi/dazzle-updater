#pragma once

#include <stdbool.h>
#include <stdint.h>

#if _WIN32
#define DAZZLE_EXPORT __declspec(dllexport)
#else
#define DAZZLE_EXPORT
#endif
