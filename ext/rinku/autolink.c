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

#include "buffer.h"
#include "autolink.h"
#include "utf8.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#if defined(_WIN32)
#define strncasecmp	_strnicmp
#endif

int
sd_autolink_issafe(const uint8_t *link, size_t link_len)
{
	static const size_t valid_uris_count = 5;
	static const char *valid_uris[] = {
		"/", "http://", "https://", "ftp://", "mailto:"
	};

	size_t i;

	for (i = 0; i < valid_uris_count; ++i) {
		size_t len = strlen(valid_uris[i]);

		if (link_len > len &&
			strncasecmp((char *)link, valid_uris[i], len) == 0 &&
			isalnum(link[len]))
			return 1;
	}

	return 0;
}

static size_t
autolink_delim(uint8_t *data, size_t link_end, size_t max_rewind, size_t size)
{
	uint8_t cclose, copen = 0;
	size_t i;

	for (i = 0; i < link_end; ++i)
		if (data[i] == '<') {
			link_end = i;
			break;
		}

	while (link_end > 0) {
		if (strchr("?!.,:", data[link_end - 1]) != NULL)
			link_end--;

		else if (data[link_end - 1] == ';') {
			size_t new_end = link_end - 2;

			while (new_end > 0 && isalpha(data[new_end]))
				new_end--;

			if (new_end < link_end - 2 && data[new_end] == '&')
				link_end = new_end;
			else
				link_end--;
		}
		else break;
	}

	if (link_end == 0)
		return 0;

	cclose = data[link_end - 1];

	switch (cclose) {
	case '"':	copen = '"'; break;
	case '\'':	copen = '\''; break;
	case ')':	copen = '('; break;
	case ']':	copen = '['; break;
	case '}':	copen = '{'; break;
	}

	if (copen != 0) {
		size_t closing = 0;
		size_t opening = 0;
		size_t i = 0;

		/* Try to close the final punctuation sign in this same line;
		 * if we managed to close it outside of the URL, that means that it's
		 * not part of the URL. If it closes inside the URL, that means it
		 * is part of the URL.
		 *
		 * Examples:
		 *
		 *	foo http://www.pokemon.com/Pikachu_(Electric) bar
		 *		=> http://www.pokemon.com/Pikachu_(Electric)
		 *
		 *	foo (http://www.pokemon.com/Pikachu_(Electric)) bar
		 *		=> http://www.pokemon.com/Pikachu_(Electric)
		 *
		 *	foo http://www.pokemon.com/Pikachu_(Electric)) bar
		 *		=> http://www.pokemon.com/Pikachu_(Electric))
		 *
		 *	(foo http://www.pokemon.com/Pikachu_(Electric)) bar
		 *		=> foo http://www.pokemon.com/Pikachu_(Electric)
		 */

		while (i < link_end) {
			if (data[i] == copen)
				opening++;
			else if (data[i] == cclose)
				closing++;

			i++;
		}

		if (closing != opening)
			link_end--;
	}

	return link_end;
}

static size_t
check_domain(uint8_t *data, size_t size, int allow_short)
{
	size_t i, np = 0;

	if (!isalnum(data[0]))
		return 0;

	for (i = 1; i < size - 1; ++i) {
		if (data[i] == '.') np++;
		else if (!isalnum(data[i]) && data[i] != '-') break;
	}

	if (allow_short) {
		/* We don't need a valid domain in the strict sense (with
		 * least one dot; so just make sure it's composed of valid
		 * domain characters and return the length of the the valid
		 * sequence. */
		return i;
	} else {
		/* a valid domain needs to have at least a dot.
		 * that's as far as we get */
		return np ? i : 0;
	}
}

int
autolink_rewind_unless_isspace(
	uint8_t *data,
	size_t position,
	size_t max_rewind,
	int *rewind_steps)
{
	/* Rewind to find the next character by examining bytes */

	/* set default value of rewind_steps, which will report how many steps
	 * backwards we had to go to find the codepoint boundry */
	*rewind_steps = 1;
	uint8_t str = 0;
	bufsize_t str_len = 0;
	int32_t uc = -1;

	if ((data[position] & 0xC0) != 0x80) {
		/* ASCII, so use existing C functions for now */
		if (isalnum(data[position]) ||
			strchr(".+-_", data[position]) != NULL)
			return 1;
		return 0;
	}
	else if ((data[position - 1] & 0xC0) != 0x80) {
		/* 2-bytes wide */
		str_len = 2;
		*rewind_steps = 2;
		uc = ((data[position - 1] & 0x1F) << 6) + (data[position] & 0x3F);

		return !utf8proc_is_space(uc);
	}
	else if ((data[position - 2] & 0xC0) == 0x80) {
		/* 3-bytes wide */
		str_len = 3;
		*rewind_steps = 3;
		uc = ((data[position - 2] & 0x3F) << 12) +
			((data[position - 1] & 0x3F) << 6) +
			(data[position] & 0x3F);

		return !utf8proc_is_space(uc);
	}
	else if ((data[position - 3] & 0xC0) == 0x80) {
		/* 4-bytes wide */
		str_len = 4;
		*rewind_steps = 4;
		uc = ((data[position - 3] & 0x07) << 18) +
			((data[position - 2] & 0x3F) << 12) +
			((data[position - 1] & 0x3F) << 6) +
			(data[position] & 0x3F);

		return !utf8proc_is_space(uc);
	}

	/* something weird is going on, return -1 */
	return -1;
}

