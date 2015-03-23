#include <time.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sailfishapp.h>
#include <thread>
#include <nemonotifications-qt5/notification.h>
#include <QtQuick>
#include <QSound>
#include <QTranslator>

#include "cyanide.h"
#include "tox_bootstrap.h"
#include "tox_callbacks.h"
#include "util.h"
#include "dns.cpp"
#include "config.h"
#include "settings.h"

Cyanide cyanide;

Settings settings;

struct Tox_Options tox_options;

int main(int argc, char *argv[])
{
    qmlRegisterType<Notification>("nemonotifications", 1, 0, "Notification");

    QGuiApplication *app = SailfishApp::application(argc, argv);
    app->setOrganizationName("Tox");
    app->setOrganizationDomain("Tox");
    app->setApplicationName("Cyanide");
    cyanide.view = SailfishApp::createView();

    cyanide.load_tox_and_stuff_pretty_please();
    std::thread tox_thread(start_tox_thread, &cyanide);

    settings.init();
    cyanide.view->rootContext()->setContextProperty("settings", &settings);
    cyanide.view->rootContext()->setContextProperty("cyanide", &cyanide);
    cyanide.view->setSource(SailfishApp::pathTo("qml/cyanide.qml"));
    cyanide.view->showFullScreen();

    QObject::connect(cyanide.view, SIGNAL(visibilityChanged(QWindow::Visibility)),
                    &cyanide, SLOT(visibility_changed(QWindow::Visibility)));

    QSound::play("");

    int result = app->exec();

    emit cyanide.signal_close_notifications();

    cyanide.run_tox_loop = false;
    tox_thread.join();

    return result;
}

void Cyanide::visibility_changed(QWindow::Visibility visibility)
{
    if(visibility == QWindow::FullScreen)
        emit signal_close_notifications();
}

void Cyanide::raise()
{
    if(!is_visible()) {
        view->raise();
        view->requestActivate();
    }
}

bool Cyanide::is_visible()
{
    return view->visibility() == QWindow::FullScreen;
}

void Cyanide::load_tox_and_stuff_pretty_please()
{
    int error;

    run_tox_loop = true;
    save_needed = false;
    self.user_status = TOX_USER_STATUS_NONE;

    tox_options = *tox_options_new((TOX_ERR_OPTIONS_NEW*)&error);
    // TODO

    sprintf(save_path, "%s/.config/tox/tox_save.tox", getenv("HOME"));

    size_t save_data_size;
    const uint8_t *save_data = get_save_data(&save_data_size);
    tox = tox_new(&tox_options, save_data, save_data_size, (TOX_ERR_NEW*)&error);

    if(save_data_size == 0)
        load_defaults();
    else
        load_tox_data();

    tox_self_get_address(tox, self_address);
    qDebug() << "Name:" << self.name;
    qDebug() << "Status" << self.status_message;
    qDebug() << "Tox ID" << get_self_address();
}

void start_tox_thread(Cyanide *cyanide)
{
    cyanide->tox_thread();
}

void Cyanide::tox_thread()
{
    set_callbacks();

    // Connect to bootstraped nodes in "tox_bootstrap.h"

    do_bootstrap();

    uint64_t last_save = get_time(), time;
    TOX_CONNECTION connection, c;
    c = connection = TOX_CONNECTION_NONE;
    while(run_tox_loop) {
        // Put toxcore to work
        tox_iterate(tox);

        // Check current connection
        if((c = tox_self_get_connection_status(tox)) != connection) {
            self.connection_status = connection = c;
            emit cyanide.signal_friend_connection_status(self_fid);
            qDebug() << (c != TOX_CONNECTION_NONE ? "Connected to DHT" : "Disconnected from DHT");
        }

        time = get_time();

        // Wait 1 million ticks then reconnect if needed and write save
        if(time - last_save >= (uint64_t)10 * 1000 * 1000 * 1000) {
            last_save = time;

            if(connection == TOX_CONNECTION_NONE) {
                do_bootstrap();
            }

            if (save_needed || (time - last_save >= (uint)100 * 1000 * 1000 * 1000)) {
                write_save();
            }
        }
    }

    qDebug() << "exiting....";

    write_save();

    //toxav_kill(av);
    tox_kill(tox);
}

