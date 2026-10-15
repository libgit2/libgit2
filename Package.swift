// swift-tools-version: 6.2

import PackageDescription

// MARK: - Platform Configuration
//
// libgit2 requires different source files for different platforms. Since SPM's
// exclude/sources parameters cannot use .when(platforms:), platform-specific
// sources are built in conditional targets and all C settings use
// .when(platforms:).
//
// This means:
// - Same-platform builds work correctly (most common case)
// - Cross-compilation selects sources and settings for the build destination
//
// Platform support:
// - Apple (macOS, iOS, tvOS, watchOS, visionOS): Full support
// - Linux: Full support with OpenSSL
// - Android: Full support with OpenSSL
// - Windows: Full support with WinHTTP and CNG
//
// SSH support:
// Default: Use ssh_exec on macOS, Linux, Android (spawns system ssh binary)
// libssh2 trait: Uses bundled libssh2 for SSH on all platforms

let apple: [Platform] = [.iOS, .macOS, .tvOS, .visionOS, .watchOS]

extension String {
  static func name(_ name: String) -> String {
    "libgit2_" + name
  }
}

var traits: Set<Trait> = [.default(enabledTraits: [])]
var targets: [Target] = []
var packageDependencies: [Package.Dependency] = []
var targetDependencies: [Target.Dependency] = []
var sourcePaths: [String] = []
var excludedPaths: [String] = []
var cSettings: [CSetting] = []
var linkerSettings: [LinkerSetting] = []

// MARK: - Core

sourcePaths += [
  "src/libgit2",
]

excludedPaths += [
  // CMake build system files
  "src/libgit2/CMakeLists.txt",
  "src/libgit2/experimental.h.in",
  "src/libgit2/git2.rc",
  "src/libgit2/config.cmake.in",
]

cSettings += [
  .headerSearchPath("src/libgit2"),
  .headerSearchPath("include"),
  .define("LIBGIT2_NO_FEATURES_H"),
  .define("GIT_ARCH_64", to: "1"),
]

// MARK: - util

cSettings += [
  .headerSearchPath("src/util"),
]

targets += [

  .target(
    name: .name("util"),
    path: ".",
    exclude: [
      "src/util/CMakeLists.txt",
      "src/util/git2_features.h.in",
      "src/util/hash",
      "src/util/unix",
      "src/util/win32",
    ],
    sources: ["src/util"],
    publicHeadersPath: "include"
  ),

  .target(
    name: .name("util_unix"),
    path: ".",
    sources: ["src/util/unix"],
    publicHeadersPath: "include"
  ),

  .target(
    name: .name("util_win32"),
    path: ".",
    sources: ["src/util/win32"],
    publicHeadersPath: "include"
  ),
]

targetDependencies += [
  .target(name: .name("util")),
  .target(name: .name("util_unix"), condition: .when(platforms: apple + [.android, .linux, .wasi])),
  .target(name: .name("util_win32"), condition: .when(platforms: [.windows])),
]

// MARK: - Threading

cSettings += [
  .define("GIT_THREADS", to: "1", .when(platforms: apple + [.android, .linux, .windows])),
  .define("GIT_THREADS_PTHREADS", to: "1", .when(platforms: apple + [.android, .linux])),
  .define("GIT_THREADS_NATIVE", to: "1", .when(platforms: [.windows])),
]

linkerSettings += [
  .linkedLibrary("pthread", .when(platforms: [.android, .linux])),
]

// MARK: - Zlib

targets += [

  .target(
    name: .name("zlib"),
    path: ".",
    exclude: [
      "deps/zlib/CMakeLists.txt",
      "deps/zlib/LICENSE",
    ],
    sources: ["deps/zlib"],
    publicHeadersPath: "include"
  )
]

targetDependencies += [
  .target(name: .name("zlib"))
]

cSettings += [
  .headerSearchPath("deps/zlib"),
  .define("GIT_COMPRESSION_BUILTIN", to: "1"),
]

// Linux/Android link against system zlib for decompression
linkerSettings += [
  .linkedLibrary("z", .when(platforms: [.android, .linux])),
]

// MARK: - PCRE2

