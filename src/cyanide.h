#ifndef CYANIDE_H
#define CYANIDE_H

#include <tox/tox.h>
#include <QObject>

#include "friend.h"
#include "message.h"

#define DEFAULT_NAME "Tox User"
#define DEFAULT_STATUS "Toxing on cyanide"

class Cyanide : public QObject
{
    Q_OBJECT

private:
    Tox *tox;

    uint8_t self_id[TOX_FRIEND_ADDRESS_SIZE];
    char self_hex_id[2 * TOX_FRIEND_ADDRESS_SIZE];

    char save_path[512];

public:
    bool save_needed;
    std::vector<Friend> friends;
    Friend *self(); // always the last element in friends
    int self_fid();  // friends.size() - 1
    void add_friend(Friend *p);

    Cyanide();

    void load_defaults();
    bool load_save();
    void write_save();
    void set_callbacks();
    void do_bootstrap();

    void load_tox_and_stuff_pretty_please();
    void tox_thread();

    /* */
    Q_INVOKABLE bool send_friend_request(QString id_string, QString msg_string);
    Q_INVOKABLE bool send_friend_message(int fid, QString msg);

    /* setters and getters */
    Q_INVOKABLE void set_friend_notification(int fid, bool status);

    Q_INVOKABLE int get_number_of_friends();
    Q_INVOKABLE QString get_friend_name(int fid);
    Q_INVOKABLE QString get_friend_avatar(int fid);
    Q_INVOKABLE QString get_friend_status_message(int fid);
    Q_INVOKABLE QString get_friend_status_icon(int fid);
    Q_INVOKABLE bool get_friend_connection_status(int fid);

    Q_INVOKABLE int get_number_of_messages(int fid);
    Q_INVOKABLE QString get_message_text(int fid, int mid);
    Q_INVOKABLE QDateTime get_message_timestamp(int fid, int mid);
    Q_INVOKABLE bool get_message_author(int fid, int mid);

signals:
    void signal_friend_request();
    void signal_friend_message(int fid, int mid);
    void signal_friend_action();
    void signal_name_change(int fid);
    void signal_status_message(int fid);
    void signal_user_status();
    void signal_typing_change();
    void signal_read_receipt();
    void signal_connection_status(int fid);
    void signal_avatar_info();
    void signal_avatar_data();
    void signal_group_invite();
    void signal_group_message();
    void signal_group_action();
    void signal_group_namelist_change();
    void signal_group_title();
    void signal_file_send_request();
    void signal_file_control();
    void signal_file_data();
};

void init(Cyanide *cyanide);

#endif // CYANIDE_H
