#include <stdio.h>
#include "ruby.h"

#include "autolink.h"
#include "buffer.h"

static VALUE rb_cRinku;

extern void
upshtml_autolink(
	struct buf *ob,
	struct buf *text,
	unsigned int flags,
	const char *link_attr,
	void (*link_text_cb)(struct buf *ob, const struct buf *link, void *payload),
	void *payload);

static void
autolink_callback(struct buf *link_text, const struct buf *link, void *block)
{
	VALUE rb_link, rb_link_text;
	rb_link = rb_str_new(link->data, link->size);
	rb_link_text = rb_funcall((VALUE)block, rb_intern("call"), 1, rb_link);
	Check_Type(rb_link_text, T_STRING);
	bufput(link_text, RSTRING_PTR(rb_link_text), RSTRING_LEN(rb_link_text));
}

static VALUE
rb_rinku_autolink(int argc, VALUE *argv, VALUE self)
{
	VALUE result, rb_text, rb_mode, rb_html, rb_block;
	struct buf input_buf = {0, 0, 0, 0, 0}, *output_buf;
	int link_mode;
	const char *link_attr = NULL;
	ID mode_sym;

	rb_scan_args(argc, argv, "3&", &rb_text, &rb_mode, &rb_html, &rb_block);

	Check_Type(rb_text, T_STRING);
	Check_Type(rb_mode, T_SYMBOL);

	if (!NIL_P(rb_html)) {
		Check_Type(rb_html, T_STRING);
		link_attr = RSTRING_PTR(rb_html);
	}

	input_buf.data = RSTRING_PTR(rb_text);
	input_buf.size = RSTRING_LEN(rb_text);
	output_buf = bufnew(128);

	mode_sym = SYM2ID(rb_mode);

	if (mode_sym == rb_intern("all"))
		link_mode = AUTOLINK_ALL;
	else if (mode_sym == rb_intern("email_addresses"))
		link_mode = AUTOLINK_EMAILS;
	else if (mode_sym == rb_intern("urls"))
		link_mode = AUTOLINK_URLS;
	else
		rb_raise(rb_eTypeError,
			"Invalid linking mode (possible values are :all, :urls, :email_addresses)");

	if (RTEST(rb_block))
		upshtml_autolink(output_buf, &input_buf, AUTOLINK_ALL, link_attr, &autolink_callback, (void*)rb_block);
	else
		upshtml_autolink(output_buf, &input_buf, AUTOLINK_ALL, link_attr, NULL, NULL);

	result = rb_str_new(output_buf->data, output_buf->size);
	bufrelease(output_buf);

	/* force the input encoding */
	if (rb_respond_to(rb_text, rb_intern("encoding"))) {
		VALUE encoding = rb_funcall(rb_text, rb_intern("encoding"), 0);
		rb_funcall(result, rb_intern("force_encoding"), 1, encoding);
	}

	return result;
}

void Init_rinku()
{
    rb_cRinku = rb_define_class("Rinku", rb_cObject);
	rb_define_singleton_method(rb_cRinku, "_auto_link", rb_rinku_autolink, -1);
}


