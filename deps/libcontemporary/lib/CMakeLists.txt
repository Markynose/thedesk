project(lib VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

file(GLOB EXTRA_FUNCTIONS "${CMAKE_CURRENT_LIST_DIR}/cmake-scripts/*.cmake")
foreach(f ${EXTRA_FUNCTIONS})
    include(${f})
endforeach()

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets Svg Network Multimedia PrintSupport Concurrent LinguistTools OPTIONAL_COMPONENTS DBus)
find_package(QCoro${QT_VERSION_MAJOR} COMPONENTS Core Network OPTIONAL_COMPONENTS DBus)
find_library(CF_FRAMEWORK CoreFoundation)
find_library(COCOA_FRAMEWORK Cocoa)


if(NOT ${QCoro${QT_VERSION_MAJOR}Core_FOUND})
    message(FATAL_ERROR "QCoro6 library not found")
ENDIF()

cntp_find_pkgconfig(Unwind libunwind FAIL_ON_ANDROID)
cntp_find_pkgconfig(X11 x11)
#cntp_find_pkgconfig(GsettingsQt gsettings-qt)

add_compile_definitions(SYSTEM_LIBRARY_DIRECTORY="${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")

cntp_sourcelist(
    OUTPUT SOURCES
    OUTPUT_ALL ALL_SOURCES
    FILES
        taboutdialog.cpp
        tapplication.cpp
        tcircularspinner.cpp
        tconditionalwidget.cpp
        tcsdtools.cpp
        tdatetimepicker.cpp
        terrorflash.cpp
        libcontemporary_global.cpp
        thelpmenu.cpp
        ticon.cpp
        tinputdialog.cpp
        tjob.cpp
        tstandardjob.cpp
        tjobmanager.cpp
        tlocale.cpp
        tlogger.cpp
        tmessagebox.cpp
        tpaintcalculator.cpp
        tpopover.cpp
        tpromise.cpp
        tpainter.cpp
        tprintpopover.cpp
        tpropertyanimation.cpp
        tscrim.cpp
        tsettings.cpp
        tshortcuthud.cpp
        tstackedwidget.cpp
        tstatusframe.cpp
        tstylemanager.cpp
        tswitch.cpp
        tsystemsound.cpp
        tstandardlabel.cpp
        tsubtitlelabel.cpp
        ttitlelabel.cpp
        ttoast.cpp
        tvariantanimation.cpp
        tvirtualkeyboard.cpp
        twindowtabber.cpp
        twindowtabberbutton.cpp
        twindowthumbnail.cpp
        texception.cpp
        tqueueguard.cpp
        ranges/trange.cpp
        tx11info.cpp
        tcontentsizer.cpp
        private/debuglogpopover.cpp
        private/debuglogwindow.cpp
        private/nativeeventfilter.cpp
        private/translatorproxy.cpp
        tcsdtools/csdbuttonbox.cpp
        tcsdtools/csdsizegrip.cpp
        tdatetimepicker/datetimepart.cpp
        tdatetimepicker/datetimepartbutton.cpp
        tnotification/tnotification-common.cpp
        jobs/jobbutton.cpp
        jobs/jobspopover.cpp
        jobs/standardjobprogresswidget.cpp jobs/standardjobprogresswidget.ui
        private/errorflashwidget.cpp
        private/errorflashwidget.ui
        plugins/tpluginmanagerpane.cpp plugins/tpluginmanagerpane.ui
        plugins/tpluginmanagerhelper.cpp
        plugins/pluginmanagementpopover.cpp plugins/pluginmanagementpopover.ui
        plugins/knownpluginmodel.cpp

        tcommandpalette/tcommandpalettewindow.cpp tcommandpalette/tcommandpalettewindow.ui
        tcommandpalette/tcommandpalettecontroller.cpp
        tcommandpalette/tcommandpalettescope.cpp
        tcommandpalette/tcommandpaletteactionscope.cpp
        tcommandpalette/tcommandpaletteallscope.cpp
        tcommandpalette/tcommandpalettecompositescope.cpp
        tcommandpalette/tcommandpalettedocumentspecificscope.cpp

        tsettingswindow/tsettingswindow.cpp tsettingswindow/tsettingswindow.ui
        tsettingswindow/tsettingspane.cpp
        tsettingswindow/tsettingswindowdelegate.cpp

        touchbar/ttouchbar.cpp
        touchbar/ttouchbarabstractitem.cpp
        touchbar/ttouchbarcustomtouchbaritem.cpp
        touchbar/ttouchbarbuttonitem.cpp
        touchbar/ttouchbarflexiblespaceitem.cpp
        touchbar/ttouchbarpopoveritem.cpp
        touchbar/ttouchbarsegmentedcontrolitem.cpp
        touchbar/ttouchbarlabelitem.cpp
        touchbar/tcompositetouchbar.cpp
        touchbar/ttouchbaractionitem.cpp
        touchbar/ttouchbargroupitem.cpp

        taboutdialog.ui
        tinputdialog.ui
        tshortcuthud.ui
        tstatusframe.ui
        tprintpopover.ui
        private/debuglogpopover.ui
        private/debuglogwindow.ui

        private/licensetextpopover.cpp private/licensetextpopover.ui
        private/tqueueguardhelper.cpp

        private/tstackedwidgetanimationcontroller.cpp
        private/tstackedwidgetanimation.cpp
        private/tstackedwidgetanimationslidehorizontal.cpp
        private/tstackedwidgetanimationlift.cpp
        private/tstackedwidgetanimationfade.cpp
        private/tstackedwidgetanimationslidevertical.cpp
        private/tstackedwidgetanimationslide.cpp
        touchbar/ttouchbardialogalertitem.cpp

        thelibs_icons.qrc

        private/tmessageboxbackend.h
)

