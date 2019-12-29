CXX = g++
CC = gcc
AR = ar
OBJECTS = client.o common.o polling.o
LIB = liborts.so
LDFLAGS = -pthread
SERVER = server
ifeq ($(TARGET),WIN32)
CXX = x86_64-w64-mingw32-g++ -m64
CC = x86_64-w64-mingw32-gcc -m64
AR = x86_64-w64-mingw32-ar
#LDFLAGS += -mwindows -static-libgcc -static-libstdc++ -Wl,-Bstatic -lstdc++ -lpthread -lwsock32 -Wl,-Bdynamic -lmingw32
LDFLAGS += -lstdc++ -lmingw32 -lwsock32
LIB = liborts.dll
SERVER = server.exe
endif
all: $(LIB) $(SERVER)
$(LIB): $(OBJECTS)
	$(CXX) -shared -o $(LIB) $(OBJECTS) $(LDFLAGS) -g
$(SERVER): server.o
	$(CXX) server.o -o $(SERVER) -Wall $(LDFLAGS) -g -L. -lorts -I.
%.o: %.cpp
	$(CXX) -c $< -o $@ -Wall -std=c++11 -fpic -g
%.o: %.c
	$(CC) -c $< -o $@ -Wall -fpic -g
clean:
	rm $(OBJECTS) $(LIB)

install:
	install -d $(DESTDIR)/usr/lib
	install $(LIB) $(DESTDIR)/usr/lib
	install -d $(DESTDIR)/usr/include/orts
	install common.h $(DESTDIR)/usr/include/orts
	install client.h $(DESTDIR)/usr/include/orts
	install polling.h $(DESTDIR)/usr/include/orts
	install -d $(DESTDIR)/usr/bin
	install server $(DESTDIR)/usr/bin/or_server
