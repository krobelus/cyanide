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

QT += sql dbus

SOURCES += \
    src/cyanide.cpp \
    src/dns.cpp \
    src/friend.cpp \
    src/settings.cpp \
    src/util.cpp \
    src/av.cpp \
    src/filetransfers.cpp \
    src/dbusinterface.cpp \
    src/history.cpp

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
    qml/pages/LoadChatHistory.qml \
    rpm/cyanide.changes \
    rpm/cyanide.spec \
    rpm/cyanide.yaml \
    translations/*.ts \
    qml/pages/FriendAction.qml \
    qml/pages/EnterPassword.qml

# to disable building translations every time, comment out the
# following CONFIG line
CONFIG +=  sailfishapp_i18n

TRANSLATIONS += translations/cyanide-de.ts \
                translations/cyanide-ru.ts \
                translations/cyanide-sv.ts \
                translations/cyanide-sl.ts \
                translations/cyanide-nl.ts \
                translations/cyanide-it.ts \
                translations/cyanide-da.ts \
                translations/cyanide-fr.ts \
                translations/cyanide-cs.ts \
                translations/cyanide-sc.ts \
                translations/cyanide-zh_Hans.ts \
                translations/cyanide-nb.ts \
                translations/cyanide-et.ts \
                translations/cyanide-ro.ts


HEADERS += \
    src/cyanide.h \
    src/tox_bootstrap.h \
    src/unused.h \
    src/util.h \
    src/friend.h \
    src/tox_callbacks.h \
    src/message.h \
    src/settings.h \
    src/dbusinterface.h \
    src/history.h

unix:!macx: LIBS += \
                -ltoxcore -ltoxdns -ltoxav -ltoxencryptsave \
                -lsodium -lopus -lvpx \
                -lrt \
                #-lnemonotifications-qt5

QMAKE_CXXFLAGS += "-std=c++11 -pthread"
QMAKE_CXXFLAGS += "-Wno-unused-parameter"
QMAKE_CXXFLAGS += "-DMLITE=1"

QMAKE_RPATHDIR += /usr/share/cyanide/lib

LIBS.path = /usr/share/cyanide/lib/
LIBS.files += /usr/lib/libsodium.so.13
LIBS.files += /usr/lib/libtoxcore.so.0 /usr/lib/libtoxav.so.0 /usr/lib/libtoxdns.so.0 /usr/lib/libtoxencryptsave.so.0
LIBS.files += /usr/lib/libvpx.so.1 /usr/lib/libvpx.so.1.3 /usr/lib/libvpx.so.1.3.0
LIBS.files += /usr/lib/libopus.so.0 /usr/lib/libopus/libopus.so.0.5.0
INSTALLS += LIBS

FILESYSTEM.path = /usr/share/lipstick/notificationcategories/
FILESYSTEM.files = filesystem/usr/share/lipstick/notificationcategories/harbour.cyanide.call.conf
FILESYSTEM.files += filesystem/usr/share/lipstick/notificationcategories/harbour.cyanide.message.conf
INSTALLS += FILESYSTEM

DBUS.path = /usr/share/dbus-1/services
DBUS.files = filesystem/usr/share/dbus-1/services/harbour.cyanide.service
INSTALLS += DBUS

RESOURCES += \
    resources.qrc

unix: PKGCONFIG += openal
unix: PKGCONFIG += mlite5
