cmake_minimum_required (VERSION 3.6)
project (ts64)
add_executable(ts64 trainsimulator.cpp interface.cpp)
target_compile_definitions(ts64 PUBLIC _GLIBCXX_DEBUG)
target_link_libraries(ts64 PRIVATE orts wsock32)
