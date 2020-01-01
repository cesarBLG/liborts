CXX = g++
CC = gcc
AR = ar
OBJECTS = client.o common.o polling.o
LIB = liborts.so
LDFLAGS = -pthread
SERVER = server
PREFIX := /usr
ifeq ($(TARGET),WIN32)
CXX = x86_64-w64-mingw32-g++ -m64
CC = x86_64-w64-mingw32-gcc -m64
AR = x86_64-w64-mingw32-ar
#LDFLAGS += -mwindows -static-libgcc -static-libstdc++ -Wl,-Bstatic -lstdc++ -lpthread -lwsock32 -Wl,-Bdynamic -lmingw32
LDFLAGS += -lstdc++ -lmingw32 -lwsock32
LIB = liborts.dll
SERVER = server.exe
endif
all: $(LIB) $(SERVER) examples
$(LIB): $(OBJECTS)
	$(CXX) -shared -o $(LIB) $(OBJECTS) $(LDFLAGS) -g
$(SERVER): server.o $(LIB)
	$(CXX) server.o -o $(SERVER) -Wall $(LDFLAGS) -g -L. -lorts -I.
%.o: %.cpp
	$(CXX) -c $< -o $@ -Wall -std=c++11 -fpic -g
%.o: %.c
	$(CC) -c $< -o $@ -Wall -fpic -g
clean:
	rm $(OBJECTS) $(LIB)

install:
	install -d $(DESTDIR)$(PREFIX)/lib
	install $(LIB) $(DESTDIR)$(PREFIX)/lib
	install -d $(DESTDIR)$(PREFIX)/include/orts
	install common.h $(DESTDIR)$(PREFIX)/include/orts
	install client.h $(DESTDIR)$(PREFIX)/include/orts
	install polling.h $(DESTDIR)$(PREFIX)/include/orts
	install -d $(DESTDIR)$(PREFIX)/bin
	install server $(DESTDIR)$(PREFIX)/bin/or_server
	install -d $(DESTDIR)$(PREFIX)/lib/systemd/system
	install or_server@.service $(DESTDIR)$(PREFIX)/lib/systemd/system
	install Examples/RailDriver/raildriver $(DESTDIR)$(PREFIX)/bin/raildriver
examples: $(LIB)
	$(MAKE) -C Examples
