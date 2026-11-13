cmake_minimum_required(VERSION 3.12.0)
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.15)

project(libcontemporary VERSION 1.0.0 LANGUAGES CXX)

find_package(QT REQUIRED NAMES Qt6)

add_subdirectory(lib)
add_subdirectory(cntp-trcommon)
add_subdirectory(cntp-trdesktop)
add_subdirectory(cntp-trappstream)
add_subdirectory(cntp-appicontool)
add_subdirectory(cntp-skel)

IF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    add_subdirectory(cntp-macdeploy)
    add_subdirectory(cntp-macpack)
ENDIF()

IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    add_subdirectory(cntp-windeploy)
ENDIF()

add_subdirectory(test)
