#ifndef FRIEND_H
#define FRIEND_H

#include <tox/tox.h>
#include "message.h"

class Friend
{

public:
    Friend();

    uint8_t name[TOX_MAX_NAME_LENGTH];
    int name_length;

    // TODO malloc/free this manually
    uint8_t status_message[TOX_MAX_STATUSMESSAGE_LENGTH];
    int status_message_length;

    uint8_t cid[TOX_PUBLIC_KEY_SIZE];

    bool connection_status;
    bool notification;
    uint8_t user_status;

    std::vector<Message> messages;

signals:

public slots:

};

#endif // FRIEND_H