targets += [

  .target(
    name: .name("pcre2"),
    path: ".",
    exclude: [
      "deps/pcre2/CMakeLists.txt",
      "deps/pcre2/LICENCE.md",
      "deps/pcre2/config.h.in",
      "deps/pcre2/pcre2_fuzzsupport.c",
    ],
    sources: ["deps/pcre2"],
    publicHeadersPath: "include"
  )
]

targetDependencies += [
  .target(name: .name("pcre2"))
]

cSettings += [

  .headerSearchPath("deps/pcre2"),

  // From cmake/SelectRegex.cmake: Select bundled PCRE2 as the regex backend.
  .define("GIT_REGEX_BUILTIN", to: "1"),

  // From cmake/SelectRegex.cmake: Statically compile PCRE2 into libgit2.
  .define("PCRE2_STATIC", to: "1"),
  .define("PCRE2_EXPORT", to: ""),
  .define("PCRE2_EXP_DECL", to: ""),
  .define("PCRE2_EXP_DEFN", to: ""),

  // From deps/pcre2/CMakeLists.txt: Bundled PCRE2 configuration values.
  .define("SUPPORT_PCRE2_8", to: "1"),
  .define("SUPPORT_UNICODE", to: "1"),
  .define("NEWLINE_DEFAULT", to: "2"),
  .define("LINK_SIZE", to: "2"),
  .define("MAX_VARLOOKBEHIND", to: "255"),
  .define("PARENS_NEST_LIMIT", to: "250"),
  .define("HEAP_LIMIT", to: "20000000"),
  .define("MATCH_LIMIT", to: "10000000"),
  .define("MATCH_LIMIT_DEPTH", to: "MATCH_LIMIT"),
  .define("PCRE2_CODE_UNIT_WIDTH", to: "8"),

  // From deps/pcre2/config.h.in: PCRE2 name-table limits.
  .define("MAX_NAME_SIZE", to: "128"),
  .define("MAX_NAME_COUNT", to: "10000"),
]

// MARK: - xdiff

targets += [

  .target(
    name: .name("xdiff"),
    path: ".",
    exclude: ["deps/xdiff/CMakeLists.txt"],
    sources: ["deps/xdiff"],
    publicHeadersPath: "include"
  )
]

targetDependencies += [
  .target(name: .name("xdiff"))
]

cSettings += [
  .headerSearchPath("deps/xdiff"),
]

// MARK: - reftable

targets += [

  .target(
    name: .name("reftable"),
    path: ".",
    exclude: [
      "deps/reftable/CMakeLists.txt",
      "deps/reftable/LICENSE",
    ],
    sources: ["deps/reftable"],
    publicHeadersPath: "include"
  )
]

targetDependencies += [
  .target(name: .name("reftable"))
]

cSettings += [
  .headerSearchPath("deps/reftable"),
]

// MARK: - llhttp

targets += [

  .target(
    name: .name("llhttp"),
    path: ".",
    exclude: [
      "deps/llhttp/CMakeLists.txt",
      "deps/llhttp/LICENSE-MIT",
    ],
    sources: ["deps/llhttp"],
    publicHeadersPath: "include"
  )
]

targetDependencies += [
  .target(name: .name("llhttp"))
]

cSettings += [
  .headerSearchPath("deps/llhttp"),
  .define("GIT_HTTPPARSER_BUILTIN", to: "1"),
]

// MARK: - Hash Implementations

targets += [

  .target(
    name: .name("hash_common_crypto"),
    path: ".",
    sources: [
      "src/util/hash/common_crypto.h",
      "src/util/hash/common_crypto.c",
    ],
    publicHeadersPath: "include"
  ),

  .target(
    name: .name("hash_win32"),
    path: ".",
    sources: [
      "src/util/hash/win32.h",
      "src/util/hash/win32.c",
    ],
    publicHeadersPath: "include"
  ),

  .target(
    name: .name("hash_rfc6234"),
    path: ".",
    sources: ["src/util/hash/rfc6234"],
    publicHeadersPath: "include"
  ),

  .target(
    name: .name("hash_sha1dc"),
    path: ".",
    sources: ["src/util/hash/sha1dc"],
    publicHeadersPath: "include"
  ),
]

targetDependencies += [
  .target(name: .name("hash_common_crypto"), condition: .when(platforms: apple)),
  .target(name: .name("hash_win32"), condition: .when(platforms: [.windows])),
  .target(name: .name("hash_rfc6234"), condition: .when(platforms: [.linux, .android, .wasi])),
  .target(name: .name("hash_sha1dc"), condition: .when(platforms: [.linux, .android, .wasi])),
]

