// swift-tools-version: 5.10
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription

let package = Package(
    name: "libgit2",
    products: [
        // Products define the executables and libraries a package produces, making them visible to other packages.
        .library(
            name: "libgit2",
            targets: ["libgit2"]
        ),
    ],
    targets: [
        // Targets are the basic building blocks of a package, defining a module or a test suite.
        // Targets can depend on other targets in this package and products from dependencies.
        .target(
            name: "libgit2",
            path: ".",
            exclude: [
                "deps/http-parser/CMakeLists.txt",
                "deps/http-parser/COPYING",
                "deps/pcre/CMakeLists.txt",
                "deps/pcre/COPYING",
                "deps/pcre/LICENCE",
                "deps/pcre/cmake/",
                "deps/pcre/config.h.in",
                "deps/xdiff/CMakeLists.txt",
                "deps/zlib/CMakeLists.txt",
                "deps/zlib/LICENSE",
                "src/libgit2/CMakeLists.txt",
                "src/libgit2/experimental.h.in",
                "src/libgit2/git2.rc",
                "src/util/CMakeLists.txt",
                "src/util/git2_features.h.in",
                "src/util/hash/builtin.c",
                "src/util/hash/builtin.h",
                "src/util/hash/collisiondetect.c",
                "src/util/hash/collisiondetect.h",
                "src/util/hash/openssl.c",
                "src/util/hash/openssl.h",
                "src/util/hash/win32.c",
                "src/util/hash/win32.h",
                "src/util/win32",
            ],
            sources: [
                "deps/http-parser",
                "deps/pcre",
                "deps/xdiff",
                "deps/zlib",
                "src/libgit2",
                "src/util",
            ],
            publicHeadersPath: "include",
            cSettings: [
                .unsafeFlags([
                    // Disable -fmodules flag. Clang finds (struct entry) in a different file (`search.h`).
                    "-fno-modules",
                    // Disable warning: "implicit conversion loses integer precision"
                    "-Wno-single-bit-bitfield-constant-conversion",
                    // Disable warning: "a function definition without a prototype is deprecated"
                    "-Wno-deprecated-non-prototype",
                ]),

                .headerSearchPath("deps/http-parser"),
                .headerSearchPath("deps/pcre"),
                .headerSearchPath("deps/xdiff"),
                .headerSearchPath("deps/zlib"),
                .headerSearchPath("src/libgit2"),
                .headerSearchPath("src/util"),

                .define("LIBGIT2_NO_FEATURES_H"),

                // .define("GIT_THREADS", to: "0"),

                .define("GIT_ARCH_64", to: "1"),

                // Git regex configuration
                .define("GIT_REGEX_BUILTIN", to: "1"),
                .define("SUPPORT_PCRE8", to: "1"),
                .define("LINK_SIZE", to: "2"),
                .define("PARENS_NEST_LIMIT", to: "250"),
                .define("MATCH_LIMIT", to: "10000000"),
                .define("MATCH_LIMIT_RECURSION", to: "10000000"),
                .define("NEWLINE", to: "10"), // LF
                .define("NO_RECURSE", to: "1"),
                .define("POSIX_MALLOC_THRESHOLD", to: "10"),
                .define("BSR_ANYCRLF", to: "0"),
                .define("MAX_NAME_SIZE", to: "32"),
                .define("MAX_NAME_COUNT", to: "10000"),

                .define("GIT_QSORT_BSD"),

                // Git SSH transport configuration
                .define("GIT_SSH", to: "1"),
                .define("GIT_SSH_EXEC", to: "1"),
                // .define("GIT_SSH_LIBSSH2", to: "0"),
                // .define("GIT_SSH_LIBSSH2_MEMORY_CREDENTIALS", to: "0"),

                // .define("GIT_NTLM", to: "0"),
                // .define("GIT_GSSAPI", to: "0"),
                // .define("GIT_GSSFRAMEWORK", to: "0"),

                // Git HTTPS transport configuration
                .define("GIT_HTTPS", to: "1"),
                // .define("GIT_OPENSSL", to: "0"),
                // .define("GIT_OPENSSL_DYNAMIC", to: "0"),
                // .define("GIT_SECURE_TRANSPORT", to: "0"),
                // .define("GIT_MBEDTLS", to: "0"),
                // .define("GIT_SCHANNEL", to: "0"),

                // Git cryptography configuration
                .define("GIT_SHA1_COMMON_CRYPTO", to: "1"),
                .define("GIT_SHA256_COMMON_CRYPTO", to: "1"),

                // .define("GIT_RAND_GETENTROPY", to: "0"),
                // .define("GIT_RAND_GETLOADAVG", to: "0"),

                .define("GIT_IO_POLL", to: "1"),
                // .define("GIT_IO_WSAPOLL", to: "0"),
                // .define("GIT_IO_SELECT", to: "0"),
            ]
        ),
    ]
)
