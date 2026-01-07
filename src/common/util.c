#include "util.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

char *ReadFile(const char *filename, size_t *filesize) {
        FILE *f = fopen(filename, "rb");

        if (!f) {
                printf("Failed to open file %s: %s\n", filename, strerror(errno));
                return NULL;
        }

        fseek(f, 0, SEEK_END);
        *filesize = ftell(f);

        fseek(f, 0, SEEK_SET);
        char *data = malloc(*filesize + 1);

        if (!data) {
                fclose(f);
                printf("Failed to allocate %lu bytes for file contents.\n", *filesize + 1);
                return NULL;
        }

        fread(data, *filesize, 1, f);
        fclose(f);

        data[*filesize] = '\0';

        return data;
}
