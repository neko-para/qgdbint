cmake_minimum_required(VERSION 3.6)
project(qgdbint VERSION 1.0.0 DESCRIPTION "gdb mi parser based on qt")

find_package(Qt5 COMPONENTS Core REQUIRED)
set(CMAKE_AUTOMOC ON)

add_library(qgdbint STATIC "qgdbint.h" "record.h" "qgdbint.cpp" "record.cpp")
target_link_libraries(qgdbint PRIVATE Qt5::Core)

add_executable(qgdbintTest "test.cpp")
target_link_libraries(qgdbintTest PRIVATE Qt5::Core qgdbint)
