cmake_minimum_required(VERSION 3.13)

project(winch_control_system LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB WINCH_SRC CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

file(GLOB WINCH_HEADERS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp
)

add_executable(winch_main
    main.cpp
    ${WINCH_SRC}
    ${WINCH_HEADERS}
)

target_include_directories(winch_main
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)
