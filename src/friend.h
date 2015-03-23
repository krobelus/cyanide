#ifndef FRIEND_H
#define FRIEND_H

#include <tox/tox.h>
#include "message.h"

class Friend
{

public:
    Friend();
    Friend(const uint8_t *cid, const uint8_t *name, size_t name_length
                             , const uint8_t *status_message, size_t status_message_length);

    uint8_t name[TOX_MAX_NAME_LENGTH];
    size_t name_length;

    // TODO store as QString
    uint8_t status_message[TOX_MAX_STATUS_MESSAGE_LENGTH];
    size_t status_message_length;

    uint8_t cid[TOX_PUBLIC_KEY_SIZE];

    TOX_CONNECTION connection_status;
    TOX_USER_STATUS user_status;
    bool accepted;
    bool notification;

    std::vector<Message> messages;

signals:

public slots:

};

#endif // FRIEND_H
