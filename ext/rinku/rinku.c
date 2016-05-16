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

#define RUBY_EXPORT __attribute__ ((visibility ("default")))

#ifdef HAVE_RUBY_ENCODING_H
#include <ruby/encoding.h>
#else
#define rb_enc_copy(dst, src)
#endif

#include "autolink.h"
#include "buffer.h"
#include "utf8.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

static VALUE rb_mRinku;

typedef enum {
	HTML_TAG_NONE = 0,
	HTML_TAG_OPEN,
	HTML_TAG_CLOSE,
} html_tag;

typedef enum {
	AUTOLINK_URLS = (1 << 0),
	AUTOLINK_EMAILS = (1 << 1),
	AUTOLINK_ALL = AUTOLINK_URLS|AUTOLINK_EMAILS
} autolink_mode;

typedef size_t (*autolink_parse_cb)(
	size_t *rewind, struct buf *, uint8_t *, size_t, size_t, unsigned int);

typedef enum {
	AUTOLINK_ACTION_NONE = 0,
	AUTOLINK_ACTION_WWW,
	AUTOLINK_ACTION_EMAIL,
	AUTOLINK_ACTION_URL,
	AUTOLINK_ACTION_SKIP_TAG
} autolink_action;

static autolink_parse_cb g_callbacks[] = {
	NULL,
	sd_autolink__www,	/* 1 */
	sd_autolink__email,/* 2 */
	sd_autolink__url,	/* 3 */
};

static const char *g_hrefs[] = {
	NULL,
	"<a href=\"http://",
	"<a href=\"mailto:",
	"<a href=\"",
};

static void
autolink__print(struct buf *ob, const struct buf *link, void *payload)
{
	bufput(ob, link->data, link->size);
}

/*
 * Rinku assumes valid HTML encoding for all input, but there's still
 * the case where a link can contain a double quote `"` that allows XSS.
 *
 * We need to properly escape the character we use for the `href` attribute
 * declaration
 */
static void print_link(struct buf *ob, const char *link, size_t size)
{
	size_t i = 0, org;

	while (i < size) {
		org = i;

		while (i < size && link[i] != '"')
			i++;

		if (i > org)
			bufput(ob, link + org, i - org);

		if (i >= size)
			break;

		BUFPUTSL(ob, "&quot;");
		i++;
	}
}

/* From sundown/html/html.c */
static int
html_is_tag(const uint8_t *tag_data, size_t tag_size, const char *tagname)
{
	size_t i;
	int closed = 0;

	if (tag_size < 3 || tag_data[0] != '<')
		return HTML_TAG_NONE;

	i = 1;

	if (tag_data[i] == '/') {
		closed = 1;
		i++;
	}

	for (; i < tag_size; ++i, ++tagname) {
		if (*tagname == 0)
			break;

		if (tag_data[i] != *tagname)
			return HTML_TAG_NONE;
	}

	if (i == tag_size)
		return HTML_TAG_NONE;

	if (rinku_isspace(tag_data[i]) || tag_data[i] == '>')
		return closed ? HTML_TAG_CLOSE : HTML_TAG_OPEN;

	return HTML_TAG_NONE;
}

static size_t
autolink__skip_tag(
	struct buf *ob,
	const uint8_t *text,
	size_t size,
	const char **skip_tags)
{
	size_t i = 0;

	while (i < size && text[i] != '>')
		i++;

	while (*skip_tags != NULL) {
		if (html_is_tag(text, size, *skip_tags) == HTML_TAG_OPEN)
			break;

		skip_tags++;
	}

	if (*skip_tags != NULL) {
		for (;;) {
			while (i < size && text[i] != '<')
				i++;

			if (i == size)
				break;

			if (html_is_tag(text + i, size - i, *skip_tags) == HTML_TAG_CLOSE)
				break;

			i++;
		}

		while (i < size && text[i] != '>')
			i++;
	}

//	bufput(ob, text, i + 1);
	return i;
}

