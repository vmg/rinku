Gem::Specification.new do |s|
  s.name = 'rinku'
  s.version = '1.3.0'
  s.summary = "Mostly autolinking"
  s.description = <<-EOF
    A fast and very smart autolinking library that
    acts as a drop-in replacement for Rails `auto_link`
  EOF
  s.email = 'vicent@github.com'
  s.homepage = 'http://github.com/tanoku/rinku'
  s.authors = ["Vicent Martí"]
  # = MANIFEST =
  s.files = %w[
    COPYING
    README.markdown
    Rakefile
    ext/rinku/rinku.c
    ext/rinku/autolink.c
    ext/rinku/autolink.h
    ext/rinku/buffer.c
    ext/rinku/buffer.h
    ext/rinku/extconf.rb
    ext/rinku/houdini.h
    ext/rinku/houdini_href_e.c
    ext/rinku/houdini_html_e.c
    lib/rinku.rb
    lib/rails_rinku.rb
    rinku.gemspec
    test/autolink_test.rb
  ]
  # = MANIFEST =
  s.test_files = ["test/autolink_test.rb"]
  s.extra_rdoc_files = ["COPYING"]
  s.extensions = ["ext/rinku/extconf.rb"]
  s.require_paths = ["lib"]
end
