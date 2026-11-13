cmake_minimum_required(VERSION 3.21.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
include(GNUInstallDirs)

set(BUILD_SHARED_LIBS OFF)

find_package(Qt6 REQUIRED COMPONENTS Core Network)

set(SOURCES
        main.cpp
        deployfolder.cpp
        common.cpp
        systemlibrarydatabase.cpp
        unzip.cpp
        portableexecutablefile.cpp)

set(HEADERS
        deployfolder.h
        common.h
        systemlibrarydatabase.h
        unzip.h
        portableexecutablefile.h
        enums.h)

add_executable(cntp-windeploy ${SOURCES} ${HEADERS})

target_link_libraries(cntp-windeploy libcontemporary Qt6::Network)

install(TARGETS cntp-windeploy
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
