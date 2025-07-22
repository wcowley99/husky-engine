#pragma once

#include <stdint.h>
#include <stdlib.h>

#define IMPL_LIST_DEFINITION(x)                                                                    \
        typedef struct {                                                                           \
                x *data;                                                                           \
                size_t length;                                                                     \
                size_t capacity;                                                                   \
        } x##List;                                                                                 \
        x##List x##ListCreate(size_t capacity);                                                    \
        void x##ListPush(x##List *list, x element);                                                \
        x *x##ListReserve(x##List *list);                                                          \
        x *x##ListGet(x##List *list, size_t idx);                                                  \
        void x##ListFree(x##List *list);

#define IMPL_LIST_OPERATIONS(x)                                                                    \
        x##List x##ListCreate(size_t capacity) {                                                   \
                x *data = malloc(sizeof(x) * capacity);                                            \
                return (x##List){.data = data, .length = 0, .capacity = capacity};                 \
        }                                                                                          \
                                                                                                   \
        void x##ListPush(x##List *list, x element) {                                               \
                if (list->length == list->capacity) {                                              \
                        list->capacity *= 2;                                                       \
                        list->data = realloc(list->data, sizeof(x) * list->capacity);              \
                }                                                                                  \
                list->data[list->length] = element;                                                \
                list->length += 1;                                                                 \
        }                                                                                          \
                                                                                                   \
        x *x##ListReserve(x##List *list) {                                                         \
                if (list->length == list->capacity) {                                              \
                        list->capacity *= 2;                                                       \
                        list->data = realloc(list->data, sizeof(x) * list->capacity);              \
                }                                                                                  \
                                                                                                   \
                x *ret = &list->data[list->length];                                                \
                list->length += 1;                                                                 \
                return ret;                                                                        \
        }                                                                                          \
        x *x##ListGet(x##List *list, size_t idx) { return &list->data[idx]; }                      \
        void x##ListFree(x##List *list) { free(list->data); }

IMPL_LIST_DEFINITION(uint32_t)

char *ReadFile(const char *filename, size_t *filesize);