/* triggered on finding a w' or 'W' character in our text string */
size_t
sd_autolink__www(
	size_t *rewind_p,
	struct buf *link,
	uint8_t *data,
	size_t max_rewind,
	size_t size,
	unsigned int flags)
{
	size_t link_end;

	/* we need to have a function that rewinds to previous codepoints, by
	 * checking the previous byte to see if its a 0-bit on the previous byte.
	 * if its a 1, we need to go back, but no further than the max_rewind
	 */
	if (max_rewind > 0 && !ispunct(data[-1]) && !rinku_isspace(data[-1]))
		return 0;

	if (size < 4 || memcmp(data, "www.", strlen("www.")) != 0)
		return 0;

	link_end = check_domain(data, size, 0);

	if (link_end == 0)
		return 0;

	while (link_end < size && !rinku_isspace(data[link_end]))
		link_end++;

	link_end = autolink_delim(data, link_end, max_rewind, size);

	if (link_end == 0)
		return 0;

	bufput(link, data, link_end);
	*rewind_p = 0;

	return (int)link_end;
}

/* triggered on finding a '@' character in our text string */
size_t
sd_autolink__email(
	size_t *rewind_p,
	struct buf *link,
	uint8_t *data,
	size_t max_rewind,
	size_t size,
	unsigned int flags)
{
	size_t link_end, rewind = 0;
	int nb = 0, np = 0;

	/* rewind until we find a character that isn't an alphanumeric or
	 * an acceptable email address character (., +, -, or _)
	 */

	int rewind_steps = 0;
	while (rewind < max_rewind && (autolink_rewind_unless_isspace(data, -rewind - 1, max_rewind, &rewind_steps)))
		rewind = rewind + rewind_steps;

	if (rewind == 0)
		return 0;

	for (link_end = 0; link_end < size; ++link_end) {
		uint8_t c = data[link_end];

		if (isalnum(c))
			continue;

		if (c == '@')
			nb++;
		else if (c == '.' && link_end < size - 1)
			np++;
		else if (c != '-' && c != '_')
			break;
	}

	if (link_end < 2 || nb != 1 || np == 0)
		return 0;

	link_end = autolink_delim(data, link_end, max_rewind, size);

	if (link_end == 0)
		return 0;

	bufput(link, data - rewind, link_end + rewind);
	*rewind_p = rewind;

	return link_end;
}

/* triggered on finding a ':' character in our text string */
size_t
sd_autolink__url(
	size_t *rewind_p,
	struct buf *link,
	uint8_t *data,
	size_t max_rewind,
	size_t size,
	unsigned int flags)
{
	size_t link_end, rewind = 0, domain_len;
	/* if the string is too short, or the next 2 characters are not `/`s */
	if (size < 4 || data[1] != '/' || data[2] != '/')
		return 0;

	/* rewind to the first space character */
	int rewind_steps = 0;

	while (rewind < max_rewind && (autolink_rewind_unless_isspace(data, -rewind - 1, max_rewind, &rewind_steps)))
		rewind = rewind + rewind_steps;

	if (!sd_autolink_issafe(data - rewind, size + rewind))
		return 0;

	link_end = strlen("://");

	domain_len = check_domain(
		data + link_end,
		size - link_end,
		flags & SD_AUTOLINK_SHORT_DOMAINS);

	if (domain_len == 0)
		return 0;

	link_end += domain_len;
	while (link_end < size && !rinku_isspace(data[link_end]))
		link_end++;

	link_end = autolink_delim(data, link_end, max_rewind, size);

	if (link_end == 0)
		return 0;

	bufput(link, data - rewind, link_end + rewind);
	*rewind_p = rewind;

	return link_end;
}