cntp_sourcelist(
    OUTPUT HEADERS
    OUTPUT_ALL ALL_HEADERS
    FILES
        taboutdialog.h
        tapplication.h
        tcircularspinner.h
        tconditionalwidget.h
        tcsdtools.h
        tdatetimepicker.h
        terrorflash.h
        libcontemporary_global.h
        thelpmenu.h
        ticon.h
        tinputdialog.h
        tjob.h
        tstandardjob.h
        tjobmanager.h
        tlocale.h
        tlogger.h
        tmessagebox.h
        tnotification.h
        torderedmap.h
        tpaintcalculator.h
        tpopover.h
        tpromise.h
        tprintpopover.h
        tpainter.h
        tpropertyanimation.h
        tscrim.h
        tsettings.h
        tshortcuthud.h
        tstackedwidget.h
        tstatusframe.h
        tstylemanager.h
        tswitch.h
        tsystemsound.h
        ttitlelabel.h
        ttoast.h
        tvariantanimation.h
        tvirtualkeyboard.h
        twindowtabber.h
        twindowtabberbutton.h
        twindowthumbnail.h
        tx11info.h
        tstandardlabel.h
        tsubtitlelabel.h
        tcontentsizer.h
        texception.h
        tqueueguard.h
        tsysconfigdetect.h
        ranges/trange.h
        plugins/plugininterface.h
        plugins/tpluginmanager.h
        plugins/tpluginmanagerpane.h
        plugins/tpluginmanagerhelper.h

        touchbar/ttouchbar.h
        touchbar/ttouchbarabstractitem.h
        touchbar/ttouchbarcustomtouchbaritem.h
        touchbar/ttouchbarbuttonitem.h
        touchbar/ttouchbarflexiblespaceitem.h
        touchbar/ttouchbarpopoveritem.h
        touchbar/ttouchbarsegmentedcontrolitem.h
        touchbar/ttouchbarlabelitem.h
        touchbar/tcompositetouchbar.h
        touchbar/ttouchbargroupitem.h
        touchbar/ttouchbardialogalertitem.h
        touchbar/ttouchbaractionitem.h
        touchbar/ttouchbar_forwarddeclares.h

        tsettingswindow/tsettingswindow.h
        tsettingswindow/tsettingspane.h

        tcommandpalette/tcommandpalettecontroller.h
        tcommandpalette/tcommandpalettescope.h
        tcommandpalette/tcommandpaletteactionscope.h
        tcommandpalette/tcommandpalettecompositescope.h
        tcommandpalette/tcommandpalettedocumentspecificscope.h
    )

