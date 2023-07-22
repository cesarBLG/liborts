CXX = g++ -D_GLIBCXX_DEBUG
CC = gcc
AR = ar
LDFLAGS = -L../.. -lorts -pthread
ifeq ($(TARGET),WIN32)
CXX = x86_64-w64-mingw32-g++ -m64
CC = x86_64-w64-mingw32-gcc -m64
AR = x86_64-w64-mingw32-ar
LDFLAGS += -mconsole -lstdc++ -lpthread -lwsock32 -lmingw32 -L. -lPIEHid64
endif
all: raildriver

raildriver: raildriver.cpp $(LIB)
	$(CXX) raildriver.cpp -o raildriver -g $(LDFLAGS) -Wall -I../../include

