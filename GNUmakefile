LIBNAME := sqlwrite
OPTIMIZATION := # -O3
CXXFLAGS := -std=c++17 -g $(OPTIMIZATION) -DNDEBUG -I. -Ifmt/include
CFLAGS := $(OPTIMIZATION) -g -DNDEBUG

ifeq ($(shell uname -s),Darwin)
LIBFILE := $(LIBNAME).dylib
DYNAMIC_LIB := -dynamiclib
SQLITE_LIB = libsqlite3.dylib
CXXFLAGS := $(CXXFLAGS) $(shell pkg-config --cflags --libs libcurl openssl)
else
LIBFILE := $(LIBNAME).so
DYNAMIC_LIB := -shared -fPIC
SQLITE_LIB = libsqlite3.so
CXXFLAGS := $(CXXFLAGS) -lcurl -lssl -lcrypto
endif

all: *.c *.cpp *.hpp
	clang++ $(CXXFLAGS) $(DYNAMIC_LIB) -o $(LIBFILE) sqlwrite.cpp fmt/src/format.cc
	clang $(CFLAGS) $(DYNAMIC_LIB) -o $(SQLITE_LIB) sqlite3.c 
	clang $(CFLAGS) shell.c -L. -lsqlite3 -o sqlwrite
