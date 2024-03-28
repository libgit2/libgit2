This error is raised because the pkg-config is not available in your system
Steps to resolve the problem
Install PkgConfig using the package manager
sudo apt-get update
sudo apt-get install pkg-config
PkgConfig tells the compiler where to find dependent libraries. Installing it with apt-get usually resolves the issue.

If GSSAPI is missing, Install the GSSAPI development package
sudo apt-get update
sudo apt-get install libkrb5-dev
If the issue persists, ensure that the GSSAPI libraries and include files are in standard locations. You can check their locations using
dpkg -L libgssapi-dev
Make sure that the paths are included in the compiler and linker search paths

If OpenSSL is missing, Install the OpenSSL development package
sudo apt-get update
sudo apt-get install libssl-dev
Installing libssl-dev provides the required OpenSSL libraries and headers.
You may also need to set the OpenSSL root directory path as a CMake variable. For example
cmake -DOPENSSL_ROOT_DIR=/usr/lib/ssl ..
Ensure to include any other details about the error messages in the documentation along with these troubleshooting steps. Let me know if you need any clarification or have additional questions!