cSettings += [
  .headerSearchPath("src/util/hash/sha1dc", .when(platforms: [.linux, .android, .wasi])),
  .headerSearchPath("src/util/hash/rfc6234", .when(platforms: [.linux, .android, .wasi])),
]

cSettings += [

  // Apple: CommonCrypto
  .define("GIT_SHA1_COMMON_CRYPTO", to: "1", .when(platforms: apple)),
  .define("GIT_SHA256_COMMON_CRYPTO", to: "1", .when(platforms: apple)),

  // Windows: CNG
  .define("GIT_SHA1_WIN32", to: "1", .when(platforms: [.windows])),
  .define("GIT_SHA256_WIN32", to: "1", .when(platforms: [.windows])),

  // Linux/Android/WASI: Builtin collision-detecting SHA1 + RFC6234 SHA256
  .define("GIT_SHA1_COLLISIONDETECT", to: "1", .when(platforms: [.android, .linux, .wasi])),
  .define("GIT_SHA256_BUILTIN", to: "1", .when(platforms: [.android, .linux, .wasi])),
  .define("SHA1DC_NO_STANDARD_INCLUDES", to: "1", .when(platforms: [.android, .linux, .wasi])),
  .define("SHA1DC_CUSTOM_INCLUDE_SHA1_C", to: "\"git2_util.h\"", .when(platforms: [.android, .linux, .wasi])),
  .define("SHA1DC_CUSTOM_INCLUDE_UBC_CHECK_C", to: "\"git2_util.h\"", .when(platforms: [.android, .linux, .wasi])),
]

// MARK: - TLS / HTTPS
//
// Note: openssl.c and mbedtls.c contain stub implementations that return 0 when
// their backends are disabled (no GIT_OPENSSL/GIT_MBEDTLS defined). These stubs
// are required because libgit2.c unconditionally calls git_openssl_stream_global_init
// and git_mbedtls_stream_global_init in its initialization. We include these files
// on all platforms to get the stubs.

targets += [

  .target(
    name: .name("streams"),
    path: ".",
    sources: [
      "src/libgit2/streams/mbedtls.c",
      "src/libgit2/streams/openssl.c",
      "src/libgit2/streams/registry.c",
      "src/libgit2/streams/socket.c",
      "src/libgit2/streams/tls.c",
    ],
    publicHeadersPath: "include"
  ),

  .target(
    name: .name("streams_secure_transport"),
    path: ".",
    sources: ["src/libgit2/streams/stransport.c"],
    publicHeadersPath: "include"
  ),

  .target(
    name: .name("streams_schannel"),
    path: ".",
    sources: ["src/libgit2/streams/schannel.c"],
    publicHeadersPath: "include"
  ),

  .target(
    name: .name("streams_openssl"),
    path: ".",
    sources: [
      "src/libgit2/streams/openssl_dynamic.c",
      "src/libgit2/streams/openssl_legacy.c",
    ],
    publicHeadersPath: "include"
  ),
]

targetDependencies += [
  .target(name: .name("streams")),
  .target(name: .name("streams_secure_transport"), condition: .when(platforms: apple)),
  .target(name: .name("streams_schannel"), condition: .when(platforms: [.windows])),
  .target(name: .name("streams_openssl"), condition: .when(platforms: [.android, .linux, .wasi])),
]

excludedPaths += [
  "src/libgit2/streams",
]

cSettings += [

  .define("GIT_HTTPS", to: "1", .when(platforms: apple + [.android, .linux, .windows])),

  // Apple: SecureTransport for TLS
  .define("GIT_SECURE_TRANSPORT", to: "1", .when(platforms: apple)),

  // Linux/Android: OpenSSL (dynamically loaded) for TLS
  .define("GIT_OPENSSL", to: "1", .when(platforms: [.android, .linux])),
  .define("GIT_OPENSSL_DYNAMIC", to: "1", .when(platforms: [.android, .linux])),

  // Windows: WinHTTP for HTTP transport, Schannel for TLS
  .define("GIT_WINHTTP", to: "1", .when(platforms: [.windows])),
  .define("GIT_SCHANNEL", to: "1", .when(platforms: [.windows])),
]

