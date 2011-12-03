require 'date'
require 'rake/clean'
require 'rake/extensiontask'
require 'digest/md5'

task :default => :test

# ==========================================================
# Ruby Extension
# ==========================================================

Rake::ExtensionTask.new('rinku')

# ==========================================================
# Testing
# ==========================================================

require 'rake/testtask'
Rake::TestTask.new('test') do |t|
  t.test_files = FileList['test/*_test.rb']
  t.ruby_opts += ['-rubygems'] if defined? Gem
end
task 'test' => [:compile]

# PACKAGING =================================================================

require 'rubygems'
$spec = eval(File.read('rinku.gemspec'))

def package(ext='')
  "pkg/rinku-#{$spec.version}" + ext
end

desc 'Build packages'
task :package => package('.gem')

desc 'Build and install as local gem'
task :install => package('.gem') do
  sh "gem install #{package('.gem')}"
end

desc 'Update the gemspec'
task :update_gem => file('rinku.gemspec')

directory 'pkg/'

file package('.gem') => %w[pkg/ rinku.gemspec] + $spec.files do |f|
  sh "gem build rinku.gemspec"
  mv File.basename(f.name), f.name
end

# GEMSPEC HELPERS ==========================================================
task :gather => 'sundown/src/autolink.h' do |t|
  files =
    FileList[
      'sundown/src/{buffer,autolink}.h',
      'sundown/src/{buffer,autolink}.c',
    ]
  cp files, 'ext/rinku/',
    :preserve => true,
    :verbose => true
end


file 'sundown/src/autolink.h' do |t|
  abort "The Sundown submodule is required."
end
