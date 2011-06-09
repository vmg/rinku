require 'set'
require 'cgi'

class Rinku
  def self.auto_link(text, *args, &block)
    return '' if text.strip.empty?

    options = args.size == 2 ? {} : (args.last.is_a?(::Hash) ? args.pop : {})
    unless args.empty?
      options[:link] = args[0] || :all
      options[:html] = args[1] || {}
    end

    _auto_link(text, options[:link] || :all, tag_options(options[:html] || {}), &block)
  end

  private
    BOOLEAN_ATTRIBUTES = %w(disabled readonly multiple checked autobuffer
                         autoplay controls loop selected hidden scoped async
                         defer reversed ismap seemless muted required
                         autofocus novalidate formnovalidate open).to_set

    def self.tag_options(options, escape = true)
      unless options.empty?
        attrs = []
        options.each_pair do |key, value|
          key = key.to_s
          if BOOLEAN_ATTRIBUTES.include?(key)
            attrs << %(#{key}="#{key}") if value
          elsif !value.nil?
            final_value = value.is_a?(Array) ? value.join(" ") : value
            final_value = CGI.escapeHTML(final_value) if escape
            attrs << %(#{key}="#{final_value}")
          end
        end
        " #{attrs.sort * ' '}" unless attrs.empty?
      end
    end
end

require 'rinku.so'
