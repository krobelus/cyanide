#include <QDebug>

#include "friend.h"

Friend::Friend()
{
    connection_status = TOX_CONNECTION_NONE;
    user_status = TOX_USER_STATUS_NONE;
    accepted = true;
    notification = false;
}

Friend::Friend(const uint8_t *cid, const uint8_t *name, size_t name_length,
               const uint8_t *status_message, size_t status_message_length)
{
    //TODO use the default constructor for this
    connection_status = TOX_CONNECTION_NONE;
    user_status = TOX_USER_STATUS_NONE;
    accepted = true;
    notification = false;

    memcpy(this->cid, cid, TOX_PUBLIC_KEY_SIZE);
    this->name_length = name_length;
    memcpy(this->name, name, name_length);
    this->status_message_length = status_message_length;
    memcpy(this->status_message, status_message, status_message_length);
}
