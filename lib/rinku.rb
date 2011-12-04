module Rinku
  VERSION = "1.2.2"
  @@skip_tags = nil
  attr_accessor :skip_tags
  extend self
end

require 'rinku.so'
