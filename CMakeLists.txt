cmake_minimum_required(VERSION 4.3)
project(GhosttyStatus C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(GhosttyStatus src/main.c)

include(CTest)
if(BUILD_TESTING)
    add_executable(component_tests tests/components.c)
    add_test(NAME components COMMAND component_tests)
endif()
