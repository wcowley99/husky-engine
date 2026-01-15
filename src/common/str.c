#include "str.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Str str_span(char *beg, char *end) {
        Str r = {0};

        r.data = beg;
        r.len = beg ? end - beg : 0;

        return r;
}

Str str_trimr(Str s) {
        while (s.len > 0 && s.data[s.len - 1] == ' ') {
                s.len -= 1;
        }

        return s;
}

Str str_triml(Str s) {
        while (s.len > 0 && s.data[0] == ' ') {
                s.len -= 1;
                s.data += 1;
        }

        return s;
}

char *str_to_chars(Str s) {
        char *r = malloc(s.len + 1);
        memcpy(r, s.data, s.len);
        r[s.len] = '\0';

        return r;
}

float str_to_float(Str s) {
        float r = 0.0f;
        float sign = 1.0f;
        float exp = 0.0f;

        for (size_t i = 0; i < s.len; i += 1) {
                switch (s.data[i]) {
                case '+':
                        break;
                case '-':
                        sign = -1.0f;
                        break;
                case '.':
                        exp = 1.0f;
                        break;
                case 'E':
                case 'e':
                        return 0; // probably small
                default:
                        r = 10.0f * r + (s.data[i] - '0');
                        exp *= 0.1f;
                }
        }

        return sign * r * (exp ? exp : 1.0f);
}

int32_t str_to_int(Str s) {
        int32_t r = 0;
        int32_t sign = 1;

        for (size_t i = 0; i < s.len; i += 1) {
                switch (s.data[i]) {
                case '+':
                        break;
                case '-':
                        sign = -1;
                        break;
                default:
                        r = 10 * r + (s.data[i] - '0');
                }
        }

        return r * sign;
}

bool str_equal(Str a, Str b) {
        return a.len == b.len && (a.len == 0 || !memcmp(a.data, b.data, a.len));
}

Cut make_cut(Str s, char c) {
        Cut r = {0};
        if (s.len == 0) {
                return r;
        }

        char *beg = s.data;
        char *end = s.data + s.len;
        char *cut = beg;

        while (cut < end && cut[0] != c) {
                cut++;
        }

        r.ok = cut < end;
        r.head = str_span(beg, cut);
        r.tail = str_span(cut + r.ok, end);

        return r;
}

Cut make_cutr(Str s, char c) {
        Cut r = {0};
        if (s.len == 0) {
                return r;
        }

        char *beg = s.data;
        char *end = s.data + s.len;
        char *cut = end - 1;

        while (cut >= beg && cut[0] != c) {
                cut--;
        }

        r.ok = cut >= beg;
        r.head = str_span(beg, cut);
        r.tail = str_span(cut + r.ok, end);

        return r;
}