linkerSettings += [

  // Apple: Security and CoreFoundation frameworks for SecureTransport TLS
  .linkedFramework("Security", .when(platforms: apple)),
  .linkedFramework("CoreFoundation", .when(platforms: apple)),

  // Linux/Android: dlopen for OpenSSL
  .linkedLibrary("dl", .when(platforms: [.android, .linux])),

  // Windows: WinHTTP and crypto libraries
  .linkedLibrary("winhttp", .when(platforms: [.windows])),
  .linkedLibrary("crypt32", .when(platforms: [.windows])),
  .linkedLibrary("secur32", .when(platforms: [.windows])),
]

// MARK: - Transports

traits.insert(
  .trait(
    name: "libssh2",
    description: "Use libssh2 for SSH transport (enables SSH for iOS, tvOS, watchOS, visionOS, Windows)"
  )
)

packageDependencies += [
  .package(
    url: "https://github.com/danielctull-forks/swift-libssh2.git",
    from: "1.11.1"
  ),
]

excludedPaths += [
  "src/libgit2/transports",
]

targets += [

  .target(
    name: .name("transports"),
    dependencies: [
      .product(
        name: "libssh2",
        package: "swift-libssh2",
        condition: .when(traits: ["libssh2"])
      ),
    ],
    path: ".",
    exclude: [
      "src/libgit2/transports/http.c",
      "src/libgit2/transports/winhttp.c",
    ],
    sources: ["src/libgit2/transports"],
    publicHeadersPath: "include"
  ),

  .target(
    name: .name("transports_http"),
    path: ".",
    sources: ["src/libgit2/transports/http.c"],
    publicHeadersPath: "include"
  ),

  .target(
    name: .name("transports_winhttp"),
    path: ".",
    sources: ["src/libgit2/transports/winhttp.c"],
    publicHeadersPath: "include"
  ),
]

targetDependencies += [
  .target(name: .name("transports")),
  .target(name: .name("transports_http"), condition: .when(platforms: apple + [.android, .linux, .wasi])),
  .target(name: .name("transports_winhttp"), condition: .when(platforms: [.windows])),
]

cSettings += [

  // Use bundled libssh2 on all platforms
  .define("GIT_SSH", to: "1", .when(traits: ["libssh2"])),
  .define("GIT_SSH_LIBSSH2", to: "1", .when(traits: ["libssh2"])),

  // Use ssh_exec on platforms that support process spawning
  .define("GIT_SSH", to: "1", .when(platforms: [.android, .linux, .macOS], traits: [])),
  .define("GIT_SSH_EXEC", to: "1", .when(platforms: [.android, .linux, .macOS], traits: [])),
]

// MARK: - NTLM Authentication

targets += [

  .target(
    name: .name("ntlm"),
    path: ".",
    sources: [
      "deps/ntlmclient/ntlm.c",
      "deps/ntlmclient/unicode_builtin.c",
      "deps/ntlmclient/util.c",
    ],
    publicHeadersPath: "include"
  ),

  .target(
    name: .name("ntlm_common_crypto"),
    path: ".",
    sources: ["deps/ntlmclient/crypt_commoncrypto.c"],
    publicHeadersPath: "include"
  ),

  .target(
    name: .name("ntlm_openssl"),
    path: ".",
    sources: ["deps/ntlmclient/crypt_openssl.c"],
    publicHeadersPath: "include"
  ),
]

targetDependencies += [
  .target(name: .name("ntlm"), condition: .when(platforms: apple + [.android, .linux])),
  .target(name: .name("ntlm_common_crypto"), condition: .when(platforms: apple)),
  .target(name: .name("ntlm_openssl"), condition: .when(platforms: [.android, .linux])),
]

cSettings += [
  .headerSearchPath("deps/ntlmclient"),
  // Enable NTLM on platforms that use ntlmclient
  .define("GIT_AUTH_NTLM", to: "1", .when(platforms: apple + [.android, .linux])),
  .define("GIT_AUTH_NTLM_BUILTIN", to: "1", .when(platforms: apple + [.android, .linux])),
  .define("NTLM_STATIC", to: "1", .when(platforms: apple + [.android, .linux])),
  .define("UNICODE_BUILTIN", to: "1", .when(platforms: apple + [.android, .linux])),
  // Crypto backend selection
  .define("CRYPT_COMMONCRYPTO", .when(platforms: apple)),
  .define("CRYPT_OPENSSL", .when(platforms: [.android, .linux])),
  .define("CRYPT_OPENSSL_DYNAMIC", .when(platforms: [.android, .linux])),
  .define("OPENSSL_API_COMPAT", to: "0x10100000L", .when(platforms: [.android, .linux])),
]

