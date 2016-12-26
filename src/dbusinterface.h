#ifndef DBUSINTERFACE_H
#define DBUSINTERFACE_H

#include <QObject>

#include "cyanide.h"

class DBusInterface : public QObject {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "harbour.cyanide")

public:
  Cyanide *cyanide;

  explicit DBusInterface(QObject *parent = 0);

  Q_SCRIPTABLE void message_notification_activated(int fid);

signals:

public slots:
};

#endif // DBUSINTERFACE_H
