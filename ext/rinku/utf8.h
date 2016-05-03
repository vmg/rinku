#ifndef UTF8_H
#define UTF8_H

#include <stdint.h>

typedef int bufsize_t;

int rinku_ispunct(char c);
int rinku_isspace(char c);

static int utf8proc_charlen(const uint8_t *str, bufsize_t str_len);
int utf8proc_iterate(const uint8_t *str, bufsize_t str_len, int32_t *dst);
int utf8proc_is_space(int32_t uc);

#endif
