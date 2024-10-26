LIBNAME := sqlwrite
OPTIMIZATION := # -O3
CXXFLAGS := -std=c++17 -g $(OPTIMIZATION) -DNDEBUG -I. -Ifmt/include
CFLAGS := $(OPTIMIZATION) -g -DNDEBUG

ifeq ($(shell uname -s),Darwin)
LIBFILE := $(LIBNAME).dylib
DYNAMIC_LIB := -dynamiclib
SQLITE_LIB = libsqlite3.dylib
CXXFLAGS := $(CXXFLAGS) -arch arm64 $(shell pkg-config --cflags --libs libcurl openssl)
else
LIBFILE := $(LIBNAME).so
DYNAMIC_LIB := -shared -fPIC
SQLITE_LIB = libsqlite3.so
CXXFLAGS := $(CXXFLAGS) -lcurl -lssl -lcrypto
endif

DOMAIN = org.plasma-umass.sqlwrite

ifeq ($(shell uname -s),Darwin)
all: $(LIBFILE) $(SQLITE_LIB) sqlwrite pkg
else
all: $(LIBFILE) $(SQLITE_LIB) sqlwrite
endif

$(LIBFILE): sqlwrite.cpp fmt/src/format.cc
	clang++ $(CXXFLAGS) $(DYNAMIC_LIB) -o $(LIBFILE) $^

$(SQLITE_LIB): sqlite3.c
	clang $(CFLAGS) $(DYNAMIC_LIB) -o $(SQLITE_LIB) $^

sqlwrite: shell.c $(SQLITE_LIB)
	clang $(CFLAGS) shell.c -L. -lsqlite3 -o sqlwrite

ifeq ($(shell uname -s),Darwin)
pkg: sqlwrite
        # Create the package directory structure
	mkdir -p pkg_root/usr/local/bin
	mkdir -p pkg_root/usr/local/lib
	cp sqlwrite pkg_root/usr/local/bin
	cp $(LIBFILE) pkg_root/usr/local/lib
	cp $(SQLITE_LIB) pkg_root/usr/local/lib

        # Adjust the binary to reference the correct library paths
	install_name_tool -change libsqlite3.dylib /usr/local/lib/$(SQLITE_LIB) pkg_root/usr/local/bin/sqlwrite
	install_name_tool -change $(LIBFILE) /usr/local/lib/$(LIBFILE) pkg_root/usr/local/bin/sqlwrite

        # Use pkgbuild to create the .pkg installer
	pkgbuild --root pkg_root --identifier $(DOMAIN) --version 1.0 --install-location / sqlwrite-mac.pkg
endif


clean:
	rm -rf sqlwrite-mac.pkg sqlwrite $(LIBFILE) $(SQLITE_LIB)


