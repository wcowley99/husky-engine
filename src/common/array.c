#include "array.h"

#include <stdlib.h>
#include <string.h>

void *array_init(size_t item_size, size_t capacity) {
        size_t array_size = item_size * capacity + sizeof(ArrayHeader);
        ArrayHeader *h = malloc(array_size);

        if (!h) {
                return NULL;
        }

        h->capacity = capacity;
        h->length = 0;

        return h + 1;
}

void *array_ensure_capacity(void *a, size_t count, size_t item_size) {
        ArrayHeader *h = array_header(a);
        size_t desired_capacity = h->length + count;

        if (h->capacity < desired_capacity) {
                size_t new_capacity = h->capacity * 1.5;
                while (new_capacity < desired_capacity) {
                        new_capacity *= 1.5;
                }

                size_t new_size = sizeof(ArrayHeader) + new_capacity * item_size;
                ArrayHeader *new_h = malloc(new_size);
                if (new_h) {
                        size_t old_size = sizeof(ArrayHeader) + h->length * item_size;
                        memcpy(new_h, h, old_size);

                        free(h);

                        new_h->capacity = new_capacity;

                        return new_h + 1;
                } else {
                        return NULL;
                }
        } else {
                return h + 1;
        }
}

void array_free(void *a) {
        ArrayHeader *h = array_header(a);
        free(h);
}