cntp_sourcelist(
    OUTPUT PRIVATE_HEADERS
    OUTPUT_ALL ALL_PRIVATE_HEADERS
    FILES
        private/errorflashwidget.h
        tsettingswindow/tsettingswindowdelegate.h
        tcommandpalette/tcommandpalettewindow.h
        tcommandpalette/tcommandpaletteallscope.h
        plugins/pluginmanagementpopover.h
        plugins/knownpluginmodel.h
        private/licensetextpopover.h
        jobs/standardjobprogresswidget.h
        private/tqueueguardhelper.h

        touchbar/private/ttouchbar_p.h
        touchbar/private/ttouchbarbuttonitem_p.h
        touchbar/private/ttouchbarcustomtouchbaritem_p.h
        touchbar/private/ttouchbarpopoveritem_p.h
        touchbar/private/ttouchbarsegmentedcontrolitem_p.h
        touchbar/private/ttouchbarlabelitem_p.h
        touchbar/private/ttouchbargroupitem_p.h

        private/tstackedwidgetanimationcontroller.h
        private/tstackedwidgetanimation.h
        private/tstackedwidgetanimationslidehorizontal.h
        private/tstackedwidgetanimationlift.h
        private/tstackedwidgetanimationfade.h
        private/tstackedwidgetanimationslidevertical.h
        private/tstackedwidgetanimationslide.h
)


