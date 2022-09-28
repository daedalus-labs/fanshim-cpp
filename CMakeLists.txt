cmake_minimum_required(VERSION 3.16.3)
project(fanshim)

if(NOT CMAKE_BUILD_TYPE STREQUAL Debug)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_compile_options(-Wall -Werror -Wpedantic -Weffc++)

find_package(fmt REQUIRED)

include_directories(
    include
    src
)

add_executable(${PROJECT_NAME})

target_sources(
    ${PROJECT_NAME}
    PRIVATE
    src/fanshim/configuration.cpp
    src/fanshim/driver.cpp
    src/fanshim/gpio.cpp
    src/fanshim/logger.cpp

    src/main.cpp
)

target_link_libraries(
    ${PROJECT_NAME}
    ${LIB_NAME}
    fmt::fmt
    stdc++fs
)
