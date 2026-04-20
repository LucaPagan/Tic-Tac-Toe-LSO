cc = gcc

cflags = -Wall -pedantic -O0 -std=c11 -g -fsanitize=address -D_GNU_SOURCE -I/opt/homebrew/include
# librerie esterne
extLib = -L/opt/homebrew/lib -lcjson

# Regole per la compilazione di ogni file .c in .o
%.o: %.c
	$(cc) $(cflags) -c $< -o $@

SOURCES = ./dynamic_array.c ./queue_list.c ./server.c ./main.c
OBJECTS = $(SOURCES:.c=.o)
TARGET = server.out # nome del file di output

all: server

# Compilazione default
server: $(OBJECTS)
	$(cc) $(cflags) $(OBJECTS) $(extLib) -o $(TARGET)

TEST_SOURCES = ./dynamic_array.c ./queue_list.c ./server.c ./test.c
TEST_OBJECTS = $(TEST_SOURCES:.c=.o)
TEST_TARGET = test.out

test: $(TEST_OBJECTS)
	$(cc) $(cflags) $(TEST_OBJECTS) $(extLib) -o $(TEST_TARGET)

# Test specifico per la dynamic array (solo il file di test + libreria)
TEST_DYN_SOURCES = ./dynamic_array.c
TEST_DYN_TARGET = test_dyn.out

# Google Test
GTEST_LIB = -lgtest -lgtest_main -lpthread
test_gtest: server.o dynamic_array.o queue_list.o tests/gtest_tris.cpp
	g++ $(cflags) tests/gtest_tris.cpp server.o dynamic_array.o queue_list.o $(extLib) $(GTEST_LIB) -o test_gtest.out

test_dyn: $(TEST_DYN_SOURCES) tests/dynamic_array_test.c
	$(cc) $(cflags) tests/dynamic_array_test.c $(TEST_DYN_SOURCES) $(extLib) -o $(TEST_DYN_TARGET)

# Pulizia dei file oggetto ed eseguibile
clean:
	rm -f $(OBJECTS) $(TEST_OBJECTS) $(TEST_DYN_TARGET) $(TEST_TARGET) $(TARGET) *.o
