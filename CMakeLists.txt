cmake_minimum_required(VERSION 3.18.0)
project(fanshim)

###############
## CONFIGURE ##
###############

if(NOT CMAKE_BUILD_TYPE STREQUAL Debug)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_compile_options(-Wall -Werror -Wpedantic -Weffc++)

find_package(spdlog REQUIRED)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/data/fanshim-driver.service.in ${CMAKE_CURRENT_BINARY_DIR}/fanshim-driver.service)

###########
## BUILD ##
###########

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
    gpiodcxx
    spdlog::spdlog
    stdc++fs
    uv
)

#############
## INSTALL ##
#############

install(
    TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
    LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
)

install (
    FILES ${CMAKE_CURRENT_BINARY_DIR}/fanshim-driver.service
    DESTINATION /etc/systemd/system/
    PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ
)

##########
## TEST ##
##########
