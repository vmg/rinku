module Rinku
  VERSION = "2.0.1"

  class << self
    attr_accessor :skip_tags
  end

  self.skip_tags = nil
end

require 'rinku.so'
