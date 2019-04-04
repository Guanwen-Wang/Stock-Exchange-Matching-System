CFLAGS=-std=gnu++11 -pedantic -ggdb3
EXTRAFLAGS=-lpqxx -lpq -pthread

all: server

server: main.cpp mystruct.h helper.h database.h database.cpp server.h server.cpp
	g++ $(CFLAGS) -o server main.cpp database.cpp server.cpp helper.cpp $(EXTRAFLAGS)

clean:
	rm -f *~ *.o server

clobber:
	rm -f *~ *.o

