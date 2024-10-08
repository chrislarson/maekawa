cmake_minimum_required(VERSION 3.22)

project(DistributedSystemSockets
        VERSION 1.0.0
        DESCRIPTION "An implementation of Maekawa's algorithm for mutual exclusion in distributed systems using sockets."
        LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(LIB_NAME lib)
set(APP_NAME app)

add_subdirectory(lib)
add_subdirectory(app)

