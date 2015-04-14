#ifndef FRIEND_H
#define FRIEND_H

#include <tox/tox.h>
#include <mlite5/mnotification.h>

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
    bool activity;
    bool blocked;

    File_Transfer avatar_transfer;

    /* maps file_number to mid (message id)
     * mid == -1 is outgoing avatar
     * mid == -2 is incoming avatar
     */
    std::map<uint32_t, int> files;

    QList<Message> messages;

    MNotification *notification;

signals:

public slots:

};

#endif // FRIEND_H
