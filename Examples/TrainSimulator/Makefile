all: ts64.exe
ts64.exe: trainsimulator.cpp
	x86_64-w64-mingw32-g++ -m64 trainsimulator.cpp interface.cpp -o ts64.exe $(CXXFLAGS) -L../.. -lorts -lwsock32
