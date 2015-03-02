#include <QDebug>

#include "friend.h"

Friend::Friend()
{
    connection_status = false;
    notification = false;
    user_status = TOX_USERSTATUS_NONE;
}