// MARK: - Process Spawning

cSettings += [
  .define("GIT_NO_PROCESS_SPAWN", .when(platforms: [.iOS, .tvOS, .visionOS, .wasi, .watchOS])),
]

// MARK: - Internationalization

cSettings += [
  .define("GIT_I18N", to: "1", .when(platforms: apple)),
  .define("GIT_I18N_ICONV", to: "1", .when(platforms: apple)),
]

linkerSettings += [
  .linkedLibrary("iconv", .when(platforms: apple)),
]

// MARK: - I/O and Polling

cSettings += [
  .define("GIT_IO_POLL", to: "1", .when(platforms: apple + [.android, .linux, .wasi])),
  .define("GIT_IO_WSAPOLL", to: "1", .when(platforms: [.windows])),
]

linkerSettings += [
  .linkedLibrary("ws2_32", .when(platforms: [.windows])),
]

// MARK: - qsort

cSettings += [
  .define("GIT_QSORT_BSD", .when(platforms: apple)),
  .define("GIT_QSORT_GNU", .when(platforms: [.linux])),
  .define("GIT_QSORT_MSC", .when(platforms: [.windows])),
]

// MARK: - Nanosecond Timestamps

cSettings += [
  .define("GIT_NSEC", to: "1", .when(platforms: apple + [.android, .linux, .wasi])),
  .define("GIT_FUTIMENS", to: "1", .when(platforms: apple + [.android, .linux, .wasi])),
  .define("GIT_NSEC_MTIMESPEC", to: "1", .when(platforms: apple)),
  .define("GIT_NSEC_MTIM", to: "1", .when(platforms: [.android, .linux])),
]

// MARK: - Random Number Generation

cSettings += [
  .define("GIT_RAND_GETENTROPY", to: "1", .when(platforms: [.linux])),
  .define("GIT_RAND_GETLOADAVG", to: "1", .when(platforms: [.linux])),
]

// MARK: - Platform-Specific Compiler/Linker Flags

// GNU extensions for Linux/Android
cSettings += [
  .define("_GNU_SOURCE", .when(platforms: [.android, .linux])),
]

// Windows-specific defines
cSettings += [
  .define("WIN32", .when(platforms: [.windows])),
  .define("_WIN32_WINNT", to: "0x0600", .when(platforms: [.windows])),
  .define("_CRT_SECURE_NO_WARNINGS", .when(platforms: [.windows])),
  .define("_CRT_NONSTDC_NO_DEPRECATE", .when(platforms: [.windows])),
]

linkerSettings += [
  .linkedLibrary("rpcrt4", .when(platforms: [.windows])),
  .linkedLibrary("ole32", .when(platforms: [.windows])),
]

// MARK: - Package Definition

let targetsWithSettings: [Target] = targets.map { target in
  .target(
    name: target.name, // Prefix name to avoid collisions.
    dependencies: target.dependencies,
    path: target.path,
    exclude: target.exclude,
    sources: target.sources,
    resources: target.resources,
    publicHeadersPath: target.publicHeadersPath,
    packageAccess: target.packageAccess,
    cSettings: cSettings + (target.cSettings ?? []), // Add shared settings
    cxxSettings: target.cxxSettings,
    swiftSettings: target.swiftSettings,
    linkerSettings: target.linkerSettings,
    plugins: target.plugins,
  )
}

let package = Package(
  name: "swift-libgit2",
  products: [
    .library(name: "libgit2", targets: ["libgit2"])
  ],
  traits: traits,
  dependencies: packageDependencies,
  targets: targetsWithSettings + [
    .target(
      name: "libgit2",
      dependencies: targetDependencies,
      path: ".",
      exclude: excludedPaths,
      sources: sourcePaths,
      publicHeadersPath: "include",
      cSettings: cSettings,
      linkerSettings: linkerSettings
    ),
  ]
)