/* bootstrap to dht with bootstrap_nodes */
void Cyanide::do_bootstrap()
{
    TOX_ERR_BOOTSTRAP error;
    static unsigned int j = 0;

    if (j == 0)
        j = rand();

    int i = 0;
    while(i < 4) {
        struct bootstrap_node *d = &bootstrap_nodes[j % countof(bootstrap_nodes)];
        tox_bootstrap(tox, d->address, d->port, d->key, &error);
        i++;
        j++;
    }
}

void Cyanide::load_defaults()
{
    TOX_ERR_SET_INFO error;

    uint8_t *name = (uint8_t*)DEFAULT_NAME, *status = (uint8_t*)DEFAULT_STATUS;
    uint16_t name_len = sizeof(DEFAULT_NAME) - 1, status_len = sizeof(DEFAULT_STATUS) - 1;

    self.name = QString(DEFAULT_NAME);
    self.status_message = QString(DEFAULT_STATUS);

    tox_self_set_name(tox, name, name_len, &error);
    tox_self_set_status_message(tox, status, status_len, &error);

    emit cyanide.signal_friend_name(self_fid, NULL);
    emit cyanide.signal_friend_status_message(self_fid);
    save_needed = true;
}

void Cyanide::write_save()
{
    void *data;
    uint32_t size;
    char tmp_path[512];
    FILE *file;

    size = tox_get_savedata_size(tox);
    data = malloc(size);
    tox_get_savedata(tox, (uint8_t*)data);

    mkdir(CONFIG_PATH, 0755);
    chmod(CONFIG_PATH, 0755);
    sprintf(tmp_path, "%s/tox_save.tmp", CONFIG_PATH);

    file = fopen(tmp_path, "wb");
    if(file) {
        fwrite(data, size, 1, file);
        fflush(file);
        int fd = fileno(file);
        fsync(fd);
        fclose(file);
        if(rename(tmp_path, save_path) != 0) {
            qDebug() << "Failed to rename file, trying again";
            remove(save_path);
            if(rename(tmp_path, save_path) != 0) {
                qDebug() << "Saving Failed";
            } else {
                qDebug() << "Saved data";
            }
        } else {
            qDebug() << "Saved data";
        }
    }
    int ch = chmod(save_path, S_IRUSR | S_IWUSR);
    if(ch)
        qDebug() << "CHMOD: failure";

    save_needed = false;
    free(data);
}

const uint8_t* Cyanide::get_save_data(size_t *size)
{
    void *data;

    qDebug() << "save_path" << save_path;

    data = file_raw(save_path, size);
    if(!data)
        size = 0;

    return (const uint8_t*)data;
}

void Cyanide::load_tox_data()
{
    TOX_ERR_FRIEND_QUERY error;
    bool success;
    size_t length;
    size_t nfriends = tox_self_get_friend_list_size(tox);
    qDebug() << "Loading" << nfriends << "friends...";

    for(size_t i = 0; i < nfriends; i++) {
        Friend f = *new Friend();

        if(!tox_friend_get_public_key(tox, i, f.public_key, (TOX_ERR_FRIEND_GET_PUBLIC_KEY*)&error))
            qDebug() << "Failed to get public key for friend " << i;

        length = tox_friend_get_name_size(tox, i, &error);
        uint8_t name[length];
        success = tox_friend_get_name(tox, i, name, &error);
        f.name = utf8_to_qstr(name, length);
        Q_ASSERT(success);

        length = tox_friend_get_status_message_size(tox, i, &error);
        uint8_t status_message[length];
        success = tox_friend_get_status_message(tox, i, status_message, &error);
        f.status_message = utf8_to_qstr(status_message, length);
        Q_ASSERT(success);

        friends.push_back(f);
    }

    length = tox_self_get_name_size(tox);
    uint8_t name[length];
    tox_self_get_name(tox, name);
    self.name = utf8_to_qstr(name, length);

    length = tox_self_get_status_message_size(tox);
    uint8_t status_message[length];
    tox_self_get_status_message(tox, status_message);
    self.status_message = utf8_to_qstr(status_message, length);

    emit cyanide.signal_friend_name(self_fid, NULL);
    emit cyanide.signal_friend_status_message(self_fid);
    save_needed = true;
}

