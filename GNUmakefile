all: *.c *.cpp *.hpp
	clang++ -Ifmt/include -std=c++17 -dynamiclib -o sqlwrite.dylib sqlwrite.cpp fmt/src/format.cc -lcurl
	clang -dynamiclib -o libsqlite3.dylib sqlite3.c 
	clang shell.c -L. -lsqlite3 -o sqlite3
