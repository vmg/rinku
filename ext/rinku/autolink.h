/*
 * Copyright (c) 2011, Vicent Marti
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef UPSKIRT_AUTOLINK_H
#define UPSKIRT_AUTOLINK_H

#include <stdbool.h>
#include "buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
	SD_AUTOLINK_SHORT_DOMAINS = (1 << 0),
};

struct sd_link_pos {
	size_t start;
	size_t end;
};

int
sd_autolink_issafe(const uint8_t *link, size_t link_len);

bool
sd_autolink__www(struct sd_link_pos *res,
	const uint8_t *data, size_t pos, size_t size, unsigned int flags);

bool
sd_autolink__email(struct sd_link_pos *res,
	const uint8_t *data, size_t pos, size_t size, unsigned int flags);

bool
sd_autolink__url(struct sd_link_pos *res,
	const uint8_t *data, size_t pos, size_t size, unsigned int flags);

#ifdef __cplusplus
}
#endif

#endif

/* vim: set filetype=c: */
