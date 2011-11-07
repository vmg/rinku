require 'rinku'

module RailsRinku
  def rinku_auto_link(text, *args, &block)
    return '' if text.blank?

    options = args.size == 2 ? {} : args.extract_options!
    unless args.empty?
      options[:link] = args[0] || :all
      options[:html] = args[1] || {}
    end
    options.reverse_merge!(:link => :all, :html => {})

    Rinku.auto_link(text, options[:link], tag_options(options[:html]), &block)
  end
end

module ActionView::Helpers::TextHelper
 include RailsRinku
 alias_method :auto_link, :rinku_auto_link
end