void Cyanide::add_friend(Friend *f)
{
    friends.push_back(*f);
    emit cyanide.signal_friend_added(friends.size() - 1);
}

void Cyanide::set_callbacks()
{
    tox_callback_friend_request(tox, callback_friend_request, NULL);
    tox_callback_friend_message(tox, callback_friend_message, NULL);
    tox_callback_friend_name(tox, callback_friend_name, NULL);
    tox_callback_friend_status_message(tox, callback_friend_status_message, NULL);
    tox_callback_friend_status(tox, callback_friend_status, NULL);
    tox_callback_friend_typing(tox, callback_friend_typing, NULL);
    tox_callback_friend_read_receipt(tox, callback_friend_read_receipt, NULL);
    tox_callback_friend_connection_status(tox, callback_friend_connection_status, NULL);

    /*
    tox_callback_group_invite(tox, callback_group_invite, NULL);
    tox_callback_group_message(tox, callback_group_message, NULL);
    tox_callback_group_action(tox, callback_group_action, NULL);
    tox_callback_group_title(tox, callback_group_title, NULL);
    tox_callback_group_namelist_change(tox, callback_group_namelist_change, NULL);
    */

}

void callback_friend_request(Tox *UNUSED(tox), const uint8_t *id, const uint8_t *msg, size_t length, void *UNUSED(userdata))
{
    qDebug() << "was called";
    int name_length = 2 * TOX_ADDRESS_SIZE * sizeof(char);
    void *name = malloc(name_length);
    address_to_string((char*)name, (char*)id);
    Friend *f = new Friend(id, utf8_to_qstr(name, name_length), utf8_to_qstr(msg, length));
    f->accepted = false;
    cyanide.add_friend(f);
    emit cyanide.signal_friend_request(cyanide.friends.size()-1);
}

void callback_friend_message(Tox *UNUSED(tox), uint32_t fid, TOX_MESSAGE_TYPE type, const uint8_t *message, size_t length, void *UNUSED(userdata))
{
    //TODO honor type
    std::vector<Message> *tmp = &cyanide.friends[fid].messages;
    tmp->push_back(Message(message, length, false));
    cyanide.friends[fid].notification = true;
    emit cyanide.signal_friend_message(fid, tmp->size() - 1);
    qDebug() << "received message" << fid << length;
}

void callback_friend_name(Tox *UNUSED(tox), uint32_t fid, const uint8_t *newname, size_t length, void *UNUSED(userdata))
{
    qDebug() << "was called";
    Friend *f = &cyanide.friends[fid];
    QString previous_name = f->name;
    f->name = utf8_to_qstr(newname, length);
    emit cyanide.signal_friend_name(fid, previous_name);
    cyanide.save_needed = true;
}

void callback_friend_status_message(Tox *UNUSED(tox), uint32_t fid, const uint8_t *newstatus, size_t length, void *UNUSED(userdata))
{
    qDebug() << "was called";
    cyanide.friends[fid].status_message = utf8_to_qstr(newstatus, length);
    emit cyanide.signal_friend_status_message(fid);
    cyanide.save_needed = true;
}

void callback_friend_status(Tox *UNUSED(tox), uint32_t fid, TOX_USER_STATUS status, void *UNUSED(userdata))
{
    qDebug() << "was called";
    cyanide.friends[fid].user_status = status;
    emit cyanide.signal_friend_status(fid);
}

void callback_friend_typing(Tox *UNUSED(tox), uint32_t fid, bool is_typing, void *UNUSED(userdata))
{
    emit cyanide.signal_friend_typing(fid, is_typing);
}

void callback_friend_read_receipt(Tox *UNUSED(tox), uint32_t fid, uint32_t receipt, void *UNUSED(userdata))
{
    qDebug() << "was called";
    return;
}

