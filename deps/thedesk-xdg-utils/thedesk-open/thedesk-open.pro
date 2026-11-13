QT       += core gui thelib tdesktopenvironment
SHARE_APP_NAME = thedesk/xdg-utils/thedesk-open

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    fileassociationselect.cpp \
    main.cpp \
    setdefaultpopover.cpp

HEADERS += \
    fileassociationselect.h \
    setdefaultpopover.h

DISTFILES += \
    associations.conf

FORMS += \
    fileassociationselect.ui \
    setdefaultpopover.ui

unix:!macx {
    # Include the-libs build tools
    include(/usr/share/the-libs/pri/buildmaster.pri)

    TARGET = thedesk-open

    target.path = /usr/bin

    defaults.files = associations.conf
    defaults.path = /etc/thesuite/theDesk

    symlinks.extra = mkdir -p $(INSTALL_ROOT)/usr/lib/thedesk/xdg-utils; ln -s /usr/bin/thedesk-open $(INSTALL_ROOT)/usr/lib/thedesk/xdg-utils/xdg-open
    symlinks.path = /

    INSTALLS += target symlinks defaults
}
