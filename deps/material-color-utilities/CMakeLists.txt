# Google's material-color-utilities (C++), vendored for theDesk's Material 3
# dynamic colour. Built as a static, position-independent library and linked
# into theDesk's own targets; nothing from here is installed.
cmake_minimum_required(VERSION 3.16)

file(GLOB MCU_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/cpp/*/*.cc)

add_library(material-color-utilities STATIC ${MCU_SOURCES})
set_target_properties(material-color-utilities PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON)
target_compile_features(material-color-utilities PUBLIC cxx_std_20)
# Sources include each other as "cpp/<module>/<file>.h"
target_include_directories(material-color-utilities PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
