project(locker VERSION 1.0.0 LANGUAGES CXX)

include(GNUInstallDirs)

find_library(PAM_LIBRARY pam REQUIRED)
find_path(PAM_INCLUDE_DIR security/pam_appl.h REQUIRED)

set(SOURCES
        main.cpp
    )

add_executable(locker-checker ${SOURCES} ${HEADERS})
cntp_target_name(locker-checker "td-locker-checker")

target_include_directories(locker-checker PRIVATE ${PAM_INCLUDE_DIR})
target_link_libraries(locker-checker ${PAM_LIBRARY})

# Not setuid: authentication goes through PAM (pam_unix uses its own setuid
# helper to verify the calling user's password).
install(TARGETS locker-checker
        RUNTIME DESTINATION ${CMAKE_INSTALL_LIBDIR}
        BUNDLE DESTINATION /Applications)

install(FILES thedesk-locker.pam
        DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/pam.d
        RENAME thedesk-locker)
