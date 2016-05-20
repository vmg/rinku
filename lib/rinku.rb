module Rinku
  VERSION = "1.7.3"

  class << self
    attr_accessor :skip_tags
  end

  self.skip_tags = nil
end

require 'rinku.so'
