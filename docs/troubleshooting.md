Troubleshooting libgit2 Problems
================================

CMake Failures
--------------

* **`Asked for OpenSSL TLS backend, but it wasn't found`**
  CMake cannot find your SSL/TLS libraries.  By default, libgit2 always
  builds with HTTPS support, and you are encouraged to install the
  OpenSSL libraries for your system (eg, `apt-get install libssl-dev`).

  For development, if you simply want to disable HTTPS support entirely,
  pass the `-DUSE_HTTPS=OFF` argument to `cmake` when configuring it.

* **`-- Could NOT find PkgConfig (missing: PKG_CONFIG_EXECUTABLE)`**
  Make sure that PkgConfig is installed on your system. It is a tool
  used to retrieve information about installed libraries. If PkgConfig
  is not installed, you can usually install it using your package manager.
  (sudo apt-get install pkg-config).

* **`-- Could NOT find GSSAPI (missing: GSSAPI_LIBRARIES GSSAPI_INCLUDE_DIR)`**
  GSSAPI is the Generic Security Services API, and it seems the build system
  cannot find the necessary libraries and include files. Install the GSSAPI
  development package using your package manager. The package names may vary
  depending on your operating system. For example, on Debian-based systems, you
  can use: (sudo apt-get install libgssapi-dev) or (sudo apt install libgssglue-dev).


* **'-- Could NOT find OpenSSL(missing: OPENSSL_CRYPTO_LIBRARY OPENSSL_INCLUDE_DIR) logo`**
  Build system is unable to find OpenSSL. Make sure OpenSSL is installed on your system.
  If OpenSSL is installed, it's possible that the build system needs information about the
  location of the OpenSSL libraries and include files. You can set the OPENSSL_ROOT_DIR
  environment variable to the root folder of your OpenSSL installation. For example:
  (export OPENSSL_ROOT_DIR=/path/to/your/openssl)
