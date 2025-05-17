/**
 * A simple script to convert any string into a C file that can be embedded into
 * the application.
 *
 * Usage: hexembed <filename> <varname>
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  if (argc != 3) {
    fprintf(stderr, "Usage: hexembed <filename> <varname>\n");
    return 1;
  }

  const char *filename = argv[1];
  FILE *f = fopen(filename, "rb");
  if (!f) {
    fprintf(stderr, "Failed to read file '%s'\n", filename);
    return 1;
  }

  fseek(f, 0, SEEK_END);
  size_t size = ftell(f);

  fseek(f, 0, SEEK_SET);
  uint8_t *data = malloc(size);

  fread(data, size, 1, f);
  fclose(f);

  const char *varname = argv[2];
  printf("/* Contents of %s */\n", filename);
  printf("#pragma once\n\n");
  printf("#include <stddef.h>\n");
  printf("#include <stdint.h>\n\n");
  printf("const size_t %s_size = %ld;\n", varname, size);
  printf("const uint8_t %s_file[] = {\n", varname);

  for (size_t i = 0; i < size; i += 1) {
    printf("0x%02x%s", data[i],
           i == size - 1 ? "\n" : ((i + 1) % 16 == 0 ? ",\n" : ","));
  }

  printf("};\n");

  free(data);
  return 0;
}
