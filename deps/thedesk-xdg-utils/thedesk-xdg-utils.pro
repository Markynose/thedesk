TEMPLATE = subdirs

SUBDIRS += \
    thedesk-open \
    thedesk-xdg-settings

symlinks.extra = mkdir -p $(INSTALL_ROOT)/usr/bin; ln -s /usr/lib/thedesk/xdg-utils $(INSTALL_ROOT)/usr/bin/thedesk-xdg-utils
symlinks.path = /

INSTALLS += symlinks
