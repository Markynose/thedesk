include_guard()

function(cntp_init target cxx-standard)
    option(CNTP_ASAN "Enable the use of AddressSanitizer" OFF)

    set_target_properties(${target} PROPERTIES
            AUTOMOC ON
            AUTORCC ON
            AUTOUIC ON
            CXX_STANDARD ${cxx-standard}
            CXX_STANDARD_REQUIRED ON)
            
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS LinguistTools)

    cntp_dotcontemporary_desktopId(${target} DESKTOPID RESOLVE_BLUEPRINT)

    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
    target_compile_definitions(${target} PRIVATE SYSTEM_LIBRARY_DIRECTORY="${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
    target_compile_definitions(${target} PRIVATE SYSTEM_PREFIX_DIRECTORY="${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_PREFIX}")
    target_compile_definitions(${target} PRIVATE SYSTEM_DATA_DIRECTORY="${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}")
    target_compile_definitions(${target} PRIVATE CNTP_TARGET_NAME="${target}")

    cntp_asan(${target})

    if(NOT ${DESKTOPID} STREQUAL "${DESKTOPID}-NOTFOUND")
        add_compile_definitions(T_APPMETA_DESKTOP_ID="${DESKTOPID}")
    endif()

    cntp_dotcontemporary_path(${target} DOTCONTEMPORARY_FILE)
    if(NOT ${DOTCONTEMPORARY_FILE} STREQUAL "DOTCONTEMPORARY_FILE-NOTFOUND")
        configure_file(${DOTCONTEMPORARY_FILE} ".contemporary.json" COPYONLY)
    endif()

    cntp_enable_coroutines(${target})
endfunction()

function(cntp_init_plugin parent target cxx-standard share-subdir)
    if(NOT TARGET ${parent}-plugins)
        add_custom_target(${parent}-plugins ALL)
    endif()

    cntp_init(${target} ${cxx-standard})
    cntp_install_plugin(${parent} ${target})

    cntp_get_target_share_dir_name(SHARE_DIR ${parent})

    target_compile_definitions(${target} PRIVATE CNTP_SHARE_DIR="${share-subdir}")

    set_target_properties(${target} PROPERTIES
            CNTP_DATA_SUBDIR ${SHARE_DIR}/plugins/${share-subdir}
            CNTP_DATA_SUBDIR_WITHOUT_PARENT ${share-subdir}
            CNTP_PARENT_TARGET ${parent})
    add_dependencies(${parent}-plugins ${target})
endfunction()
