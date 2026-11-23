#pragma once

#include <cstdint>

#include "freertos/FreeRTOS.h"

namespace smartface {

struct TaskConfig {
    const char* name;
    uint32_t stackBytes;
    UBaseType_t priority;
    BaseType_t core;
};

}  // namespace smartface
