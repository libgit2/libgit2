// swift-tools-version:5.3
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription

let libgit2OriginPath = "."
let httpClientPath = "\(libgit2OriginPath)/deps/http-parser"
let ntlmClientPath = "\(libgit2OriginPath)/deps/ntlmclient"

let package = Package(
    name: "SwiftPackage",
    products: [
        // Products define the executables and libraries a package produces, and make them visible to other packages.
        .library(name: "Libgit2Origin",
                 targets: ["Libgit2Origin"]
        ),
        .library(name: "http-client",
                 targets: ["http-client"]
        ),
        .library(name: "ntlmclient",
                 targets: ["ntlmclient"]
        ),
    ],
    dependencies: [
        // Dependencies declare other packages that this package depends on.
        // .package(url: /* package url */, from: "1.0.0"),
    ],
    targets: [
        // Targets are the basic building blocks of a package. A target can define a module or a test suite.
        // Targets can depend on other targets in this package, and on products in packages this package depends on.
        .target(name: "Libgit2Origin",
                dependencies: [
                    "libssh2", "libssl", "libcrypto", "http-client", "ntlmclient"
                ],
                path: libgit2OriginPath,
                exclude: [
                    // ./
                    "xcode",
                    "ci",
                    "cmake",
                    "deps",
                    "docs",
                    "examples",
                    "fuzzers",
                    "script",
                    "tests",
                    "api.docurium",
                    "AUTHORS",
                    "CMakeLists.txt",
                    "COPYING",
                    "git.git-authors",
                    "package.json",
                    "README.md",
                    "SECURITY.md",
                    "update-xcode.sh",
                    // ./src/
                    "src/CMakeLists.txt",
                    "src/features.h.in",
                    // ./src/hash/sha1
                    "src/hash/sha1/common_crypto.h",
                    "src/hash/sha1/common_crypto.c",
                    "src/hash/sha1/generic.h",
                    "src/hash/sha1/generic.c",
                    "src/hash/sha1/mbedtls.h",
                    "src/hash/sha1/mbedtls.c",
                    "src/hash/sha1/openssl.h",
                    "src/hash/sha1/openssl.c",
                    "src/hash/sha1/win32.h",
                    "src/hash/sha1/win32.c",
                    // ./src/hash/
                    "src/hash/sha1.h",
                    
                    // ./src/win32
                    "src/win32",
                    
                    // ./include/git2/
                    "include/git2/stdint.h",
                    
                ],
                sources: ["src"],
                resources: nil,
                publicHeadersPath: "include",
                cSettings: [
                    .headerSearchPath("include/git2/sys/features.h"),
                    .headerSearchPath("src"),
                    .headerSearchPath("src/allocators"),
                    .headerSearchPath("src/hash"),
                    .headerSearchPath("src/streams"),
                    .headerSearchPath("src/transports"),
                    .headerSearchPath("src/unix"),
                    .headerSearchPath("src/xdiff"),
                    .headerSearchPath("include"),
                    .headerSearchPath("include/git2"),
                    .headerSearchPath("include/git2/sys"),
                    .headerSearchPath("deps/http-parser"),
                    .headerSearchPath("deps/ntlmclient"),
                    .define("HAVE_QSORT_R_BSD"),
                    .define("_FILE_OFFSET_BITS", to: "64"),
                    .define("SHA1DC_NO_STANDARD_INCLUDES", to: "1"),
                    .define("SHA1DC_CUSTOM_INCLUDE_SHA1_C", to: "\"common.h\""),
                    .define("SHA1DC_CUSTOM_INCLUDE_UBC_CHECK_C", to: "\"common.h\""),
                                        
                    .define("USE_HEADERMAP", to: "NO"),
                    .define("GCC_INLINES_ARE_PRIVATE_EXTERN", to: "NO"),
                    .define("GCC_SYMBOLS_PRIVATE_EXTERN", to: "NO"),
                    .unsafeFlags([
                        "-D_GNU_SOURCE",
                        "-Werror",
                        "-Wno-error",
                        "-Wall",
                        "-Wextra",
                        "-fvisibility=hidden",
                        "-fPIC",
                        "-Wdocumentation",
                        "-Wno-documentation-deprecated-sync",
                        "-Wno-missing-field-initializers",
                        "-Wstrict-aliasing",
                        "-Wstrict-prototypes",
                        "-Wdeclaration-after-statement",
                        "-Wshift-count-overflow",
                        "-Wunused-const-variable",
                        "-Wunused-function",
                        "-Wint-conversion",
                        "-Wformat",
                        "-Wformat-security",
                        "-Wmissing-declarations",
                        "-D_DEBUG",
                        "-std=gnu90",
                        
//                        "-headerpad_max_install_names",
                    ])
                ],
                cxxSettings: nil,
                swiftSettings: nil,
                linkerSettings: [
                    .linkedFramework("CoreFoundation"),
                    .linkedFramework("Security"),
                    .linkedLibrary("z"),
                    .linkedLibrary("iconv"),                    
                ]
        ),
        
        .target(name: "http-client",
                dependencies: [],
                path: httpClientPath,
                exclude: [
                    "CMakeLists.txt",
                    "COPYING"
                ],
                sources: nil,
                resources: nil,
                publicHeadersPath: ".",
                cSettings: [],
                cxxSettings: nil,
                swiftSettings: nil,
                linkerSettings: []
        ),
        
        .target(name: "ntlmclient",
                dependencies: ["libssh2"],
                path: ntlmClientPath,
                exclude: [
                    "crypt_openssl.h",
                    "crypt_openssl.c",
                    "crypt_mbedtls.h",
                    "crypt_mbedtls.c",
                    "unicode_builtin.c",
                    "CMakeLists.txt",
                ],
                sources: [
                    "ntlm.c",
                    "unicode_iconv.c",
                    "util.c",
                    "crypt_commoncrypto.c"
                ],
                resources: nil,
                publicHeadersPath: ".",
                cSettings: [
                    .define("NTLM_STATIC", to: "1"),
                    .define("CRYPT_COMMONCRYPTO")
                ],
                cxxSettings: nil,
                swiftSettings: nil,
                linkerSettings: []
        ),
        
        .binaryTarget(name: "libssh2", path: "libssh2.xcframework"),
        .binaryTarget(name: "libssl", path: "libssl.xcframework"),
        .binaryTarget(name: "libsqlite3", path: "libsqlite3.xcframework"),
        .binaryTarget(name: "libcrypto", path: "libcrypto.xcframework"),
    ]
)

//let settings = package.targets.first?.cSettings
//print("Package: \(String(describing: settings))")
