cmake_minimum_required(VERSION 3.21.0)

project(thedesk-open VERSION 1.0.0 LANGUAGES CXX)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
find_package(libcontemporary REQUIRED)
find_package(libtdesktopenvironment REQUIRED)

set(SOURCES
    fileassociationselect.cpp fileassociationselect.ui
    main.cpp
    setdefaultpopover.cpp setdefaultpopover.ui
)

set(HEADERS
    fileassociationselect.h
    setdefaultpopover.h
)

add_executable(thedesk-open ${SOURCES} ${HEADERS})

set_target_properties(thedesk-open PROPERTIES
    CNTP_DATA_SUBDIR thedesk/xdg-utils/thedesk-open
)

cntp_init(thedesk-open 20)
cntp_translate(thedesk-open)
cntp_target_name(thedesk-open "thedesk-open")

target_link_libraries(thedesk-open Qt::Widgets libcontemporary libtdesktopenvironment)

install(TARGETS thedesk-open
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(FILES associations.conf
    DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/thesuite/theDesk/
)

link_xdg_util(thedesk-open xdg-open)
