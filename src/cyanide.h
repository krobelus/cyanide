#ifndef CYANIDE_H
#define CYANIDE_H

#include <QObject>
#include <QtQuick>
#include <QtDBus>
#include <tox/tox.h>
#include <tox/toxav.h>

#include "config.h"
#include "friend.h"
#include "message.h"

class Cyanide : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "harbour.cyanide")

private:
    uint8_t self_address[TOX_ADDRESS_SIZE];

    void kill_tox();
    void killall_tox();
    QString send_friend_request_id(const uint8_t *id, const uint8_t *msg, size_t msg_length);
    QString send_friend_request_unboxed(char *name, size_t length, char *msg, size_t msg_length);
    QString send_file_control(int fid, int mid, TOX_FILE_CONTROL file_control);

public:
    Tox *tox;
    ToxAv *toxav;

    QString profile_name, next_profile_name;

    /* get the tox save file based on profile_name */
    QString tox_save_file();
    QString tox_save_file(QString name);

    QQuickView *view;
    int eventfd;

    Friend self;
    static const int SELF_FRIEND_NUMBER = -1;

    std::map<uint32_t, Friend> friends;

    bool save_needed;
    enum LOOP_STATE loop;

    uint32_t add_friend(Friend *f);
    uint32_t next_friend_number();
    uint32_t next_but_one_unoccupied_friend_number();
    void add_message(uint32_t fid, Message message);
    void incoming_avatar(uint32_t fid, uint32_t file_number, uint64_t file_size,
                         const uint8_t *filename, size_t filename_length);
    void incoming_avatar_chunk(uint32_t fid, uint64_t position,
                               const uint8_t *data, size_t length);
    bool get_file_id(uint32_t fid, File_Transfer *ft);

    /* read the name of the profile (profile_name) to load from DEFAULT_PROFILE_FILE */
    void read_default_profile(QStringList args);
    void write_default_profile();

    void load_tox_and_stuff_pretty_please();
    /* reads the tox save file into memory and stores the length in *size */
    const uint8_t *get_save_data(size_t *size);
    /* loads default name, status, ... */
    void load_defaults();
    /* load name, status, friends from the tox object */
    void load_tox_data();
    void write_save();
    void set_callbacks();
    void set_av_callbacks();
    void do_bootstrap();

    void tox_thread();
    void tox_loop();

    void suspend_thread();
    void resume_thread();

    void relocate_blocked_friend();
    void send_new_avatar();
    QString send_file(TOX_FILE_KIND kind, int fid, QString path, uint8_t *file_id);
    void wifi_monitor();

    Q_SCRIPTABLE void message_notification_activated(int fid);

    Q_INVOKABLE void raise();
    Q_INVOKABLE bool is_visible();
    Q_INVOKABLE void visibility_changed(QWindow::Visibility visibility);
    Q_INVOKABLE void notify_error(QString summary, QString body);
    Q_INVOKABLE void notify_message(int fid, QString summary, QString body);
    Q_INVOKABLE void check_wifi();

    Q_INVOKABLE void reload();
    Q_INVOKABLE void load_new_profile();
    Q_INVOKABLE void load_tox_save_file(QString path);
    Q_INVOKABLE void delete_current_profile();

    Q_INVOKABLE QString send_friend_request(QString id_string, QString msg_string);
    Q_INVOKABLE bool accept_friend_request(int fid);
    Q_INVOKABLE void remove_friend(int fid);
    Q_INVOKABLE QString send_friend_message(int fid, QString msg);
    Q_INVOKABLE void send_typing_notification(int fid, bool typing);
    Q_INVOKABLE QString send_avatar(int fid);
    Q_INVOKABLE QString send_file(int fid, QString path);
    Q_INVOKABLE QString resume_transfer(int mid, int fid);
    Q_INVOKABLE QString pause_transfer(int mid, int fid);
    Q_INVOKABLE QString cancel_transfer(int mid, int fid);

    /* setters and getters */
    Q_INVOKABLE QString get_profile_name();
    Q_INVOKABLE QString set_profile_name(QString name);

    Q_INVOKABLE QList<int> get_friend_numbers(); /* friend list ordering goes here */
    Q_INVOKABLE QList<int> get_message_numbers(int fid);

    Q_INVOKABLE void set_friend_activity(int fid, bool status);
    Q_INVOKABLE void set_friend_blocked(int fid, bool block);
    Q_INVOKABLE void set_self_name(QString name);
    Q_INVOKABLE void set_self_status_message(QString status_message);
    Q_INVOKABLE void set_self_user_status(int status);
    Q_INVOKABLE QString set_self_avatar(QString path);

    Q_INVOKABLE QString get_self_address();
    Q_INVOKABLE int get_self_user_status();

    Q_INVOKABLE QString get_friend_public_key(int fid);
    Q_INVOKABLE QString get_friend_name(int fid);
    Q_INVOKABLE QString get_friend_avatar(int fid);
    Q_INVOKABLE QString get_friend_status_message(int fid);
    Q_INVOKABLE QString get_friend_status_icon(int fid);
    Q_INVOKABLE QString get_friend_status_icon(int fid, bool online);
    Q_INVOKABLE bool get_friend_connection_status(int fid);
    Q_INVOKABLE bool get_friend_accepted(int fid);
    Q_INVOKABLE bool get_friend_blocked(int fid);

    Q_INVOKABLE int get_message_type(int fid, int mid);
    Q_INVOKABLE bool get_message_author(int fid, int mid);
    Q_INVOKABLE QString get_message_text(int fid, int mid);
    Q_INVOKABLE QString get_message_html_escaped_text(int fid, int mid);
    Q_INVOKABLE QString get_message_rich_text(int fid, int mid);
    Q_INVOKABLE QDateTime get_message_timestamp(int fid, int mid);
    Q_INVOKABLE int get_file_status(int fid, int mid);
    Q_INVOKABLE QString get_file_link(int fid, int mid);
    Q_INVOKABLE int get_file_progress(int fid, int mid);

signals:
    void signal_focus_friend(int fid);
    void signal_friend_added(int fid);
    void signal_friend_activity(int fid);
    void signal_friend_blocked(int fid, bool blocked);

    void signal_friend_request(int fid);
    void signal_friend_message(int fid, int mid, int type);
    void signal_friend_action();
    void signal_friend_name(int fid, QString previous_name);
    void signal_friend_status_message(int fid);
    void signal_friend_status(int fid);
    void signal_friend_typing(int fid, bool is_typing);
    void signal_friend_read_receipt();
    void signal_friend_connection_status(int fid, bool online);
    void signal_avatar_change(int fid);

    void signal_file_status(int fid, int mid, int status);
    void signal_file_progress(int fid, int mid, int progress);

    void signal_av_invite(int fid);

public slots:
    void wifi_changed(QString str, QDBusVariant variant);
};

void start_tox_thread(Cyanide *cyanide);

#endif // CYANIDE_H
