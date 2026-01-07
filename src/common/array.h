#pragma once

#include <stddef.h>

#define ARRAY_INITIAL_CAPACITY 16

typedef struct {
        size_t capacity;
        size_t length;
} ArrayHeader;

#define array(T) (T *)array_init(sizeof(T), ARRAY_INITIAL_CAPACITY)

#define array_append(a, v)                                                                         \
        ((a) = array_ensure_capacity(a, 1, sizeof(v)), (a)[array_header(a)->length] = (v),         \
         array_header(a)->length++)

#define array_header(a) ((ArrayHeader *)(a) - 1)
#define array_length(a) (array_header(a)->length)
#define array_capacity(a) (array_header(a)->capacity)

void *array_init(size_t item_size, size_t capacity);
void *array_ensure_capacity(void *a, size_t count, size_t item_size);

void array_free(void *a);
