#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
        char *data;
        size_t len;
} Str;

#define make_str(s) (Str){s, sizeof(s) - 1}

Str str_span(char *beg, char *end);

Str str_trimr(Str s);
Str str_triml(Str s);

float str_to_float(Str s);
int32_t str_to_int(Str s);

bool str_equal(Str a, Str b);

typedef struct {
        Str head;
        Str tail;
        bool ok;
} Cut;

Cut make_cut(Str s, char c);
