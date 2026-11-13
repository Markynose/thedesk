QT += core gui widgets thelib tdesktopenvironment
SHARE_APP_NAME = thedesk/xdg-utils/thedesk-xdg-settings

CONFIG += c++11 console
CONFIG -= app_bundle

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        main.cpp

unix:!macx {
    # Include the-libs build tools
    include(/usr/share/the-libs/pri/buildmaster.pri)

    TARGET = thedesk-xdg-settings

    target.path = /usr/bin

    symlinks.extra = mkdir -p $(INSTALL_ROOT)/usr/lib/thedesk/xdg-utils; ln -s /usr/bin/thedesk-xdg-settings $(INSTALL_ROOT)/usr/lib/thedesk/xdg-utils/xdg-settings
    symlinks.path = /

    INSTALLS += target symlinks
}
