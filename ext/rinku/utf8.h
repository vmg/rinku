#ifndef UTF8_H
#define UTF8_H

#include <stdint.h>

typedef int bufsize_t;

typedef struct {
  unsigned char *ptr;
  bufsize_t asize, size;
} rinku_strbuf;

void rinku_strbuf_put(rinku_strbuf *buf, const unsigned char *data,
                        bufsize_t len);

int rinku_ispunct(char c);
int rinku_isspace(char c);

void utf8proc_encode_char(int32_t uc, rinku_strbuf *buf);
int utf8proc_iterate(const uint8_t *str, bufsize_t str_len, int32_t *dst);
void utf8proc_check(rinku_strbuf *dest, const uint8_t *line,
                        bufsize_t size);
int utf8proc_is_space(int32_t uc);
int utf8proc_is_punctuation(int32_t uc);

#endif