int
rinku_autolink(
	struct buf *ob,
	const uint8_t *text,
	size_t size,
	autolink_mode mode,
	unsigned int flags,
	const char *link_attr,
	const char **skip_tags,
	void (*link_text_cb)(struct buf *ob, const struct buf *link, void *payload),
	void *payload)
{
	size_t i, end, last_link_found = 0;
	struct buf *link = bufnew(16);
	char active_chars[256] = {0};
	int link_count = 0;

	if (!text || size == 0)
		return 0;

	/* assign special actions to certain characters in active_chars */
	active_chars['<'] = AUTOLINK_ACTION_SKIP_TAG;

	if (mode & AUTOLINK_EMAILS)
		active_chars['@'] = AUTOLINK_ACTION_EMAIL;

	if (mode & AUTOLINK_URLS) {
		active_chars['w'] = AUTOLINK_ACTION_WWW;
		active_chars['W'] = AUTOLINK_ACTION_WWW;
		active_chars[':'] = AUTOLINK_ACTION_URL;
	}

	if (link_text_cb == NULL)
		link_text_cb = &autolink__print;

	if (link_attr != NULL) {
		while (rinku_isspace(*link_attr))
			link_attr++;
	}

	bufgrow(ob, size);

	i = end = 0;

	while (i < size) {
		size_t rewind, link_end;
		char action = 0;

		/* This part of the code doesn't need to be UTF-8 aware, since we only
		 * really are detecting for the "special" characters what we need to
		 * react to - W, w, @, or :
		 */

		/* count to the position of first action character */
		while (end < size && (action = active_chars[text[end]]) == 0)
			end++;

		/* if we've reached the end of the buffer AND found a link, append
		 * text + i to the end of it
		 */
		if (end == size) {
			/* only write to a new output buffer if we've created a link, and
			 * if we didn't find one, we will return the empty buffer
			 * unmodified. This will save us an allocation on the Ruby heap
			 */
			if (link_count > 0)
				bufput(ob, text + i, end - i);
			break;
		}

		/* if we've found a '<', skip forward to the '>' and continue */
		if (action == AUTOLINK_ACTION_SKIP_TAG) {
			end += autolink__skip_tag(ob,
				text + end, size - end, skip_tags);

			continue;
		}

		link->size = 0;

		/* depending on action, call one of the autolink callbacks */
		link_end = g_callbacks[(int)action](
			&rewind, link, (uint8_t *)text + end,
			end - last_link_found,
			size - end, flags);

		/* print the link */
		if (link_end > 0) {
			bufput(ob, text + i, end - i - rewind);

			bufputs(ob, g_hrefs[(int)action]);
			print_link(ob, link->data, link->size);

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
			last_link_found = end = i;
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

const char **rinku_load_tags(VALUE rb_skip)
{
	const char **skip_tags;
	size_t i, count;

	Check_Type(rb_skip, T_ARRAY);

	count = RARRAY_LEN(rb_skip);
	skip_tags = xmalloc(sizeof(void *) * (count + 1));

	for (i = 0; i < count; ++i) {
		VALUE tag = rb_ary_entry(rb_skip, i);
		Check_Type(tag, T_STRING);
		skip_tags[i] = StringValueCStr(tag);
	}

	skip_tags[count] = NULL;
	return skip_tags;
}

/*
 * Document-method: auto_link
 *
 * call-seq:
 *  auto_link(text, mode=:all, link_attr=nil, skip_tags=nil, flags=0)
 *  auto_link(text, mode=:all, link_attr=nil, skip_tags=nil, flags=0) { |link_text| ... }
 *
 * Parses a block of text looking for "safe" urls or email addresses,
 * and turns them into HTML links with the given attributes.
 *
 * NOTE: The block of text may or may not be HTML; if the text is HTML,
 * Rinku will skip the relevant tags to prevent double-linking and linking
 * inside `pre` blocks by default.
 *
 * NOTE: If the input text is HTML, it's expected to be already escaped.
 * Rinku will perform no escaping.
 *
 * NOTE: Currently the follow protocols are considered safe and are the
 * only ones that will be autolinked.
 *
 *     http:// https:// ftp:// mailto://
 *
 * Email addresses are also autolinked by default. URLs without a protocol
 * specifier but starting with 'www.' will also be autolinked, defaulting to
 * the 'http://' protocol.
 *
 * -   `text` is a string in plain text or HTML markup. If the string is formatted in
 * HTML, Rinku is smart enough to skip the links that are already enclosed in `<a>`
 * tags.`
 *
 * -   `mode` is a symbol, either `:all`, `:urls` or `:email_addresses`,
 * which specifies which kind of links will be auto-linked.
 *
 * -   `link_attr` is a string containing the link attributes for each link that
 * will be generated. These attributes are not sanitized and will be include as-is
 * in each generated link, e.g.
 *
 *      ~~~~~ruby
 *      auto_link('http://www.pokemon.com', :all, 'target="_blank"')
 *      # => '<a href="http://www.pokemon.com" target="_blank">http://www.pokemon.com</a>'
 *      ~~~~~
 *
 *     This string can be autogenerated from a hash using the Rails `tag_options` helper.
 *
 * -   `skip_tags` is a list of strings with the names of HTML tags that will be skipped
 * when autolinking. If `nil`, this defaults to the value of the global `Rinku.skip_tags`,
 * which is initially `["a", "pre", "code", "kbd", "script"]`.
 *
 * -   `flag` is an optional boolean value specifying whether to recognize
 * 'http://foo' as a valid domain, or require at least one '.'. It defaults to false.
 *
 * -   `&block` is an optional block argument. If a block is passed, it will
 * be yielded for each found link in the text, and its return value will be used instead
 * of the name of the link. E.g.
 *
 *     ~~~~~ruby
 *     auto_link('Check it out at http://www.pokemon.com') do |url|
 *       "THE POKEMAN WEBSITEZ"
 *     end
 *     # => 'Check it out at <a href="http://www.pokemon.com">THE POKEMAN WEBSITEZ</a>'
 *     ~~~~~~
 */
static VALUE
rb_rinku_autolink(int argc, VALUE *argv, VALUE self)
{
	static const char *SKIP_TAGS[] = {"a", "pre", "code", "kbd", "script", NULL};

	VALUE result, rb_text, rb_mode, rb_html, rb_skip, rb_flags, rb_block;
	struct buf *output_buf;
	int link_mode, count;
	unsigned int link_flags = 0;
	const char *link_attr = NULL;
	const char **skip_tags = NULL;
	ID mode_sym;

	rb_scan_args(argc, argv, "14&", &rb_text, &rb_mode,
		&rb_html, &rb_skip, &rb_flags, &rb_block);

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

	if (NIL_P(rb_skip))
		rb_skip = rb_iv_get(self, "@skip_tags");

	if (NIL_P(rb_skip)) {
		skip_tags = SKIP_TAGS;
	} else {
		skip_tags = rinku_load_tags(rb_skip);
	}

	if (!NIL_P(rb_flags)) {
		Check_Type(rb_flags, T_FIXNUM);
		link_flags = FIX2INT(rb_flags);
	}

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
		RSTRING_PTR(rb_text),
		RSTRING_LEN(rb_text),
		link_mode,
		link_flags,
		link_attr,
		skip_tags,
		RTEST(rb_block) ? &autolink_callback : NULL,
		(void*)rb_block);

	if (count == 0)
		result = rb_text;
	else {
		result = rb_str_new(output_buf->data, output_buf->size);
		rb_enc_copy(result, rb_text);
	}

	if (skip_tags != SKIP_TAGS)
		xfree(skip_tags);

	bufrelease(output_buf);
	return result;
}

void RUBY_EXPORT Init_rinku()
{
	rb_mRinku = rb_define_module("Rinku");
	rb_define_method(rb_mRinku, "auto_link", rb_rinku_autolink, -1);
	rb_define_const(rb_mRinku, "AUTOLINK_SHORT_DOMAINS", INT2FIX(SD_AUTOLINK_SHORT_DOMAINS));
}
