#ifndef FRIEND_H
#define FRIEND_H

#include <tox/tox.h>
#include "message.h"

class Friend
{

public:
    Friend();
    Friend(const uint8_t *public_key, QString name, QString status_message);

    QString name;
    QString status_message;

    uint8_t public_key[TOX_PUBLIC_KEY_SIZE];
    uint8_t avatar_hash[TOX_HASH_LENGTH];

    TOX_CONNECTION connection_status;
    TOX_USER_STATUS user_status;
    bool accepted;
    bool notification;

    std::vector<Message> messages;


signals:

public slots:

};

#endif // FRIEND_H
