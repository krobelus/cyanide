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

Tox_Options options;

int main(int argc, char *argv[])
{
    qmlRegisterType<Notification>("nemonotifications", 1, 0, "Notification");

    QGuiApplication *app = SailfishApp::application(argc, argv);
    app->setOrganizationName("Tox");
    app->setOrganizationDomain("Tox");
    app->setApplicationName("Cyanide");
    cyanide.view = SailfishApp::createView();

    cyanide.load_tox_and_stuff_pretty_please();
    std::thread tox_thread(init, &cyanide);

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

void init(Cyanide *cyanide)
{
    cyanide->tox_thread();
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
    run_tox_loop = true;
    save_needed = false;
    self.user_status = TOX_USERSTATUS_NONE;

    if((tox = tox_new(&options)) == NULL) {
        qDebug() << "tox_new() failed";
        exit(1);
    }

    sprintf(save_path, "%s/.config/tox/tox_save.tox", getenv("HOME"));

    if(!load_save())
        load_defaults();

    tox_get_address(tox, self_id);
    char hex_id[2 * TOX_FRIEND_ADDRESS_SIZE];
    id_to_string(hex_id, (char*)self_id);
    qDebug() << "Name:" << to_QString(self.name, self.name_length);
    qDebug() << "Status" << to_QString(self.status_message, self.status_message_length);
    qDebug() << "Tox ID" << to_QString(hex_id, 2 * TOX_FRIEND_ADDRESS_SIZE);
}

void Cyanide::tox_thread()
{
    /*
    uint8_t avatar_data[TOX_AVATAR_MAX_DATA_LENGTH];
    uint32_t avatar_size;
    if (init_avatar(&self.avatar, self.id, avatar_data, &avatar_size)) {
        tox_set_avatar(tox, TOX_AVATAR_FORMAT_PNG, avatar_data, avatar_size); // set avatar before connecting

        char_t hash_string[TOX_HASH_LENGTH * 2];
        hash_to_string(hash_string, self.avatar.hash);
        debug("Tox Avatar Hash: %.*s\n", (int)sizeof(hash_string), hash_string);
    }

    // Give toxcore the functions to call
    set_callbacks();

    */

    set_callbacks();

    //return; // asdf

    // Connect to bootstraped nodes in "tox_bootstrap.h"

    do_bootstrap();

    /*
    // Start the tox av session.
    av = toxav_new(tox, MAX_CALLS);

    // Give toxcore the av functions to call
    set_av_callbacks(av);

    global_av = av;
    tox_thread_init = 1;

    // Start the treads
    thread(audio_thread, av);
    thread(video_thread, av);
    thread(toxav_thread, av);
    */

    uint64_t last_save = get_time(), time;
    bool connected = false;
    while(run_tox_loop) {
        // Put toxcore to work
        tox_do(tox);

        // Check current connection
        if(tox_isconnected(tox) != connected) {
            connected = !connected;
            self.connection_status = connected;
            emit cyanide.signal_connection_status(self_fid);
            qDebug() << (connected ? "Connected to DHT" : "Disconnected from DHT");
        }

        time = get_time();

        // Wait 1 million ticks then reconnect if needed and write save
        if(time - last_save >= (uint64_t)10 * 1000 * 1000 * 1000) {
            last_save = time;

            if(!connected) {
                do_bootstrap();
            }

            if (save_needed
                    //|| (time - last_save >= (uint)100 * 1000 * 1000 * 1000)
                    ) {
                write_save();
            }
        }

        /*
        // If there's a message, load it, and send to the tox message thread
        if(tox_thread_msg) {
            TOX_MSG *msg = &tox_msg;
            // If msg->msg is 0, reconfig if needed and break from tox_do
            if(!msg->msg) {
                reconfig = msg->param1;
                tox_thread_msg = 0;
                break;
            }
            tox_thread_message(tox, av, time, msg->msg, msg->param1, msg->param2, msg->data);
            tox_thread_msg = 0;
        }
        */

        /*
        // Thread active transfers and check if friend is typing
        utox_thread_work_for_transfers(tox, time);
        utox_thread_work_for_typing_notifications(tox, time);
        */

        uint32_t interval = tox_do_interval(tox);
        usleep(1000 * ((interval > 20) ? 20 : interval));
    }

    qDebug() << "exiting....";

    write_save();

    //toxav_kill(av);
    tox_kill(tox);
}
/* bootstrap to dht with bootstrap_nodes */
void Cyanide::do_bootstrap()
{
    static unsigned int j = 0;

    if (j == 0)
        j = rand();

    int i = 0;
    while(i < 4) {
        struct bootstrap_node *d = &bootstrap_nodes[j % countof(bootstrap_nodes)];
        tox_bootstrap_from_address(tox, d->address, d->port, d->key);
        i++;
        j++;
    }
}

void Cyanide::load_defaults()
{
    uint8_t *name = (uint8_t*)DEFAULT_NAME, *status = (uint8_t*)DEFAULT_STATUS;
    uint16_t name_len = sizeof(DEFAULT_NAME) - 1, status_len = sizeof(DEFAULT_STATUS) - 1;

    memcpy(self.name, name, name_len);
    self.name_length = name_len;
    memcpy(self.status_message, status, status_len);
    self.status_message_length = status_len;

    tox_set_name(tox, name, name_len);
    tox_set_status_message(tox, status, status_len);

    emit cyanide.signal_name_change(self_fid, NULL);
    emit cyanide.signal_status_message(self_fid);
    save_needed = true;
}

void Cyanide::write_save()
{
    void *data;
    uint32_t size;
    char tmp_path[512];
    FILE *file;

    size = tox_size(tox);
    data = malloc(size);
    tox_save(tox, (uint8_t*)data);

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

bool Cyanide::load_save()
{
    uint32_t size;
    void *data;

    qDebug() << "save_path" << save_path;

    data = file_raw(save_path, &size);
    if(!data)
        return false;

    switch(tox_load(tox, (const uint8_t*)data, size)) {
        case  1: /* >found encrypted data
                  * we should probably tell the user at some point
                  */
            return false;
        case -1: /* TODO maybe check whether its sane to continue with this data
                  * for now we just assume that
                  */
            break;
    }

    uint32_t nfriends = tox_count_friendlist(tox);
    qDebug() << "Loading" << nfriends << "friends...";

    for(uint32_t i = 0; i < nfriends; i++) {
        Friend *f = new Friend();

        tox_get_client_id(tox, i, f->cid);
        f->name_length = tox_get_name(tox, i, f->name);

        f->status_message_length = tox_get_status_message_size(tox, i);
        tox_get_status_message(tox, i, f->status_message, TOX_MAX_STATUSMESSAGE_LENGTH);

        friends.push_back(*f);
    }

    self.name_length = tox_get_self_name(tox, self.name);
    self.status_message_length = tox_get_self_status_message_size(tox);
    tox_get_self_status_message(tox, self.status_message, self.status_message_length);

    emit cyanide.signal_name_change(self_fid, NULL);
    emit cyanide.signal_status_message(self_fid);
    save_needed = true;

    //char new_save_path[512];
    //sprintf(new_save_path, "%s/.config/tox/%s.tox", getenv("HOME"), to_const_char(self_name));
    //if(rename(save_path, new_save_path) != 0) {
    //    qDebug() << "renaming" << save_path << "to" << new_save_path << "failed";
    //}

    free(data);
    return true;
}

void Cyanide::add_friend(Friend *f)
{
    friends.push_back(*f);
    qDebug() << "added friend number" << friends.size() - 1 << "to the list view";
    emit cyanide.signal_friend_added(friends.size() - 1);
}

void Cyanide::set_callbacks()
{
    tox_callback_friend_request(tox, callback_friend_request, NULL);
    tox_callback_friend_message(tox, callback_friend_message, NULL);
    tox_callback_friend_action(tox, callback_friend_action, NULL);
    tox_callback_name_change(tox, callback_name_change, NULL);
    tox_callback_status_message(tox, callback_status_message, NULL);
    tox_callback_user_status(tox, callback_user_status, NULL);
    tox_callback_typing_change(tox, callback_typing_change, NULL);
    tox_callback_read_receipt(tox, callback_read_receipt, NULL);
    tox_callback_connection_status(tox, callback_connection_status, NULL);

    tox_callback_group_invite(tox, callback_group_invite, NULL);
    tox_callback_group_message(tox, callback_group_message, NULL);
    tox_callback_group_action(tox, callback_group_action, NULL);
    tox_callback_group_title(tox, callback_group_title, NULL);
    tox_callback_group_namelist_change(tox, callback_group_namelist_change, NULL);

    tox_callback_avatar_info(tox, callback_avatar_info, NULL);
    tox_callback_avatar_data(tox, callback_avatar_data, NULL);

    tox_callback_file_send_request(tox, callback_file_send_request, NULL);
    tox_callback_file_control(tox, callback_file_control, NULL);
    tox_callback_file_data(tox, callback_file_data, NULL);

    /*
    toxav_register_callstate_callback(toxav, onAvInvite, av_OnInvite, this);
    toxav_register_callstate_callback(toxav, onAvStart, av_OnStart, this);
    toxav_register_callstate_callback(toxav, onAvCancel, av_OnCancel, this);
    toxav_register_callstate_callback(toxav, onAvReject, av_OnReject, this);
    toxav_register_callstate_callback(toxav, onAvEnd, av_OnEnd, this);
    toxav_register_callstate_callback(toxav, onAvRinging, av_OnRinging, this);
    toxav_register_callstate_callback(toxav, onAvMediaChange, av_OnPeerCSChange, this);
    toxav_register_callstate_callback(toxav, onAvMediaChange, av_OnSelfCSChange, this);
    toxav_register_callstate_callback(toxav, onAvRequestTimeout, av_OnRequestTimeout, this);
    toxav_register_callstate_callback(toxav, onAvPeerTimeout, av_OnPeerTimeout, this);

    toxav_register_audio_callback(toxav, playCallAudio, this);
    toxav_register_video_callback(toxav, playCallVideo, this);
    */
}

//void* Cyanide::copy_message(const uint8_t *str, uint16_t length, uint8_t msg_type)
//{
//    return NULL;
//    length = utf8_validate(str, length);
//
//    MESSAGE *msg = malloc(sizeof(MESSAGE) + length);
//    msg->author = 0;
//    msg->msg_type = msg_type;
//    msg->length = length;
//    memcpy(msg->msg, str, length);
//
//    return msg;
//}

//void* Cyanide::copy_groupmessage(Tox *tox, const uint8_t *str, uint16_t length, uint8_t msg_type, int gid, int pid)
//{
//    return NULL;
//    uint8_t name[TOX_MAX_NAME_LENGTH];
//    int namelen = tox_group_peername(tox, gid, pid, name);
//    if(namelen == 0 || namelen == -1) {
//        memcpy(name, "<unknown>", 9);
//        namelen = 9;
//    }
//
//    length = utf8_validate(str, length);
//    namelen = utf8_validate(name, namelen);
//
//
//    MESSAGE *msg = malloc(sizeof(MESSAGE) + 1 + length + namelen);
//    msg->author = 0;
//    msg->msg_type = msg_type;
//    msg->length = length;
//    memcpy(msg->msg, str, length);
//
//    msg->msg[length] = (char_t)namelen;
//    memcpy(&msg->msg[length] + 1, name, namelen);
//
//    return msg;
//}

void callback_friend_request(Tox *UNUSED(tox), const uint8_t *id, const uint8_t *msg, uint16_t length, void *UNUSED(userdata))
{
    qDebug() << "was called";
    int name_length = 2 * TOX_FRIEND_ADDRESS_SIZE * sizeof(char);
    void *name = malloc(name_length);
    id_to_string((char*)name, (char*)id);
    Friend f = *new Friend(id, (const uint8_t*)name, name_length, (const uint8_t*)msg, length);
    f.accepted = false;
    cyanide.add_friend(&f);
    emit cyanide.signal_friend_request(cyanide.friends.size()-1);
}

void callback_friend_message(Tox *tox, int fid, const uint8_t *message, uint16_t length, void *UNUSED(userdata))
{
    std::vector<Message> *tmp = &cyanide.friends[fid].messages;
    tmp->push_back(Message(message, length, false));
    cyanide.friends[fid].notification = true;
    emit cyanide.signal_friend_message(fid, tmp->size() - 1);
}

void callback_friend_action(Tox *tox, int fid, const uint8_t *action, uint16_t length, void *UNUSED(userdata))
{
    qDebug() << "was called";
    return;
//    /* send action/emote to UI */
//    postmessage(FRIEND_MESSAGE, fid, 0, copy_message(action, length, MSG_TYPE_ACTION_TEXT));
//
//    debug("Friend Action (%u): %.*s\n", fid, length, action);
//
//    /* write action/emote to logfile */
//    log_write(tox, fid, action, length, 0, LOG_FILE_MSG_TYPE_ACTION);
}

void callback_name_change(Tox *UNUSED(tox), int fid, const uint8_t *newname, uint16_t length, void *UNUSED(userdata))
{
    qDebug() << "was called";
    Friend *f = &cyanide.friends[fid];
    QString previous_name = to_QString(f->name, f->name_length);
    memcpy(f->name, newname, length);
    f->name_length = length;
    emit cyanide.signal_name_change(fid, previous_name);
    cyanide.save_needed = true;
}

void callback_status_message(Tox *UNUSED(tox), int fid, const uint8_t *newstatus, uint16_t length, void *UNUSED(userdata))
{
    qDebug() << "was called";
    memcpy(cyanide.friends[fid].status_message, newstatus, length);
    cyanide.friends[fid].status_message_length = length;
    emit cyanide.signal_status_message(fid);
    cyanide.save_needed = true;
}

void callback_user_status(Tox *UNUSED(tox), int fid, uint8_t status, void *UNUSED(userdata))
{
    qDebug() << "was called";
    cyanide.friends[fid].user_status = status;
    emit cyanide.signal_user_status(fid);
}

void callback_typing_change(Tox *UNUSED(tox), int fid, uint8_t is_typing, void *UNUSED(userdata))
{
    emit cyanide.signal_typing_change(fid, is_typing == 1);
}

void callback_read_receipt(Tox *UNUSED(tox), int fid, uint32_t receipt, void *UNUSED(userdata))
{
    qDebug() << "was called";
    return;
}

void callback_connection_status(Tox *tox, int fid, uint8_t status, void *UNUSED(userdata))
{
    if(fid > -1)
        qDebug() << "Friend " << fid << "now" << (status ? "online" : "offline");
    //if(status)
        //tox_request_avatar_info(tox, fid);
    cyanide.friends[fid].connection_status = status;
    emit cyanide.signal_connection_status(fid);
//    FRIEND *f = &friend[fid];
//    int i;
//
//    postmessage(FRIEND_ONLINE, fid, status, NULL);
//
//    if(!status) {
//        /* break all file transfers */
//        for(i = 0; i != countof(f->incoming); i++) {
//            if(f->incoming[i].status) {
//                f->incoming[i].status = FT_BROKE;
//                postmessage(FRIEND_FILE_IN_STATUS, fid, i, (void*)FILE_BROKEN);
//            }
//        }
//        for(i = 0; i != countof(f->outgoing); i++) {
//            if(f->outgoing[i].status) {
//                f->outgoing[i].status = FT_BROKE;
//                postmessage(FRIEND_FILE_OUT_STATUS, fid, i, (void*)FILE_BROKEN);
//            }
//        }
//    } else {
//        /* resume any broken file transfers */
//        for(i = 0; i != countof(f->incoming); i++) {
//            if(f->incoming[i].status == FT_BROKE) {
//                tox_file_send_control(tox, fid, 1, i, TOX_FILECONTROL_RESUME_BROKEN, (void*)&f->incoming[i].bytes, sizeof(uint64_t));
//            }
//        }
//        /* request avatar info (in case it changed) */
//        tox_request_avatar_info(tox, fid);
//    }
//
//    debug("Friend Online/Offline (%u): %u\n", fid, status);
}

void callback_avatar_info(Tox *tox, int fid, uint8_t format, uint8_t *hash, void *UNUSED(userdata))
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

void callback_avatar_data(Tox *tox, int fid, uint8_t format, uint8_t *hash, uint8_t *data, uint32_t datalen, void *UNUSED(userdata))
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

void callback_group_invite(Tox *tox, int fid, uint8_t type, const uint8_t *data, uint16_t length, void *UNUSED(userdata))
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

QString Cyanide::send_friend_request(QString id_string, QString msg_string)
{
    int id_len = tox_string_length(id_string);
    char id[id_len];
    to_tox_string(id_string, (uint8_t*)id);

    if(msg_string.isEmpty())
        msg_string = QString(DEFAULT_FRIEND_REQUEST_MESSAGE);

    int msg_len = tox_string_length(msg_string);
    char msg[msg_len];
    to_tox_string(msg_string, (uint8_t*)msg);

    QString error = send_friend_request_unboxed(id, id_len, msg, msg_len);
    if(error == "")
        settings.add_friend_address(friends.size()-1, id_string);
    return error;
}

void Cyanide::send_typing_notification(int fid, bool typing)
{
    if(settings.get("send-typing-notifications") == "true")
        tox_set_user_is_typing(tox, fid, (uint8_t)typing);
}

QString Cyanide::send_friend_request_unboxed(char *name, int length, char *msg, int msg_length)
{
    if(!length) {
        return tr("No name");
    }

    uint8_t id[TOX_FRIEND_ADDRESS_SIZE];
    //if(length_cleaned == TOX_FRIEND_ADDRESS_SIZE * 2 && string_to_id((char*)id, (char*)name_cleaned)) {
    if(string_to_id((char*)id, (char*)name)) {
        //return friend_addid(id, msg, msg_length);
    } else {
        /* not a regular id, try DNS discovery */
        // addfriend_status = ADDF_DISCOVER;
        //void *data = dns_request((char*)name_cleaned, length_cleaned);
        void *data = dns_request((char*)name, length);
        if(data == NULL)
            return tr("Invalid Tox ID");
        memcpy(id, data, TOX_FRIEND_ADDRESS_SIZE);
        free(data);
    }

    QString error = send_friend_request_id(id, (const uint8_t*)msg, msg_length);

    if(error == "") {
        Friend *f = new Friend((const uint8_t*)id, (const uint8_t*)name, length, NULL, 0);
        add_friend(f);
    }

    return error;
}


/* */
QString Cyanide::send_friend_request_id(const uint8_t *id, const uint8_t *msg, int msg_length)
{
    int32_t fid = tox_add_friend(tox, id, msg, msg_length);
    switch(fid) {
        case TOX_FAERR_TOOLONG:
            return tr("Error: Message is too long");
        case TOX_FAERR_NOMESSAGE:
            return tr("Error: Empty message");
        case TOX_FAERR_OWNKEY:
            return tr("Error: Tox ID is self ID");
        case TOX_FAERR_ALREADYSENT:
            return tr("Error: Tox ID is already in friend list");
        case TOX_FAERR_UNKNOWN:
            return tr("Error: Unknown");
        case TOX_FAERR_BADCHECKSUM:
            return tr("Error: Invalid Tox ID (bad checksum)");
        case TOX_FAERR_SETNEWNOSPAM:
            return tr("Error: Invalid Tox ID (bad nospam value)");
        case TOX_FAERR_NOMEM:
            return tr("Error: No memory");
        default:
            return "";
    }
}

bool Cyanide::send_friend_message(int fid, QString msg)
{
    Friend *f = &friends[fid];
    if(!f->connection_status)
        return false;

    uint32_t message_id = tox_send_message(tox, fid, to_tox_string(msg), tox_string_length(msg));
    qDebug() << "message id:" << message_id;

    f->messages.push_back(Message(msg, true));
    emit cyanide.signal_friend_message(self_fid, f->messages.size() - 1);

    return true;
}

bool Cyanide::accept_friend_request(int fid)
{
    if(tox_add_friend_norequest(tox, friends[fid].cid) == -1) {
        qDebug() << "could not add friend";
        return false;
    }

    friends[fid].accepted = true;
    save_needed = true;
    return true;
}

void Cyanide::remove_friend(int fid)
{
    tox_del_friend(tox, fid);
    save_needed = true;
    friends.erase(friends.begin() + fid);
    settings.remove_friend(fid);
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
    self.name_length = tox_string_length(name);
    to_tox_string(name, self.name);
    success = 0 == tox_set_name(tox, self.name, self.name_length);
    Q_ASSERT(success);
    save_needed = true;
    emit signal_name_change(self_fid, NULL);
}

void Cyanide::set_self_status_message(QString status_message)
{
    bool success;
    self.status_message_length = tox_string_length(status_message);
    to_tox_string(status_message, self.status_message);
    success = 0 == tox_set_status_message(tox, self.status_message, self.status_message_length);
    Q_ASSERT(success);
    save_needed = true;
    emit signal_status_message(self_fid);
}

int Cyanide::get_self_user_status()
{
    Q_ASSERT(self.user_status != TOX_USERSTATUS_INVALID);

    switch(self.user_status) {
        case TOX_USERSTATUS_NONE:
            return 0;
        case TOX_USERSTATUS_AWAY:
            return 1;
        case TOX_USERSTATUS_BUSY:
            return 2;
        default:
            return 0;
    }
}

void Cyanide::set_self_user_status(int status)
{
    switch(status) {
        case 0:
            self.user_status = TOX_USERSTATUS_NONE;
            break;
        case 1:
            self.user_status = TOX_USERSTATUS_AWAY;
            break;
        case 2:
            self.user_status = TOX_USERSTATUS_BUSY;
            break;
    }
    tox_set_user_status(tox, self.user_status);
    emit cyanide.signal_user_status(self_fid);
}

int Cyanide::get_number_of_friends()
{
    return friends.size();
}

QString Cyanide::get_friend_name(int fid)
{
    Friend f = fid == -1 ? self : friends[fid];
    return to_QString(f.name, f.name_length);
}

QString Cyanide::get_friend_avatar(int fid)
{
    return QString("qrc:/images/blankavatar");
}

QString Cyanide::get_friend_status_message(int fid)
{
    Friend f = fid == -1 ? self : friends[fid];
    return to_QString(f.status_message, f.status_message_length);
}

QString Cyanide::get_friend_status_icon(int fid)
{
    QString url = "qrc:/images/";
    Friend f = fid == -1 ? self : friends[fid];
    uint8_t status = f.user_status;

    Q_ASSERT(self.user_status != TOX_USERSTATUS_INVALID);

    if(!f.connection_status) {
        url.append("offline");
    } else {
        switch(status) {
        case TOX_USERSTATUS_NONE:
            url.append("online");
            break;
        case TOX_USERSTATUS_AWAY:
            url.append("away");
            break;
        case TOX_USERSTATUS_BUSY:
            url.append("busy");
            break;
        default: //TOX_USERSTATUS_INVALID:
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
    return f.connection_status;
}

bool Cyanide::get_friend_accepted(int fid)
{
    return friends[fid].accepted;
}

QString Cyanide::get_friend_cid(int fid)
{
    uint8_t hex_id[2 * TOX_PUBLIC_KEY_SIZE];
    cid_to_string((char*)hex_id, (char*)friends[fid].cid);
    return to_QString(hex_id, 2 * TOX_PUBLIC_KEY_SIZE);
}

QString Cyanide::get_self_address()
{
    char hex_id[2 * TOX_FRIEND_ADDRESS_SIZE];
    id_to_string((char*)hex_id, (char*)self_id);
    return to_QString(hex_id, 2 * TOX_FRIEND_ADDRESS_SIZE);
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
