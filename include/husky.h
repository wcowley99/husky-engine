#pragma once

#include "common/log.h"

#define ASSERT(x)                                                                                  \
        do {                                                                                       \
                if (!(x)) {                                                                        \
                        ERROR("ASSERT failed: %s", #x);                                            \
                        exit(1);                                                                   \
                }                                                                                  \
        } while (0)
