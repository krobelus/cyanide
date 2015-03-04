#include <QDebug>

#include "friend.h"

Friend::Friend()
{
    connection_status = false;
    notification = false;
    user_status = TOX_USERSTATUS_NONE;
    accepted = true;
}

Friend::Friend(const uint8_t *cid, const uint8_t *name, uint32_t name_length
                         , const uint8_t *status_message, uint32_t status_message_length)
{
    //TODO use the default constructor for this
    connection_status = false;
    notification = false;
    user_status = TOX_USERSTATUS_NONE;
    accepted = true;

    memcpy(this->cid, cid, TOX_CLIENT_ID_SIZE);
    this->name_length = name_length;
    memcpy(this->name, name, name_length);
    this->status_message_length = status_message_length;
    memcpy(this->status_message, status_message, status_message_length);
}
