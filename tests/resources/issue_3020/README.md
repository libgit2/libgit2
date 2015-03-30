HashCheck for Windows
=====================

HashCheck is a Windows application that creates and verifies file checksums.


How to build
------------

This project is compatible with:
- Visual Studio 2013
- Eclipse Luna CDT with MinGW (GCC 4.8.1)


Usage
-----

Launch the executable to create a checksum file if it does not already exists.

Launch the executable to verify file integrity against a checksum file if it exists.
 

License
-------

Hashing code found on the Internet is in the public domain:

MD5: Colin Plumb (1993) / John Walker (2003)

SHA1: Steve Reid (199?) / ... / Ralph Giles (2002)

FileStream code inspired by dotnet/corefx's Win32FileStream.cs.  See FileStream.cpp for licensing information.


FAQ
---

### Why MinGW?

I wanted to have an executable having the least runtime dependencies possible.

### Why FileStream?

MinGW's fstream can't open WCHAR filenames and I was not interested in changing fstream's implementation.

### What's the coding style?

The coding style is inspired from Google C++ [coding style guide](http://google-styleguide.googlecode.com/svn/trunk/cppguide.html "Google C++ Style Guide").
