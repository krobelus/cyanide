#include <QDebug>
#include <QDnsLookup>
#include <tox/toxdns.h>

#include "util.h"

QByteArray makeJsonRequest(QString url, QString json) {
  QNetworkAccessManager netman;
  QNetworkRequest request{url};
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
  QNetworkReply *reply = netman.post(request, json.toUtf8());

  while (!reply->isFinished()) {
    QThread::msleep(1);
    qApp->processEvents();
  }

  QByteArray result = reply->readAll();
  delete reply;
  return result;
}

bool dns_lookup(uint8_t *dest, QString &address) {
  // JSON injection ?
  address = address.trimmed();
  address.replace('\\', "\\\\");
  address.replace('"', "\"");

  const QString json{"{\"action\":3,\"name\":\"" + address + "\"}"};

  QString apiUrl = "https://" + address.split(QLatin1Char('@')).last() + "/api";
  QByteArray response = makeJsonRequest(apiUrl, json);

  static const QByteArray pattern{"tox_id\""};
  const int index = response.indexOf(pattern);
  if (index == -1)
    return false;

  response = response.mid(index + pattern.size());

  const int idStart = response.indexOf('"');
  if (idStart == -1)
    return false;

  response = response.mid(idStart + 1);

  const int idEnd = response.indexOf('"');
  if (idEnd == -1)
    return false;

  response.truncate(idEnd);

  qDebug() << response;

  return string_to_address(dest, (uint8_t *)response.data());
}