void callback_friend_connection_status(Tox *tox, uint32_t fid, TOX_CONNECTION status, void *UNUSED(userdata))
{
    cyanide.friends[fid].connection_status = status;
    emit cyanide.signal_friend_connection_status(fid);
}

void callback_avatar_info(Tox *tox, uint32_t fid, uint8_t format, uint8_t *hash, void *UNUSED(userdata))
{
    qDebug() << "was called";
    return;
//    FRIEND *f = &friend[fid];
//
//    if (format != TOX_AVATAR_FORMAT_NONE) {
//        if (!friend_has_avatar(f) || memcmp(f->avatar.hash, hash, TOX_HASH_LENGTH) != 0) { // check if avatar has changed
//            memcpy(f->avatar.hash, hash, TOX_HASH_LENGTH); // set hash pre-emptively so we don't request data twice
//
//            char_t hash_string[TOX_HASH_LENGTH * 2];
//            hash_to_string(hash_string, hash);
//            debug("Friend Avatar Hash (%u): %.*s\n", fid, (int)sizeof(hash_string), hash_string);
//
//            tox_request_avatar_data(tox, fid);
//        }
//    } else if (friend_has_avatar(f)) {
//        postmessage(FRIEND_UNSETAVATAR, fid, 0, NULL); // unset avatar if we had one
//    }
}

void callback_avatar_data(Tox *tox, uint32_t fid, uint8_t format, uint8_t *hash, uint8_t *data, uint32_t datalen, void *UNUSED(userdata))
{
    qDebug() << "was called";
    return;
//    FRIEND *f = &friend[fid];
//
//    if (memcmp(f->avatar.hash, hash, TOX_HASH_LENGTH) == 0) { // same hash as in last avatar_info
//        uint8_t *data_out = malloc(datalen);
//        memcpy(data_out, data, datalen);
//        postmessage(FRIEND_SETAVATAR, fid, datalen, data_out);
//    }
}

void callback_group_invite(Tox *tox, uint32_t fid, uint8_t type, const uint8_t *data, uint16_t length, void *UNUSED(userdata))
{
    qDebug() << "was called";
    return;
//    int gid = -1;
//    if (type == TOX_GROUPCHAT_TYPE_TEXT) {
//        gid = tox_join_groupchat(tox, fid, data, length);
//    } else if (type == TOX_GROUPCHAT_TYPE_AV) {
//        gid = toxav_join_av_groupchat(tox, fid, data, length, &callback_av_group_audio, NULL);
//    }
//
//    if(gid != -1) {
//        postmessage(GROUP_ADD, gid, 0, tox);
//    }
//
//    debug("Group Invite (%i,f:%i) type %u\n", gid, fid, type);
}

void callback_group_message(Tox *tox, int gid, int pid, const uint8_t *message, uint16_t length, void *UNUSED(userdata))
{
    qDebug() << "was called";
    return;
//    postmessage(GROUP_MESSAGE, gid, 0, copy_groupmessage(tox, message, length, MSG_TYPE_TEXT, gid, pid));
//
//    debug("Group Message (%u, %u): %.*s\n", gid, pid, length, message);
}

void callback_group_action(Tox *tox, int gid, int pid, const uint8_t *action, uint16_t length, void *UNUSED(userdata))
{
    qDebug() << "was called";
    return;
//    postmessage(GROUP_MESSAGE, gid, 0, copy_groupmessage(tox, action, length, MSG_TYPE_ACTION_TEXT, gid, pid));
//
//    debug("Group Action (%u, %u): %.*s\n", gid, pid, length, action);
}

