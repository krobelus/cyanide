#include <time.h>

#include "message.h"
#include "util.h"

Message::Message(const uint8_t *message, uint16_t length, bool from_me)
{
    author = from_me;
    text = utf8_to_qstr(message, length);
    timestamp = QDateTime::currentDateTime();
}

Message::Message(QString msg, bool from_me)
{
    author = from_me;
    text = msg;
    timestamp = QDateTime::currentDateTime();
}
