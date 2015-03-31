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
    src/util.cpp \
    src/friend.cpp \
    src/message.cpp \
    src/dns.cpp \
    src/settings.cpp

OTHER_FILES += \
    qml/cover/CoverPage.qml \
    rpm/cyanide.spec \
    rpm/cyanide.yaml \
    translations/*.ts \
    qml/pages/FriendList.qml \
    qml/pages/Profile.qml \
    qml/js/Misc.js \
    qml/pages/Friend.qml \
    qml/cyanide.qml \
    cyanide.desktop \
    qml/pages/AddFriend.qml \
    qml/pages/AcceptFriend.qml \
    cyanide.png \
    rpm/cyanide.changes \
    qml/pages/Settings.qml \
    notificationcategories/x-nemo.social.tox.message.conf \
    qml/pages/FileChooser.qml

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
    src/config.h \
    src/file_transfers.h

unix:!macx: LIBS += -L$$PWD/res/usr/lib/ -ltoxcore -ltoxdns -lsodium -lrt -lresolv -lnemonotifications-qt5

INCLUDEPATH += $$PWD/res/usr/include
DEPENDPATH += $$PWD/res/usr/include

QMAKE_CXXFLAGS += "-std=c++0x -pthread"
QMAKE_CXXFLAGS += "-Wno-write-strings"
QMAKE_CXXFLAGS += "-Wno-unused-parameter"
QMAKE_CXXFLAGS += "-Wno-pointer-arith"
#QMAKE_CXXFLAGS += "-Wno-unused-function -Wno-comment

RESOURCES += \
    resources.qrc