void callback_group_namelist_change(Tox *tox, int gid, int pid, uint8_t change, void *UNUSED(userdata))
{
    qDebug() << "was called";
    return;
//    switch(change) {
//    case TOX_CHAT_CHANGE_PEER_ADD: {
//        postmessage(GROUP_PEER_ADD, gid, pid, tox);
//        break;
//    }
//
//    case TOX_CHAT_CHANGE_PEER_DEL: {
//        postmessage(GROUP_PEER_DEL, gid, pid, tox);
//        break;
//    }
//
//    case TOX_CHAT_CHANGE_PEER_NAME: {
//        uint8_t name[TOX_MAX_NAME_LENGTH];
//        int len = tox_group_peername(tox, gid, pid, name);
//
//        len = utf8_validate(name, len);
//
//        uint8_t *data = malloc(len + 1);
//        data[0] = len;
//        memcpy(data + 1, name, len);
//
//        postmessage(GROUP_PEER_NAME, gid, pid, data);
//        break;
//    }
//    }
//    debug("Group Namelist Change (%u, %u): %u\n", gid, pid, change);
}

void callback_group_title(Tox *tox, int gid, int pid, const uint8_t *title, uint8_t length, void *UNUSED(userdata))
{
    qDebug() << "was called";
    return;
//    length = utf8_validate(title, length);
//    if (!length)
//        return;
//
//    uint8_t *copy_title = malloc(length);
//    if (!copy_title)
//        return;
//
//    memcpy(copy_title, title, length);
//    postmessage(GROUP_TITLE, gid, length, copy_title);
//
//    debug("Group Title (%u, %u): %.*s\n", gid, pid, length, title);
}

void callback_file_send_request(Tox *tox, int32_t fid, uint8_t filenumber, uint64_t filesize, const uint8_t *filename,
                                       uint16_t filename_length, void *UNUSED(userdata))
{
    qDebug() << "was called";
    return;
}
void callback_file_control(Tox *tox, int32_t fid, uint8_t receive_send, uint8_t filenumber, uint8_t control,
                                  const uint8_t *data, uint16_t length, void *UNUSED(userdata))
{
    qDebug() << "was called";
    return;
}
void callback_file_data(Tox *UNUSED(tox), int32_t fid, uint8_t filenumber, const uint8_t *data, uint16_t length,
                               void *UNUSED(userdata))
{
    qDebug() << "was called";
    return;
}

void Cyanide::send_typing_notification(int fid, bool typing)
{
    TOX_ERR_SET_TYPING error;
    if(settings.get("send-typing-notifications") == "true")
        tox_self_set_typing(tox, (uint32_t)fid, typing, &error);
}

QString Cyanide::send_friend_request(QString id_str, QString msg_str)
{
    size_t id_len = qstrlen(id_str);
    char id[id_len];
    qstr_to_utf8((uint8_t*)id, id_str);

    if(msg_str.isEmpty())
        msg_str = QString(DEFAULT_FRIEND_REQUEST_MESSAGE);

    size_t msg_len = qstrlen(msg_str);
    uint8_t msg[msg_len];
    qstr_to_utf8(msg, msg_str);

    uint8_t address[TOX_ADDRESS_SIZE];
    if(!string_to_address((char*)address, (char*)id)) {
        /* not a regular id, try DNS discovery */
        void *data = dns_request((char*)id, id_len);
        if(data == NULL)
            return tr("Invalid Tox ID");
        memcpy(address, data, TOX_ADDRESS_SIZE);
        free(data);
    }

    QString errmsg = send_friend_request_id(address, msg, msg_len);
    if(errmsg == "") {
        settings.add_friend_address(friends.size()-1, (const char*)address);
        Friend *f = new Friend((const uint8_t*)address, id_str, "");
        add_friend(f);
    }

    return errmsg;
}

