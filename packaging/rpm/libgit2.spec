#
# spec file for package libgit2
#
# Copyright (c) 2012 Saleem Ansari <tuxdna@gmail.com>
# Copyright (c) 2012 SUSE LINUX Products GmbH, Nuernberg, Germany.
# Copyright (c) 2011, Sascha Peilicke <saschpe@gmx.de>
#
# All modifications and additions to the file contributed by third parties
# remain the property of their copyright owners, unless otherwise agreed
# upon. The license for this file, and modifications and additions to the
# file, is the same license as for the pristine package itself (unless the
# license for the pristine package is not an Open Source License, in which
# case the license is the MIT License). An "Open Source License" is a
# license that conforms to the Open Source Definition (Version 1.9)
# published by the Open Source Initiative.

# Please submit bugfixes or comments via http://bugs.opensuse.org/
#
Name:           libgit2
Version:        0.16.0
Release:        1
Summary:        C git library
License:        GPL-2.0 with linking
Group:          Development/Libraries/C and C++
Url:            http://libgit2.github.com/
Source0:        https://github.com/downloads/libgit2/libgit2/libgit2-0.16.0.tar.gz
BuildRequires:  cmake
BuildRequires:  pkgconfig
BuildRoot:      %{_tmppath}/%{name}-%{version}-build
%if 0%{?fedora} || 0%{?rhel_version} || 0%{?centos_version}
BuildRequires:  openssl-devel
%else
BuildRequires:  libopenssl-devel
%endif

%description
libgit2 is a portable, pure C implementation of the Git core methods
provided as a re-entrant linkable library with a solid API, allowing
you to write native speed custom Git applications in any language
with bindings.

%package -n %{name}-0
Summary:        C git library
Group:          System/Libraries

%description -n %{name}-0
libgit2 is a portable, pure C implementation of the Git core methods
provided as a re-entrant linkable library with a solid API, allowing
you to write native speed custom Git applications in any language
with bindings.

%package devel
Summary:        C git library
Group:          Development/Libraries/C and C++
Requires:       %{name}-0 >= %{version}

%description devel
This package contains all necessary include files and libraries needed
to compile and develop applications that use libgit2.

%prep
%setup -q

%build
cmake . \
    -DCMAKE_C_FLAGS:STRING="%{optflags}" \
    -DCMAKE_INSTALL_PREFIX:PATH=%{_prefix} \
    -DLIB_INSTALL_DIR:PATH=%{_libdir}S
make %{?_smp_mflags}

%install
%make_install

%post -n %{name}-0 -p /sbin/ldconfig
%postun -n %{name}-0 -p /sbin/ldconfig

%files -n %{name}-0
%defattr (-,root,root)
%doc AUTHORS COPYING README.md
%{_libdir}/%{name}.so.*

%files devel
%defattr (-,root,root)
%doc CONVENTIONS examples
%{_libdir}/%{name}.so
%{_includedir}/git2*
%{_libdir}/pkgconfig/libgit2.pc

%changelog
* Tue Mar 04 2012 tuxdna@gmail.com
- Update to version 0.16.0 
* Tue Jan 31 2012 jengelh@medozas.de
- Provide pkgconfig symbols
* Thu Oct 27 2011 saschpe@suse.de
- Change license to 'GPL-2.0 with linking', fixes bnc#726789
* Wed Oct 26 2011 saschpe@suse.de
- Update to version 0.15.0:
  * Upstream doesn't provide changes
- Removed outdated %%clean section
* Tue Jan 18 2011 saschpe@gmx.de
- Proper Requires for devel package
* Tue Jan 18 2011 saschpe@gmx.de
- Set BuildRequires to "openssl-devel" also for RHEL and CentOS
* Tue Jan 18 2011 saschpe@gmx.de
- Initial commit (0.0.1)
- Added patch to fix shared library soname
