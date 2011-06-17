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
#define RSTRING_NOT_MODIFIED

#include <stdio.h>
#include "ruby.h"

#ifdef HAVE_RUBY_ENCODING_H
#include <ruby/encoding.h>
#else
#define rb_enc_copy(dst, src)
#endif

#include "autolink.h"
#include "buffer.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

static VALUE rb_mRinku;

typedef enum {
	AUTOLINK_URLS = (1 << 0),
	AUTOLINK_EMAILS = (1 << 1),
	AUTOLINK_ALL = AUTOLINK_URLS|AUTOLINK_EMAILS
} autolink_mode;

typedef size_t (*autolink_parse_cb)(size_t *rewind, struct buf *, char *, size_t, size_t);

typedef enum {
	AUTOLINK_ACTION_NONE = 0,
	AUTOLINK_ACTION_WWW,
	AUTOLINK_ACTION_EMAIL,
	AUTOLINK_ACTION_URL,
	AUTOLINK_ACTION_SKIP_TAG
} autolink_action;

static autolink_parse_cb g_callbacks[] = {
	NULL,
	ups_autolink__www,	/* 1 */
	ups_autolink__email,/* 2 */
	ups_autolink__url,	/* 3 */
};

static const char *g_hrefs[] = {
	NULL,
	"<a href=\"http://",
	"<a href=\"mailto:",
	"<a href=\"",
};

static void
autolink_escape_cb(struct buf *ob, const struct buf *text, void *unused)
{
	size_t  i = 0, org;

	while (i < text->size) {
		org = i;

		while (i < text->size &&
			text->data[i] != '<' &&
			text->data[i] != '>' &&
			text->data[i] != '&' &&
			text->data[i] != '"')
			i++;

		if (i > org)
			bufput(ob, text->data + org, i - org);

		if (i >= text->size)
			break;

		switch (text->data[i]) {
			case '<': BUFPUTSL(ob, "&lt;"); break;
			case '>': BUFPUTSL(ob, "&gt;"); break;
			case '&': BUFPUTSL(ob, "&amp;"); break;
			case '"': BUFPUTSL(ob, "&quot;"); break;
			default: bufputc(ob, text->data[i]); break;
		}

		i++;
	}
}

static inline int
is_closing_a(const char *tag, size_t size)
{
	size_t i;

	if (tag[0] != '<' || size < STRLEN("</a>") || tag[1] != '/')
		return 0;

	i = 2;

	while (i < size && isspace(tag[i]))
		i++;

	if (i == size || tag[i] != 'a')
		return 0;

	i++;

	while (i < size && isspace(tag[i]))
		i++;

	if (i == size || tag[i] != '>')
		return 0;

	return i;
}

static size_t
autolink__skip_tag(struct buf *ob, char *text, size_t size)
{
	size_t i = 0;

	while (i < size && text[i] != '>')
		i++;

	if (size > 3 && text[1] == 'a' && isspace(text[2])) {
		while (i < size) {
			size_t tag_len = is_closing_a(text + i, size - i);
			if (tag_len) {
				i += tag_len;
				break;
			}
			i++;
		}
	}

	return i + 1;
}

int
rinku_autolink(
	struct buf *ob,
	struct buf *text,
	unsigned int flags,
	const char *link_attr,
	void (*link_text_cb)(struct buf *ob, const struct buf *link, void *payload),
	void *payload)
{
	size_t i, end;
	struct buf *link = bufnew(16);
	char active_chars[256];
	void (*link_url_cb)(struct buf *, const struct buf *, void *);
	int link_count = 0;

	if (!text || text->size == 0)
		return;

	memset(active_chars, 0x0, sizeof(active_chars));

	active_chars['<'] = AUTOLINK_ACTION_SKIP_TAG;

	if (flags & AUTOLINK_EMAILS)
		active_chars['@'] = AUTOLINK_ACTION_EMAIL;

	if (flags & AUTOLINK_URLS) {
		active_chars['w'] = AUTOLINK_ACTION_WWW;
		active_chars['W'] = AUTOLINK_ACTION_WWW;
		active_chars[':'] = AUTOLINK_ACTION_URL;
	}

	if (link_text_cb == NULL)
		link_text_cb = &autolink_escape_cb;

	if (link_attr != NULL) {
		while (isspace(*link_attr))
			link_attr++;
	}

	bufgrow(ob, text->size);

	i = end = 0;

	while (i < text->size) {
		size_t rewind, link_end;
		char action;

		while (end < text->size && (action = active_chars[text->data[end] & 0xFF]) == 0)
			end++;

		if (end == text->size) {
			if (link_count > 0)
				bufput(ob, text->data + i, end - i);
			break;
		}

		if (action == AUTOLINK_ACTION_SKIP_TAG) {
			end += autolink__skip_tag(ob, text->data + end, text->size - end);
			continue;
		}

		link->size = 0;
		link_end = g_callbacks[(int)action](&rewind, link, text->data + end, end, text->size - end);

		/* print the link */
		if (link_end > 0) {
			bufput(ob, text->data + i, end - i - rewind);

			bufputs(ob, g_hrefs[(int)action]);
			autolink_escape_cb(ob, link, NULL);

			if (link_attr) {
				BUFPUTSL(ob, "\" ");
				bufputs(ob, link_attr);
				bufputc(ob, '>');
			} else {
				BUFPUTSL(ob, "\">");
			}

			link_text_cb(ob, link, payload);
			BUFPUTSL(ob, "</a>");

			link_count++;
			i = end + link_end;
			end = i;
		} else {
			end = end + 1;
		}
	}

	bufrelease(link);
	return link_count;
}


