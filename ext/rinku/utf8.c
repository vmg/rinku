#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#include "buffer.h"
#include "utf8.h"

static const int8_t utf8proc_utf8class[256] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    4, 4, 4, 4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0};

static int utf8proc_charlen(const uint8_t *str, bufsize_t str_len) {
  int length, i;

  if (!str_len)
    return 0;

  length = utf8proc_utf8class[str[0]];

  if (!length)
    return -1;

  if (str_len >= 0 && (bufsize_t)length > str_len)
    return -str_len;

  for (i = 1; i < length; i++) {
    if ((str[i] & 0xC0) != 0x80)
      return -i;
  }

  return length;
}

// Validate a single UTF-8 character according to RFC 3629.
static int utf8proc_valid(const uint8_t *str, bufsize_t str_len) {
  int length = utf8proc_utf8class[str[0]];

  if (!length)
    return -1;

  if ((bufsize_t)length > str_len)
    return -str_len;

  switch (length) {
  case 2:
    if ((str[1] & 0xC0) != 0x80)
      return -1;
    if (str[0] < 0xC2) {
      // Overlong
      return -length;
    }
    break;

  case 3:
    if ((str[1] & 0xC0) != 0x80)
      return -1;
    if ((str[2] & 0xC0) != 0x80)
      return -2;
    if (str[0] == 0xE0) {
      if (str[1] < 0xA0) {
        // Overlong
        return -length;
      }
    } else if (str[0] == 0xED) {
      if (str[1] >= 0xA0) {
        // Surrogate
        return -length;
      }
    }
    break;

  case 4:
    if ((str[1] & 0xC0) != 0x80)
      return -1;
    if ((str[2] & 0xC0) != 0x80)
      return -2;
    if ((str[3] & 0xC0) != 0x80)
      return -3;
    if (str[0] == 0xF0) {
      if (str[1] < 0x90) {
        // Overlong
        return -length;
      }
    } else if (str[0] >= 0xF4) {
      if (str[0] > 0xF4 || str[1] >= 0x90) {
        // Above 0x10FFFF
        return -length;
      }
    }
    break;
  }

  return length;
}

int utf8proc_iterate(const uint8_t *str, bufsize_t str_len,
                           int32_t *dst) {
  int length;
  int32_t uc = -1;

  *dst = -1;
  length = utf8proc_charlen(str, str_len);
  if (length < 0)
    return -1;

  switch (length) {
  case 1:
    // 1-byte wide character, so just return it
    uc = str[0];
    break;
  case 2:
    /* 2-bytes wide, so do some bitshift magic and add the value of the next
     * byte in the string to find its "uc" (numerical representation?)
     */
    uc = ((str[0] & 0x1F) << 6) + (str[1] & 0x3F);
    /* if the uc is less than 128, there's a problem (since it's not supposed to
     * be a 7-bit character?!) */
    if (uc < 0x80)
      uc = -1;
    break;
  case 3:
    uc = ((str[0] & 0x0F) << 12) + ((str[1] & 0x3F) << 6) + (str[2] & 0x3F);
    if (uc < 0x800 || (uc >= 0xD800 && uc < 0xE000))
      uc = -1;
    break;
  case 4:
    uc = ((str[0] & 0x07) << 18) + ((str[1] & 0x3F) << 12) +
         ((str[2] & 0x3F) << 6) + (str[3] & 0x3F);
    if (uc < 0x10000 || uc >= 0x110000)
      uc = -1;
    break;
  }

  if (uc < 0)
    return -1;

  *dst = uc;
  return length;
}

// matches anything in the Zs class, plus LF, CR, TAB, FF.
int utf8proc_is_space(int32_t uc) {
  return (uc == 9 || uc == 10 || uc == 12 || uc == 13 || uc == 32 ||
          uc == 160 || uc == 5760 || (uc >= 8192 && uc <= 8202) || uc == 8239 ||
          uc == 8287 || uc == 12288);
}


/** 1 = space, 2 = punct, 3 = digit, 4 = alpha, 0 = other
 */
static const uint8_t rinku_ctype_class[256] = {
    /*      0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f */
    /* 0 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0,
    /* 1 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 2 */ 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    /* 3 */ 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 2, 2, 2, 2,
    /* 4 */ 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    /* 5 */ 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2, 2, 2, 2, 2,
    /* 6 */ 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    /* 7 */ 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2, 2, 2, 2, 0,
    /* 8 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 9 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* a */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* b */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* c */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* d */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* e */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* f */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

/**
 * Returns 1 if c is an ascii punctuation character.
 */
int rinku_ispunct(char c) { return rinku_ctype_class[(uint8_t)c] == 2; }

/**
 * Returns 1 if c is a "whitespace" character as defined by the spec.
 */
int rinku_isspace(char c) { return rinku_ctype_class[(uint8_t)c] == 1; }