/* */
QString Cyanide::send_friend_request_id(const uint8_t *id, const uint8_t *msg, size_t msg_length)
{
    TOX_ERR_FRIEND_ADD error;
    uint32_t fid = tox_friend_add(tox, id, msg, msg_length, &error);
    switch(error) {
        case TOX_ERR_FRIEND_ADD_OK:
            return "";
        case TOX_ERR_FRIEND_ADD_NULL:
            return ""; // TODO
        case TOX_ERR_FRIEND_ADD_TOO_LONG:
            return tr("Error: Message is too long");
        case TOX_ERR_FRIEND_ADD_NO_MESSAGE:
            return tr("Error: Empty message");
        case TOX_ERR_FRIEND_ADD_OWN_KEY:
            return tr("Error: Tox ID is self ID");
        case TOX_ERR_FRIEND_ADD_ALREADY_SENT:
            return tr("Error: Tox ID is already in friend list");
        case TOX_ERR_FRIEND_ADD_BAD_CHECKSUM:
            return tr("Error: Invalid Tox ID (bad checksum)");
        case TOX_ERR_FRIEND_ADD_SET_NEW_NOSPAM:
            return tr("Error: Invalid Tox ID (bad nospam value)");
        case TOX_ERR_FRIEND_ADD_MALLOC:
            return tr("Error: No memory");
        default:
            Q_ASSERT(fid != UINT32_MAX);
            Q_ASSERT(fid == friends.size()-1);
            return "";
    }
}

bool Cyanide::send_friend_message(int fid, QString msg_str)
{
    TOX_ERR_FRIEND_SEND_MESSAGE error;
    TOX_MESSAGE_TYPE type = TOX_MESSAGE_TYPE_NORMAL;
    Friend *f = &friends[fid];
    if(f->connection_status == TOX_CONNECTION_NONE)
        return false;

    size_t msg_len = qstrlen(msg_str);
    uint8_t msg[msg_len];
    qstr_to_utf8(msg, msg_str);
    uint32_t message_id = tox_friend_send_message(tox, fid, type, msg, msg_len, &error);
    qDebug() << "message id:" << message_id;

    f->messages.push_back(Message(msg_str, true));
    emit cyanide.signal_friend_message(self_fid, f->messages.size() - 1);

    return true;
}

bool Cyanide::accept_friend_request(int fid)
{
    TOX_ERR_FRIEND_ADD error;
    if(tox_friend_add_norequest(tox, friends[fid].public_key, &error) == UINT32_MAX) {
        qDebug() << "could not add friend";
        return false;
    }

    friends[fid].accepted = true;
    save_needed = true;
    return true;
}

void Cyanide::remove_friend(int fid)
{
    TOX_ERR_FRIEND_DELETE error;
    if(tox_friend_delete(tox, fid, &error)) {
        save_needed = true;
        friends.erase(friends.begin() + fid);
        settings.remove_friend(fid);
    } else {
        qDebug() << "Failed to remove friend";
    }
}

void Cyanide::play_sound(QString file)
{
    if(file == "")
        return;

    QSound::play(file);
}

/* setters and getters */

void Cyanide::set_friend_notification(int fid, bool status)
{
    friends[fid].notification = status;
    emit cyanide.signal_notification(fid);
}

void Cyanide::set_self_name(QString name)
{
    bool success;
    TOX_ERR_SET_INFO error;
    self.name = name;

    size_t length = qstrlen(name);
    uint8_t tmp[length];
    qstr_to_utf8(tmp, name);
    success = tox_self_set_name(tox, tmp, length, &error);
    Q_ASSERT(success);

    save_needed = true;
    emit signal_friend_name(self_fid, NULL);
}

void Cyanide::set_self_status_message(QString status_message)
{
    bool success;
    TOX_ERR_SET_INFO error;
    self.status_message = status_message;

    size_t length = qstrlen(status_message);
    uint8_t tmp[length];
    qstr_to_utf8(tmp, status_message);

    success = tox_self_set_status_message(tox, tmp, length, &error);
    Q_ASSERT(success);

    save_needed = true;
    emit signal_friend_status_message(self_fid);
}

int Cyanide::get_self_user_status()
{
    switch(self.user_status) {
        case TOX_USER_STATUS_NONE:
            return 0;
        case TOX_USER_STATUS_AWAY:
            return 1;
        case TOX_USER_STATUS_BUSY:
            return 2;
        default:
            Q_ASSERT(false);
            return 0;
    }
}

void Cyanide::set_self_user_status(int status)
{
    switch(status) {
        case 0:
            self.user_status = TOX_USER_STATUS_NONE;
            break;
        case 1:
            self.user_status = TOX_USER_STATUS_AWAY;
            break;
        case 2:
            self.user_status = TOX_USER_STATUS_BUSY;
            break;
    }
    tox_self_set_status(tox, self.user_status);
    emit cyanide.signal_friend_status(self_fid);
}

