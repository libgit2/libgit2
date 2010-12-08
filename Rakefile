desc "Build docs"
task :docs do
  puts "Generating Doxygen docs"
  `doxygen api.doxygen`
  `git stash`
  `git checkout gh-pages`
  `cp -Rf apidocs/html/* .`
end
#
