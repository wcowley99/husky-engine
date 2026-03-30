#pragma once

#include <stdio.h>

#define ASSERT(x)                                                                                  \
        do {                                                                                       \
                if (!(x)) {                                                                        \
                        printf("[%s:%d] ASSERT failed: %s\n", __FILE__, __LINE__, #x);             \
                        exit(1);                                                                   \
                }                                                                                  \
        } while (0)