int Cyanide::get_number_of_friends()
{
    return friends.size();
}

QString Cyanide::get_friend_name(int fid)
{
    Friend f = fid == -1 ? self : friends[fid];
    return f.name;
}

QString Cyanide::get_friend_avatar(int fid)
{
    return QString("qrc:/images/blankavatar");
}

QString Cyanide::get_friend_status_message(int fid)
{
    Friend f = fid == -1 ? self : friends[fid];
    return f.status_message;
}

QString Cyanide::get_friend_status_icon(int fid)
{
    QString url = "qrc:/images/";
    Friend f = fid == -1 ? self : friends[fid];

    if(f.connection_status == TOX_CONNECTION_NONE) {
        url.append("offline");
    } else {
        switch(f.user_status) {
        case TOX_USER_STATUS_NONE:
            url.append("online");
            break;
        case TOX_USER_STATUS_AWAY:
            url.append("away");
            break;
        case TOX_USER_STATUS_BUSY:
            url.append("busy");
            break;
        default:
            Q_ASSERT(false);
            url.append("offline");
        }
    }
    
    if(f.notification)
        url.append("_notification");

    url.append("_2x");

    return url;
}

bool Cyanide::get_friend_connection_status(int fid)
{
    Friend f = fid == -1 ? self : friends[fid];
    return f.connection_status != TOX_CONNECTION_NONE;
}

bool Cyanide::get_friend_accepted(int fid)
{
    return friends[fid].accepted;
}

QString Cyanide::get_friend_public_key(int fid)
{
    uint8_t hex_id[2 * TOX_PUBLIC_KEY_SIZE];
    public_key_to_string((char*)hex_id, (char*)friends[fid].public_key);
    return utf8_to_qstr(hex_id, 2 * TOX_PUBLIC_KEY_SIZE);
}

QString Cyanide::get_self_address()
{
    char hex_address[2 * TOX_ADDRESS_SIZE];
    address_to_string((char*)hex_address, (char*)self_address);
    return utf8_to_qstr(hex_address, 2 * TOX_ADDRESS_SIZE);
}

int Cyanide::get_number_of_messages(int fid)
{
    return friends[fid].messages.size();
}

QString Cyanide::get_message_text(int fid, int mid)
{
    return friends[fid].messages[mid].text;
}

QString Cyanide::get_message_rich_text(int fid, int mid)
{
    QString text = friends[fid].messages[mid].text;

    const QString email_chars = QRegExp::escape("A-Za-z0-9!#$%&'*+-/=?^_`{|}~.");
    const QString url_chars =   QRegExp::escape("A-Za-z0-9!#$%&'*+-/=?^_`{|}~");
    const QString email_token = "[" + email_chars + "]+";
    const QString url_token = "[" + url_chars + "]+";

    /* match either protocol:address or email@domain or example.org */
    QRegExp rx("(\\b[A-Za-z][A-Za-z0-9]*:[^\\s]+\\b"
               "|\\b" + email_token + "@" + email_token + "\\b"
               "|\\b" + url_token  + "\\." + email_token + "\\b)");
    QString link;
    int protocol;
    int pos = 0;

     while ((pos = rx.indexIn(text, pos)) != -1) {
         /* check whether the captured text has a protocol identifier */
         protocol = rx.cap(1).indexOf(QRegExp("^[A-Za-z0-9]+:"), 0);
         link = "<a href=\"" +
                 QString(  (protocol != -1) ? ""
                         : (rx.cap(1).indexOf("@") != -1) ? "mailto:"
                         : "https:")
                 + rx.cap(1) + "\">" + rx.cap(1) + "</a>";
         text.replace(pos, rx.matchedLength(), link);
         pos += link.length();
     }

    return text;
}

QDateTime Cyanide::get_message_timestamp(int fid, int mid)
{
    return friends[fid].messages[mid].timestamp;
}

bool Cyanide::get_message_author(int fid, int mid)
{
    return friends[fid].messages[mid].author;
}
