#include "dbusinterface.h"

#include <QtDBus>

DBusInterface::DBusInterface(QObject *parent) : QObject(parent) {}

void DBusInterface::message_notification_activated(int fid) {
  cyanide->on_message_notification_activated(fid);
}
