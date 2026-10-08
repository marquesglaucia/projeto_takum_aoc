# Uso: make test LIBTAKUM=/caminho/para/libtakum   (compare com a referencia oficial)
CXX=g++
CXXFLAGS=-O2 -std=c++17 -Wall
LIBTAKUM?=../libtakum
lib/libtakum.a:
	mkdir -p lib && for f in $(LIBTAKUM)/src/*.c; do gcc -O2 -std=c99 -c $$f -o lib/$$(basename $$f .c).o; done && ar rcs $@ lib/*.o
test: lib/libtakum.a
	$(CXX) $(CXXFLAGS) -I$(LIBTAKUM) test_takum.cpp takum_decoder.cpp lib/libtakum.a -lm -o test_takum && ./test_takum