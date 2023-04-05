LIBNAME := sqlwrite
CXXFLAGS := -std=c++17 -O3 -DNDEBUG -Ifmt/include
CFLAGS := -O3 -DNDEBUG

ifeq ($(shell uname -s),Darwin)
LIBFILE := lib$(LIBNAME).dylib
DYNAMIC_LIB := -dynamiclib
SQLITE_LIB = libsqlite3.dylib
else
LIBFILE := lib$(LIBNAME).so
DYNAMIC_LIB := -shared
SQLITE_LIB = libsqlite3.so
endif

all: *.c *.cpp *.hpp
	clang++ $(CXXFLAGS) $(DYNAMIC_LIB) -o $(LIBFILE) sqlwrite.cpp fmt/src/format.cc -lcurl
	clang $(CFLAGS) $(DYNAMIC_LIB) -o $(SQLITE_LIB) sqlite3.c 
	clang shell.c -L. -lsqlite3 -o sqlite3
