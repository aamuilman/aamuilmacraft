#pragma once

#include <stdint.h>
#include <stddef.h>

namespace utf8 {
static constexpr uint32_t REPLACEMENT = 0xFFFD;

uint32_t next(const char** p);

size_t count(const char* s);
}