if(${Qt${QT_VERSION_MAJOR}DBus_FOUND} AND ${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    qt_generate_dbus_interface(jobs/jobdbusmanager.h)
    qt_generate_dbus_interface(jobs/jobdbus.h)
    qt_add_dbus_adaptor(SOURCES ${CMAKE_CURRENT_BINARY_DIR}/jobdbusmanager.xml jobs/jobdbusmanager.h JobDbusManager manageradaptor)
    qt_add_dbus_adaptor(SOURCES ${CMAKE_CURRENT_BINARY_DIR}/jobdbus.xml jobs/jobdbus.h JobDbus jobadaptor)
endif()

cntp_sourcelist(
    OUTPUT SOURCES
    OUTPUT_ALL ALL_SOURCES
    TARGET_SYSTEM_NAME "Darwin"
    FILES
        tcsdtools/csdbuttonbox-objc.mm
        tcsdtools/tcsdtools-objc.mm
        tnotification/tnotification-mac.mm
        private/nsalertmac.mm
        private/tmessageboxbackend-mac.mm
        private/tapplicationmacprivate.mm
        touchbar/ttouchbar_objc.mm
        touchbar/ttouchbarcustomtouchbaritem_objc.mm
        touchbar/ttouchbarbuttonitem_objc.mm
        touchbar/ttouchbarflexiblespaceitem_objc.mm
        touchbar/ttouchbarpopoveritem_objc.mm
        touchbar/ttouchbarsegmentedcontrolitem_objc.mm
        touchbar/ttouchbarlabelitem_objc.mm
        touchbar/ttouchbargroupitem_objc.mm
)

cntp_sourcelist(
    OUTPUT HEADERS
    OUTPUT_ALL ALL_HEADERS
    TARGET_SYSTEM_NAME "Darwin"
    FILES
        private/tapplicationmacprivate.h
)

cntp_sourcelist(
    OUTPUT SOURCES
    OUTPUT_ALL ALL_SOURCES
    TARGET_SYSTEM_NAME "Linux"
    FILES
        tnotification/tnotification-linux.cpp
        jobs/jobdbus.cpp
        jobs/jobdbusmanager.cpp
        private/tmessageboxbackend-qt.cpp
)

cntp_sourcelist(
    OUTPUT SOURCES
    OUTPUT_ALL ALL_SOURCES
    TARGET_SYSTEM_NAME "Windows"
    FILES
        tnotification/tnotification-win.cpp
        private/tmessageboxbackend-win.cpp
        private/tmessageboxbackend-win.h
        private/twindowthumbnail-win.cpp
        private/twindowthumbnail-win.h
)

cntp_sourcelist(
    OUTPUT SOURCES
    OUTPUT_ALL ALL_SOURCES
    TARGET_SYSTEM_NAME "Android"
    FILES
        tnotification/tnotification-android.cpp
        private/tmessageboxbackend-qt.cpp
)

add_library(libcontemporary SHARED ${SOURCES} ${HEADERS} ${PRIVATE_HEADERS})
cntp_init(libcontemporary 20)
cntp_target_public_headers(libcontemporary
        DESTINATION_DIRECTORY libcontemporary
        HEADERS ${HEADERS})
set_target_properties(libcontemporary PROPERTIES
        FRAMEWORK TRUE
        MACOSX_FRAMEWORK_IDENTIFIER com.vicr123.libcontemporary
        VERSION 6.0.1
        TRANSLATION_SOURCES "${ALL_SOURCES};${ALL_HEADERS}"
    )
cntp_enable_coroutines(libcontemporary)

target_link_libraries(libcontemporary Qt::Widgets Qt::Svg Qt::Network Qt::Multimedia Qt::Concurrent Qt::PrintSupport QCoro::Coro QCoro::Core QCoro::Network)
target_compile_definitions(libcontemporary PRIVATE LIBCONTEMPORARY_LIBRARY)

if(${Qt${QT_VERSION_MAJOR}DBus_FOUND})
    target_link_libraries(libcontemporary Qt6::DBus)
    target_compile_definitions(libcontemporary PRIVATE HAVE_QT_DBUS)
ENDIF()

if(${QCoro${QT_VERSION_MAJOR}DBus_FOUND})
    target_link_libraries(libcontemporary QCoro::DBus)
ENDIF()

IF(${Unwind_FOUND})
    target_link_libraries(libcontemporary PkgConfig::Unwind)
    target_compile_definitions(libcontemporary PRIVATE HAVE_LIBUNWIND)
ENDIF()

IF(${X11_FOUND})
    IF (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
        target_link_libraries(libcontemporary PkgConfig::X11)
        target_compile_definitions(libcontemporary PRIVATE HAVE_X11)
    ENDIF()
ENDIF()

#IF(${GsettingsQt_FOUND})
#    target_link_libraries(libcontemporary PkgConfig::GsettingsQt)
#    target_compile_definitions(libcontemporary PRIVATE HAVE_GSETTINGS)
#ENDIF()

IF(CF_FRAMEWORK_FOUND)
    target_link_libraries(libcontemporary CF_FRAMEWORK COCOA_FRAMEWORK)
ENDIF()

IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    target_link_libraries(libcontemporary User32 Kernel32 Comctl32 DbgHelp windowsapp Dwmapi)
ENDIF()

IF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    get_target_property(LIBCONTEMPORARY_VERSION libcontemporary VERSION)

    set(CMAKECONFIG_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR}/libcontemporary.framework/Resources/CMake)
ELSE()
    set(CMAKECONFIG_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/libcontemporary)
ENDIF()

cntp_translate(libcontemporary OUTPUT QM_FILES)
qt_add_resources(libcontemporary "thelibs_translations"
    PREFIX "/the-libs/translations"
    BASE "${CMAKE_CURRENT_BINARY_DIR}"
    FILES "${QM_FILES}"
)

file(GLOB LICENSE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/licenses/*")
qt_add_resources(libcontemporary "libcontemporary_licenses"
    PREFIX "/the-libs/licenses"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/licenses"
    FILES "${LICENSE_FILES}"
)

install(TARGETS libcontemporary
        EXPORT libcontemporaryTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${libcontemporary_INCLUDE_DIR}
        FRAMEWORK DESTINATION ${CMAKE_INSTALL_LIBDIR})

configure_package_config_file(libcontemporaryConfig.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/libcontemporaryConfig.cmake
        INSTALL_DESTINATION ${CMAKECONFIG_INSTALL_DIR})

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libcontemporaryConfig.cmake
        DESTINATION ${CMAKECONFIG_INSTALL_DIR})
install(DIRECTORY cmake-scripts
        DESTINATION ${CMAKECONFIG_INSTALL_DIR})
install(EXPORT libcontemporaryTargets
        FILE libcontemporaryTargets.cmake
        DESTINATION ${CMAKECONFIG_INSTALL_DIR})
