#ifndef CYANIDE_H
#define CYANIDE_H

#include <QObject>
#include <QtQuick>
#include <tox/tox.h>

#include "friend.h"
#include "message.h"
#include "file_transfers.h"

class Cyanide : public QObject
{
    Q_OBJECT

private:
    uint8_t self_address[TOX_ADDRESS_SIZE];

    QString send_friend_request_id(const uint8_t *id, const uint8_t *msg, size_t msg_length);
    QString send_friend_request_unboxed(char *name, size_t length, char *msg, size_t msg_length);
    bool send_file_control(File_Transfer *ft, TOX_FILE_CONTROL file_control);

public:
    Tox *tox;
    char save_path[TOX_MAX_FILENAME_LENGTH];
    QQuickView *view;
    Friend self;
    static const int SELF_FRIEND_NUMBER = -1;
    std::map<uint32_t, Friend> friends;
    std::map<uint64_t, File_Transfer> file_transfers;
    bool run_tox_loop, save_needed;
    uint32_t add_friend(Friend *f);
    void add_file_transfer(File_Transfer *ft);
    void remove_file_transfer(File_Transfer *ft);
    File_Transfer *get_file_transfer(uint32_t friend_number, uint32_t file_number);
    bool get_file_id(File_Transfer *ft);

    void load_tox_and_stuff_pretty_please();
    /* reads the tox save file into memory and stores the length in *size */
    const uint8_t *get_save_data(size_t *size);
    /* loads default name, status, ... */
    void load_defaults();
    /* load name, status, friends from the tox object */
    void load_tox_data();
    void write_save();
    void set_callbacks();
    void do_bootstrap();

    void tox_thread();

    void send_new_avatar();

    /* */
    Q_INVOKABLE QString send_friend_request(QString id_string, QString msg_string);
    Q_INVOKABLE void send_typing_notification(int fid, bool typing);
    Q_INVOKABLE bool send_friend_message(int fid, QString msg);
    Q_INVOKABLE bool accept_friend_request(int fid);
    Q_INVOKABLE void remove_friend(int fid);
    Q_INVOKABLE void play_sound(QString file);
    Q_INVOKABLE void raise();
    Q_INVOKABLE bool is_visible();
    Q_INVOKABLE void visibility_changed(QWindow::Visibility visibility);
    Q_INVOKABLE bool resume_transfer(File_Transfer *ft);
    Q_INVOKABLE bool pause_transfer(File_Transfer *ft);
    Q_INVOKABLE bool cancel_transfer(File_Transfer *ft);
    Q_INVOKABLE QString send_file(int fid, QString path);
    Q_INVOKABLE QString send_avatar(int fid);
    Q_INVOKABLE QString send_file(TOX_FILE_KIND kind, int fid, QString path, uint8_t *file_id);

    /* setters and getters */
    Q_INVOKABLE QList<int> get_friend_numbers();
    Q_INVOKABLE void set_friend_notification(int fid, bool status);
    Q_INVOKABLE void set_self_name(QString name);
    Q_INVOKABLE void set_self_status_message(QString status_message);
    Q_INVOKABLE void set_self_user_status(int status);
    Q_INVOKABLE QString set_self_avatar(QString path);

    Q_INVOKABLE QString get_self_address();
    Q_INVOKABLE int get_self_user_status();
    Q_INVOKABLE QString get_friend_public_key(int fid);
    Q_INVOKABLE int get_number_of_friends();
    Q_INVOKABLE QString get_friend_name(int fid);
    Q_INVOKABLE QString get_friend_avatar(int fid);
    Q_INVOKABLE QString get_friend_status_message(int fid);
    Q_INVOKABLE QString get_friend_status_icon(int fid);
    Q_INVOKABLE bool get_friend_connection_status(int fid);
    Q_INVOKABLE bool get_friend_accepted(int fid);

    Q_INVOKABLE int get_number_of_messages(int fid);
    Q_INVOKABLE QString get_message_text(int fid, int mid);
    Q_INVOKABLE QString get_message_rich_text(int fid, int mid);
    Q_INVOKABLE QDateTime get_message_timestamp(int fid, int mid);
    Q_INVOKABLE bool get_message_author(int fid, int mid);

signals:
    void signal_friend_added(int fid);
    void signal_notification(int fid);
    void signal_close_notifications();

    void signal_friend_request(int fid);
    void signal_friend_message(int fid, int mid);
    void signal_friend_action();
    void signal_friend_name(int fid, QString previous_name);
    void signal_friend_status_message(int fid);
    void signal_friend_status(int fid);
    void signal_friend_typing(int fid, bool is_typing);
    void signal_friend_read_receipt();
    void signal_friend_connection_status(int fid);
    void signal_avatar_change(int fid);
};

void start_tox_thread(Cyanide *cyanide);

#endif // CYANIDE_H
