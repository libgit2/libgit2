desc "Build and Run Tests"
task :build do
  `./waf clean`
  `./waf clean-tests`
  `./waf configure`
  `./waf build`
  `./waf test`
end

desc "Build docs"
task :docs do
  puts "Generating Doxygen docs"
  `doxygen api.doxygen`
  `git stash`
  `git checkout gh-pages`
  `cp -Rf apidocs/html/* .`
  `git add .`
  `git commit -am 'generated docs'`
  `git push origin gh-pages`
  `git checkout master`
end
