cmake_minimum_required (VERSION 3.6)
project (liborts)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
add_library(orts SHARED client.cpp common.cpp polling.cpp ip_discovery.cpp)
target_include_directories(orts PRIVATE include)

add_executable(server server.cpp serverlib.cpp)
target_include_directories(server PRIVATE include)
target_link_libraries(server PRIVATE orts)

if(WIN32)
    target_link_libraries(server PRIVATE ws2_32)
endif()

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    if(WIN32)
        add_subdirectory(Examples/TrainSimulator)
    endif()
endif()
