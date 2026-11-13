cmake_minimum_required(VERSION 3.5)

project(libcontemporary_tests LANGUAGES CXX)

enable_testing()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Test)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(libcontemporary_tests main.cpp
    tst_trange.cpp tst_trange.h
)
add_test(NAME TestCase1 COMMAND TestCase1)

target_link_libraries(libcontemporary_tests PRIVATE Qt${QT_VERSION_MAJOR}::Test libcontemporary)
target_include_directories(libcontemporary_tests PUBLIC ../lib/)
