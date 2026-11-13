# Try the living-type loading screen in a window (not installed):
#   build/thedesk/tools/splash-preview/splash-preview [--field] [--fullscreen]
# Keys: 1 collage, 2 field, S slow motion, E exit, R restart, Esc quit
cmake_minimum_required(VERSION 3.24.0)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
add_executable(splash-preview main.cpp)
set_target_properties(splash-preview PROPERTIES AUTOMOC ON)
target_include_directories(splash-preview PRIVATE ../../libthedesk)
target_link_libraries(splash-preview libthedesk thedesk-m3 Qt::Widgets)
