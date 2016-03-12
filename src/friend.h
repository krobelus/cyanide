#ifndef FRIEND_H
#define FRIEND_H

#include <tox/tox.h>
#include <tox/toxav.h>
#include <mlite5/mnotification.h>

#include "message.h"

class Call_State : public QObject
{
    Q_OBJECT
    Q_ENUMS(_)
public:
    enum _ { Error = TOXAV_FRIEND_CALL_STATE_ERROR
           , Finished = TOXAV_FRIEND_CALL_STATE_FINISHED
           , Sending_A = TOXAV_FRIEND_CALL_STATE_SENDING_A
           , Sending_V = TOXAV_FRIEND_CALL_STATE_SENDING_V
           , Accepting_A = TOXAV_FRIEND_CALL_STATE_ACCEPTING_A
           , Accepting_V = TOXAV_FRIEND_CALL_STATE_ACCEPTING_V
           , Outgoing = 1 << 28
           , Incoming = 1 << 29
           , Ringing = 1 << 30
           , Active = Sending_A | Sending_V | Accepting_A | Accepting_V
           };
    static QStringList display(int s);
};

class Call_Control : public QObject
{
    Q_OBJECT
    Q_ENUMS(_)
public:
    enum _ { Resume = TOXAV_CALL_CONTROL_RESUME
           , Pause = TOXAV_CALL_CONTROL_PAUSE
           , Cancel = TOXAV_CALL_CONTROL_CANCEL
           , Mute_Audio = TOXAV_CALL_CONTROL_MUTE_AUDIO
           , Unmute_Audio = TOXAV_CALL_CONTROL_UNMUTE_AUDIO
           , Hide_Video = TOXAV_CALL_CONTROL_HIDE_VIDEO
           , Show_Video = TOXAV_CALL_CONTROL_SHOW_VIDEO
           };
};

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
    bool needs_avatar;

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
