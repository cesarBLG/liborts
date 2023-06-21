cmake_minimum_required (VERSION 3.6)
project (liborts)

add_executable(server server.cpp serverlib.cpp client.cpp common.cpp polling.cpp)
target_include_directories(server PRIVATE include)

if(WIN32)
    target_link_libraries(server PRIVATE ws2_32)
endif()

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    if(WIN32)
        add_subdirectory(Examples/TrainSimulator)
    endif()
endif()
