#ifndef MESSAGE_H
#define MESSAGE_H

#include <QtCore>

class Message
{

public:
    Message(const uint8_t *message, uint16_t length, bool author);
    Message(QString msg, bool author);

    QDateTime timestamp;
    bool author;
    QString text;

signals:

public slots:

};

#endif // MESSAGE_H