/**
 * Ruby code
 */
static void
autolink_callback(struct buf *link_text, const struct buf *link, void *block)
{
	VALUE rb_link, rb_link_text;
	rb_link = rb_str_new(link->data, link->size);
	rb_link_text = rb_funcall((VALUE)block, rb_intern("call"), 1, rb_link);
	Check_Type(rb_link_text, T_STRING);
	bufput(link_text, RSTRING_PTR(rb_link_text), RSTRING_LEN(rb_link_text));
}

/*
 * Document-method: auto_link
 *
 * call-seq:
 *  auto_link(text, mode=:all, link_attr=nil)
 *  auto_link(text, mode=:all, link_attr=nil) { |link_text| ... }
 *
 * Parses a block of text looking for "safe" urls or email addresses,
 * and turns them into HTML links with the given attributes.
 *
 * NOTE: Currently the follow protocols are considered safe and are the
 * only ones that will be autolinked.
 *
 *		http:// https:// ftp:// mailto://
 *
 * Email addresses are also autolinked by default. URLs without a protocol
 * specifier but starting with 'www.' will also be autolinked, defaulting to
 * the 'http://' protocol.
 *
 * +text+ is a string in plain text or HTML markup. If the string is formatted in
 * HTML, Rinku is smart enough to skip the links that are already enclosed in <a>
 * tags.
 *
 * +mode+ is a symbol, either :all, :urls or :email_addresses, which specifies which
 * kind of links will be auto-linked.
 *
 * +link_attr+ is a string containing the link attributes for each link that
 * will be generated. These attributes are not sanitized and will be include as-is
 * in each generated link, e.g.
 *
 *	auto_link('http://www.pokemon.com', :all, 'target="_blank"')
 *	# => '<a href="http://www.pokemon.com" target="_blank">http://www.pokemon.com</a>'
 *
 * This string can be autogenerated from a hash using the Rails `tag_options` helper.
 *
 * +block+ The method takes an optional block argument. If a block is passed, it will
 * be yielded for each found link in the text, and its return value will be used instead
 * of the name of the link. E.g.
 *
 *	auto_link('Check it out at http://www.pokemon.com') do |url|
 *		"THE POKEMAN WEBSITEZ"
 *	end
 *	# => 'Check it out at <a href="http://www.pokemon.com">THE POKEMAN WEBSITEZ</a>'
 */
static VALUE
rb_rinku_autolink(int argc, VALUE *argv, VALUE self)
{
	VALUE result, rb_text, rb_mode, rb_html, rb_block;
	struct buf input_buf = {0, 0, 0, 0, 0}, *output_buf;
	int link_mode, count;
	const char *link_attr = NULL;
	ID mode_sym;

	rb_scan_args(argc, argv, "12&", &rb_text, &rb_mode, &rb_html, &rb_block);

	Check_Type(rb_text, T_STRING);

	if (!NIL_P(rb_mode)) {
		Check_Type(rb_mode, T_SYMBOL);
		mode_sym = SYM2ID(rb_mode);
	} else {
		mode_sym = rb_intern("all");
	}

	if (!NIL_P(rb_html)) {
		Check_Type(rb_html, T_STRING);
		link_attr = RSTRING_PTR(rb_html);
	}

	input_buf.data = RSTRING_PTR(rb_text);
	input_buf.size = RSTRING_LEN(rb_text);
	output_buf = bufnew(32);

	if (mode_sym == rb_intern("all"))
		link_mode = AUTOLINK_ALL;
	else if (mode_sym == rb_intern("email_addresses"))
		link_mode = AUTOLINK_EMAILS;
	else if (mode_sym == rb_intern("urls"))
		link_mode = AUTOLINK_URLS;
	else
		rb_raise(rb_eTypeError,
			"Invalid linking mode (possible values are :all, :urls, :email_addresses)");

	count = rinku_autolink(
		output_buf,
		&input_buf,
		link_mode,
		link_attr,
		RTEST(rb_block) ? &autolink_callback : NULL,
		(void*)rb_block);

	if (count == 0)
		result = rb_text;
	else {
		result = rb_str_new(output_buf->data, output_buf->size);
		rb_enc_copy(result, rb_text);
	}

	bufrelease(output_buf);
	return result;
}

void Init_rinku()
{
	rb_mRinku = rb_define_module("Rinku");
	rb_define_method(rb_mRinku, "auto_link", rb_rinku_autolink, -1);
}


