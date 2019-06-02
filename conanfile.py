from conans import ConanFile, CMake


class Libgit2Conan(ConanFile):
    name = "libgit2"
    version = "0.28.0"
    license = "GPL v2"
    url = "https://github.com/libgit2/libgit2"
    description = """`libgit2` is a portable, pure C implementation of the Git core methods
provided as a linkable library with a solid API, allowing to build Git
functionality into your application."""
    topics = ("<Put some tag here>", "<here>", "<and here>")

    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False]}
    default_options = "shared=False"

    generators = "cmake"
    # exports_sources = "CMakeLists.txt", "src/*", "include/*", "cmake/*", "deps/*", \
    #     "tests/*", "libgit2.pc.in", "libgit2_clar.supp", "fuzzers/*"
    exports_sources = "*"
    no_copy_source = False

    scm = {
        "type": "git",
        "subfolder": "hello",
        "url": "auto",
        "revision": "auto"
    }

    def build(self):
        cmake = CMake(self)
        cmake.configure(source_folder=".")
        cmake.build()
        self.run("bin/libgit2_clar")

    def package(self):
        self.copy("*.h", dst="include", src="src")
        self.copy("*.lib", dst="lib", keep_path=False)
        self.copy("*.dll", dst="bin", keep_path=False)
        self.copy("*.dylib*", dst="lib", keep_path=False)
        self.copy("*.so", dst="lib", keep_path=False)
        self.copy("*.a", dst="lib", keep_path=False)

    def package_info(self):
        self.cpp_info.libs = ["libgit2"]
