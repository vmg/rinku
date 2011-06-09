Rinku does linking
==================

Rinku is a Ruby library that does autolinking.
It parses text and turns anything that remotely resembles a link into an HTML link,
just like the Ruby on Rails `auto_link` method -- but it's about 20 times faster,
because it's written in C, and it's about 20 times smarter when linking,
because it does actual parsing instead of RegEx replacements.

Rinku is a Ruby Gem 
-------------------

Rinku is available as a Ruby gem:

    $ [sudo] gem install rinku

The Rinku source is available at GitHub:

    $ git clone git://github.com/tanoku/rinku.git

Rinku is a drop-in replacement for Rails `auto_link`
----------------------------------------------------

And I'm a lazy bum, so I've copied and pasted the Rails API docs here.
Yes, the Rinku API is 100% compatible.

~~~~~~ruby
# Turns all URLs and e-mail addresses into clickable links. The <tt>:link</tt> option
# will limit what should be linked. You can add HTML attributes to the links using
# <tt>:html</tt>. Possible values for <tt>:link</tt> are <tt>:all</tt> (default),
# <tt>:email_addresses</tt>, and <tt>:urls</tt>. If a block is given, each URL and
# e-mail address is yielded and the result is used as the link text.
#
# ==== Examples
#   auto_link("Go to http://www.rubyonrails.org and say hello to david@loudthinking.com")
#   # => "Go to <a href=\"http://www.rubyonrails.org\">http://www.rubyonrails.org</a> and
#   #     say hello to <a href=\"mailto:david@loudthinking.com\">david@loudthinking.com</a>"
#
#   auto_link("Visit http://www.loudthinking.com/ or e-mail david@loudthinking.com", :link => :urls)
#   # => "Visit <a href=\"http://www.loudthinking.com/\">http://www.loudthinking.com/</a>
#   #     or e-mail david@loudthinking.com"
#
#   auto_link("Visit http://www.loudthinking.com/ or e-mail david@loudthinking.com", :link => :email_addresses)
#   # => "Visit http://www.loudthinking.com/ or e-mail <a href=\"mailto:david@loudthinking.com\">david@loudthinking.com</a>"
#
#   post_body = "Welcome to my new blog at http://www.myblog.com/.  Please e-mail me at me@email.com."
#   auto_link(post_body, :html => { :target => '_blank' }) do |text|
#     truncate(text, :length => 15)
#   end
#   # => "Welcome to my new blog at <a href=\"http://www.myblog.com/\" target=\"_blank\">http://www.m...</a>.
#         Please e-mail me at <a href=\"mailto:me@email.com\">me@email.com</a>."
#
#
# You can still use <tt>auto_link</tt> with the old API that accepts the
# +link+ as its optional second parameter and the +html_options+ hash
# as its optional third parameter:
#   post_body = "Welcome to my new blog at http://www.myblog.com/. Please e-mail me at me@email.com."
#   auto_link(post_body, :urls)     # => Once upon\na time
#   # => "Welcome to my new blog at <a href=\"http://www.myblog.com/\">http://www.myblog.com</a>.
#         Please e-mail me at me@email.com."
#
#   auto_link(post_body, :all, :target => "_blank")     # => Once upon\na time
#   # => "Welcome to my new blog at <a href=\"http://www.myblog.com/\" target=\"_blank\">http://www.myblog.com</a>.
#         Please e-mail me at <a href=\"mailto:me@email.com\">me@email.com</a>."
~~~~~~~~~

Rinku is written by me
----------------------

I am Vicent Marti, and I wrote Rinku.
While Rinku is busy doing autolinks, you should be busy following me on twitter. `@tanoku`. Do it.

Rinku has an awesome license
----------------------------

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

