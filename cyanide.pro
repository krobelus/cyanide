# NOTICE:
#
# Application name defined in TARGET has a corresponding QML filename.
# If name defined in TARGET is changed, the following needs to be done
# to match new name:
#   - corresponding QML filename must be changed
#   - desktop icon filename must be changed
#   - desktop filename must be changed
#   - icon definition filename in desktop file must be changed
#   - translation filenames have to be changed

# The name of your application
TARGET = cyanide

CONFIG += sailfishapp

QT += sql dbus multimedia

SOURCES += \
    src/cyanide.cpp \
    src/dns.cpp \
    src/friend.cpp \
    src/settings.cpp \
    src/util.cpp \
    src/av.cpp

OTHER_FILES += \
    cyanide.desktop \
    cyanide.png \
    filesystem/* \
    qml/cover/CoverPage.qml \
    qml/cyanide.qml \
    qml/js/Misc.js \
    qml/pages/AcceptFriend.qml \
    qml/pages/AddFriend.qml \
    qml/pages/FileChooser.qml \
    qml/pages/FriendList.qml \
    qml/pages/Friend.qml \
    qml/pages/Profile.qml \
    qml/pages/Settings.qml \
    rpm/cyanide.changes \
    rpm/cyanide.spec \
    rpm/cyanide.yaml \
    translations/*.ts \
    filesystem/usr/share/dbus-1/services/harbour.cyanide.service \
    qml/pages/FriendAction.qml \
    filesystem/usr/share/lipstick/notificationcategories/harbour.cyanide.call.conf \
    filesystem/usr/share/lipstick/notificationcategories/harbour.cyanide.message.conf


# to disable building translations every time, comment out the
# following CONFIG line
# CONFIG +=  sailfishapp_i18n

TRANSLATIONS += translations/cyanide-de.ts \
                translations/cyanide-ru.ts \
                translations/cyanide-sv.ts \
                translations/cyanide-sl.ts

HEADERS += \
    src/cyanide.h \
    src/tox_bootstrap.h \
    src/unused.h \
    src/util.h \
    src/friend.h \
    src/tox_callbacks.h \
    src/message.h \
    src/settings.h \
    src/config.h

unix:!macx: LIBS += -L$$PWD/res/usr/lib/ \
                -ltoxcore -ltoxdns -ltoxav \
                -lsodium -lopus -lvpx \
                -lrt

INCLUDEPATH += $$PWD/res/usr/include
DEPENDPATH += $$PWD/res/usr/include

QMAKE_CXXFLAGS += "-std=c++0x -pthread"
QMAKE_CXXFLAGS += "-Wno-write-strings"
QMAKE_CXXFLAGS += "-Wno-unused-parameter"
QMAKE_CXXFLAGS += "-Wno-pointer-arith"
#QMAKE_CXXFLAGS += "-Wno-unused-function -Wno-comment

RESOURCES += \
    resources.qrc

unix: PKGCONFIG += mlite5
