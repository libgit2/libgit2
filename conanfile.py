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

    requires = "OpenSSL/1.1.1@conan/stable"

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
        args = []

        if self.settings.os == "Windows":
            if self.options.shared == True:
                args.append("STATIC_CRT=OFF")  # default is ON
            if self.settings.build_type == "Debug":
                args.append("MSVC_CRTDBG=ON")  # default is OFF

        cmake.configure(args=args, source_folder=".")
        cmake.build()
        self.run("bin/libgit2_clar")

    def package(self):
        self.copy("*.h",        dst="include",  src="include")
        self.copy("*.lib",      dst="lib",      keep_path=False)
        self.copy("*.dll",      dst="bin",      keep_path=False)
        self.copy("*.dylib*",   dst="lib",      keep_path=False)
        self.copy("*.so",       dst="lib",      keep_path=False)
        self.copy("*.a",        dst="lib",      keep_path=False)

    def package_info(self):
        self.cpp_info.libs = ["git2", "pthread", "rt"]
