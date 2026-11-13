include_guard()

function(cntp_find_pkgconfig targetName packageName)
    set(options REQUIRED FAIL_ON_ANDROID)
    set(multiValueArgs WIN_FALLBACK_DLL WIN_FALLBACK_IMPLIB WIN_FALLBACK_INCLUDE)
    cmake_parse_arguments(FIND_PKGCONFIG "${options}" "" "${multiValueArgs}" ${ARGN})

    IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
        IF(${targetName}_FOUND)
            RETURN()
        ENDIF()

        IF(EXISTS ${FIND_PKGCONFIG_WIN_FALLBACK_DLL})
            add_library(PkgConfig::${targetName} SHARED IMPORTED)
            set_target_properties(PkgConfig::${targetName} PROPERTIES
                IMPORTED_LOCATION ${FIND_PKGCONFIG_WIN_FALLBACK_DLL}
                IMPORTED_IMPLIB ${FIND_PKGCONFIG_WIN_FALLBACK_IMPLIB})
            target_include_directories(PkgConfig::${targetName} INTERFACE
                ${FIND_PKGCONFIG_WIN_FALLBACK_INCLUDE})
            set(${targetName}_FOUND 1 PARENT_SCOPE)
        ENDIF()
    ELSEIF(${CMAKE_SYSTEM_NAME} MATCHES "Android" AND FIND_PKGCONFIG_FAIL_ON_ANDROID)
    ELSE()
        IF(FIND_PKGCONFIG_REQUIRED)
            find_package(PkgConfig REQUIRED)
            pkg_check_modules(${targetName} IMPORTED_TARGET REQUIRED ${packageName})
        ELSE()
            find_package(PkgConfig)
            IF(PkgConfig_FOUND)
                pkg_check_modules(${targetName} IMPORTED_TARGET ${packageName})
            ENDIF()
        ENDIF()
    ENDIF()
endfunction()
