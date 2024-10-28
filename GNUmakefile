LIBNAME := sqlwrite
OPTIMIZATION := # -O3
CXXFLAGS := -std=c++17 -g $(OPTIMIZATION) -DNDEBUG -I. -Ifmt/include
CFLAGS := $(OPTIMIZATION) -g -DNDEBUG -fPIC


# Check for macOS and set variables accordingly
ifeq ($(shell uname -s),Darwin)
DYLIB_EXT = dylib
DYNAMIC_LIB := -dynamiclib
CXXFLAGS := $(CXXFLAGS) -arch arm64 $(shell pkg-config --cflags --libs libcurl openssl)
PACKAGE := pkg
else
DYLIB_EXT = so
DYNAMIC_LIB := -shared -fPIC
CFLAGS := $(CFLAGS) -lpthread -ldl
CXXFLAGS := $(CXXFLAGS) -lcurl -lssl -lcrypto
PACKAGE := linux-package
endif

LIBFILE := $(LIBNAME).$(DYLIB_EXT)
SQLITE_LIB = libsqlite3.$(DYLIB_EXT)
DOMAIN = org.plasma-umass.sqlwrite

all: $(LIBFILE) $(SQLITE_LIB) sqlwrite-bin $(PACKAGE)

$(LIBFILE): sqlwrite.cpp fmt/src/format.cc
	clang++ $(CXXFLAGS) $(DYNAMIC_LIB) -o $(LIBFILE) $^

$(SQLITE_LIB): sqlite3.c
	clang $(CFLAGS) $(DYNAMIC_LIB) -o $(SQLITE_LIB) $^

sqlwrite-bin: shell.c $(SQLITE_LIB)
	clang $(CFLAGS) shell.c -L. -lsqlite3 -o sqlwrite-bin

ifeq ($(shell uname -s),Darwin)
pkg: sqlwrite-bin $(LIBFILE) $(SQLITE_LIB)
        # Create the package directory structure
	mkdir -p pkg_root/usr/local/bin
	mkdir -p pkg_root/usr/local/lib
	cp sqlwrite pkg_root/usr/local/bin
	cp sqlwrite-bin pkg_root/usr/local/lib
	cp $(LIBFILE) pkg_root/usr/local/lib
	cp $(SQLITE_LIB) pkg_root/usr/local/lib

        # Adjust the binary to reference the correct library paths
	install_name_tool -change libsqlite3.dylib /usr/local/lib/$(SQLITE_LIB) pkg_root/usr/local/lib/sqlwrite-bin
        # install_name_tool -change $(LIBFILE) /usr/local/lib/$(LIBFILE) pkg_root/usr/local/lib/sqlwrite-bin

        # Use pkgbuild to create the .pkg installer
	pkgbuild --root pkg_root --identifier $(DOMAIN) --version 1.0 --install-location / sqlwrite-mac.pkg
endif

# Packaging for Linux systems

linux-package: deb-package rpm-package

deb-package: sqlwrite-bin $(LIBFILE) $(SQLITE_LIB)
        # Create the package directory structure
	mkdir -p pkg_root/usr/local/bin
	mkdir -p pkg_root/usr/local/lib
	cp sqlwrite pkg_root/usr/local/bin
	cp sqlwrite-bin pkg_root/usr/local/lib
	cp $(LIBFILE) pkg_root/usr/local/lib
	cp $(SQLITE_LIB) pkg_root/usr/local/lib

	mkdir -p pkg_root/DEBIAN
	echo "Package: sqlwrite" > pkg_root/DEBIAN/control
	echo "Version: 1.0" >> pkg_root/DEBIAN/control
	echo "Section: base" >> pkg_root/DEBIAN/control
	echo "Priority: optional" >> pkg_root/DEBIAN/control
	echo "Architecture: $(shell dpkg --print-architecture)" >> pkg_root/DEBIAN/control
	echo "Maintainer: your-email@example.com" >> pkg_root/DEBIAN/control
	echo "Description: Sqlwrite command-line tool" >> pkg_root/DEBIAN/control
	dpkg-deb --build pkg_root sqlwrite-linux.deb

rpm-package: sqlwrite-bin $(LIBFILE) $(SQLITE_LIB)
	mkdir -p rpm_root/usr/local/bin
	mkdir -p rpm_root/usr/local/lib
	cp sqlwrite rpm_root/usr/local/bin
	cp sqlwrite-bin rpm_root/usr/local/lib
	cp $(LIBFILE) rpm_root/usr/local/lib
	cp $(SQLITE_LIB) rpm_root/usr/local/lib

        # Create necessary RPM build directories outside the buildroot
	mkdir -p rpmbuild/BUILD rpmbuild/RPMS rpmbuild/SOURCES rpmbuild/SPECS rpmbuild/SRPMS

        # Generate the spec file outside the buildroot
	echo "%define _topdir $(shell pwd)/rpmbuild" > rpmbuild/SPECS/sqlwrite.spec
	echo "Name: sqlwrite" >> rpmbuild/SPECS/sqlwrite.spec
	echo "Version: 1.0" >> rpmbuild/SPECS/sqlwrite.spec
	echo "Release: 1" >> rpmbuild/SPECS/sqlwrite.spec
	echo "Summary: Sqlwrite command-line tool" >> rpmbuild/SPECS/sqlwrite.spec
	echo "License: Apache-2.0" >> rpmbuild/SPECS/sqlwrite.spec
	echo "Group: Development/Tools" >> rpmbuild/SPECS/sqlwrite.spec
	echo "BuildArch: $(shell uname -m)" >> rpmbuild/SPECS/sqlwrite.spec
	echo "%description" >> rpmbuild/SPECS/sqlwrite.spec
	echo "Sqlwrite command-line tool for SQL tasks." >> rpmbuild/SPECS/sqlwrite.spec
	echo "%files" >> rpmbuild/SPECS/sqlwrite.spec
	echo "/usr/local/bin/sqlwrite" >> rpmbuild/SPECS/sqlwrite.spec
	echo "/usr/local/lib/sqlwrite-bin" >> rpmbuild/SPECS/sqlwrite.spec
	echo "/usr/local/lib/$(LIBFILE)" >> rpmbuild/SPECS/sqlwrite.spec
	echo "/usr/local/lib/$(SQLITE_LIB)" >> rpmbuild/SPECS/sqlwrite.spec

        # Run rpmbuild with the spec file outside the buildroot
	rpmbuild -bb rpmbuild/SPECS/sqlwrite.spec --buildroot $(shell pwd)/rpm_root
	cp rpmbuild/RPMS/*/sqlwrite-1.0-1.*.rpm sqlwrite-linux.rpm

clean:
	rm -rf sqlwrite-mac.pkg sqlwrite-linux.deb sqlwrite-bin $(LIBFILE) $(SQLITE_LIB)


