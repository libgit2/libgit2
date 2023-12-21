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
-------------------
-- Could NOT find PkgConfig (missing: PKG_CONFIG_EXECUTABLE) 
the reason for this is the unavailabilty of the utility in your system
install pk-config utility on your system,resolving this on Linux is a simple task by installing pkg-config through the following method:
     sudo apt-get install pkg-config
source :https://copyprogramming.com/howto/error-could-not-find-pkgconfig-missing-pkg-config-executable

-- Could NOT find GSSAPI (missing: GSSAPI_LIBRARIES GSSAPI_INCLUDE_DIR)

$ sudo ln -s /usr/bin/krb5-config.mit /usr/bin/krb5-config
$ sudo ln -s /usr/lib/x86_64-linux-gnu/libgssapi_krb5.so.2 /usr/lib/libgssapi_krb5.so
$ sudo apt-get install python-pip libkrb5-dev
$ sudo pip install gssapi

source: https://devpress.csdn.net/python/63045d5f7e6682346619a917.html

-- Could NOT find OpenSSL, try to set the path to OpenSSL root folder in the system variable OPENSSL_ROOT_DIR (missing: OPENSSL_CRYPTO_LIBRARY OPENSSL_INCLUDE_DIR)

The package name is different in Linux distributions. Use as per below.
Open a terminal window and use the following command to install in Ubuntu, Debian, Linux Mint and related distributions.
sudo apt install libssl-dev

For Fedora, CentOS, RHEL and related distributions, use the below.
sudo dnf install openssl-dev

For Arch Linux, use the following command to install it.
sudo pacman -S --needed openssl

After you install it, you can now run your compilation which caused the error.


