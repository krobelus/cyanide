#include <QDebug>

#include "friend.h"

Friend::Friend()
{
    connection_status = TOX_CONNECTION_NONE;
    user_status = TOX_USER_STATUS_NONE;
    accepted = true;
    notification = false;
    memset(avatar_hash, 0, TOX_HASH_LENGTH);
}

Friend::Friend(const uint8_t *public_key, QString name, QString status_message)
{
    // TODO use the default constructor for this
    connection_status = TOX_CONNECTION_NONE;
    user_status = TOX_USER_STATUS_NONE;
    accepted = true;
    notification = false;
    memset(avatar_hash, 0, TOX_HASH_LENGTH);

    memcpy(this->public_key, public_key, TOX_PUBLIC_KEY_SIZE);
    this->name = name;
    this->status_message = status_message;
}
