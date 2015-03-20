#ifndef FRIEND_H
#define FRIEND_H

#include <tox/tox.h>
#include "message.h"

class Friend
{

public:
    Friend();
    Friend(const uint8_t *cid, const uint8_t *name, uint32_t name_length
                             , const uint8_t *status_message, uint32_t status_message_length);

    uint8_t name[TOX_MAX_NAME_LENGTH];
    int name_length;

    // TODO store as QString
    uint8_t status_message[TOX_MAX_STATUSMESSAGE_LENGTH];
    int status_message_length;

    uint8_t cid[TOX_PUBLIC_KEY_SIZE];

    bool connection_status;
    bool notification;
    bool accepted;
    uint8_t user_status;

    std::vector<Message> messages;

signals:

public slots:

};

#endif // FRIEND_H
