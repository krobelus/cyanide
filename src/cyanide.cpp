#include <time.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sailfishapp.h>
#include <thread>

#include "cyanide.h"
#include "tox_bootstrap.h"
#include "tox_callbacks.h"
#include "util.h"

Cyanide cyanide;

Tox_Options options;

int main(int argc, char *argv[])
{
    QGuiApplication *app = SailfishApp::application(argc, argv);
    app->setOrganizationName("Tox");
    app->setOrganizationDomain("Tox");
    app->setApplicationName("Cyanide");
    QQuickView *view = SailfishApp::createView();

    cyanide.load_tox_and_stuff_pretty_please();
    std::thread tox_thread(init, &cyanide);

    view->rootContext()->setContextProperty("cyanide", &cyanide);
    view->setSource(SailfishApp::pathTo("qml/cyanide.qml"));
    view->showFullScreen();

    return app->exec();
}

void init(Cyanide *cyanide)
{
    cyanide->tox_thread();
}

void Cyanide::load_tox_and_stuff_pretty_please()
{
    save_needed = false;

    if((tox = tox_new(&options)) == NULL) {
        qDebug() << "tox_new() failed";
        exit(1);
    }

    sprintf(save_path, "%s/.config/tox/tox_save.tox", getenv("HOME"));

    if(!load_save())
        load_defaults();

    tox_get_address(tox, self_id);
    id_to_string(self_hex_id, (char*)self_id);
    qDebug() << "name:" << to_QString(self.name, self.name_length);
    qDebug() << "status" << to_QString(self.status_message, self.status_message_length);
    qDebug() << "Tox ID: " << self_hex_id;
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
    while(true) {
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

            if (save_needed || (time - last_save >= (uint)100 * 1000 * 1000 * 1000)) {
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

    write_save();

    //TODO wait for threads to return or something

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

    emit cyanide.signal_name_change(self_fid);
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

    //TODO use relative paths
    mkdir("/home/nemo/.config/tox", 775);
    sprintf(tmp_path, "%s/.config/tox/tox_save.tmp", getenv("HOME"));

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
    if(!ch){
        qDebug() << "CHMOD: success";
    } else {
        qDebug() << "CHMOD: failure";
    }

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

        add_friend(f);
    }

    self.name_length = tox_get_self_name(tox, self.name);
    self.status_message_length = tox_get_self_status_message_size(tox);
    tox_get_self_status_message(tox, self.status_message, self.status_message_length);

    emit cyanide.signal_name_change(self_fid);
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
    return;
//    length = utf8_validate(msg, length);
//
//    FRIENDREQ *req = malloc(sizeof(FRIENDREQ) + length);
//
//    req->length = length;
//    memcpy(req->id, id, sizeof(req->id));
//    memcpy(req->msg, msg, length);
//
//    postmessage(FRIEND_REQUEST, 0, 0, req);
//
//    /*int r = tox_add_friend_norequest(tox, id);
//    void *data = malloc(TOX_FRIEND_ADDRESS_SIZE);
//    memcpy(data, id, TOX_FRIEND_ADDRESS_SIZE);
//
//    postmessage(FRIEND_ACCEPT, (r < 0), (r < 0) ? 0 : r, data);*/
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
    memcpy(cyanide.friends[fid].name, newname, length);
    cyanide.friends[fid].name_length = length;
    emit cyanide.signal_name_change(fid);
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
    emit cyanide.signal_user_status();
}

void callback_typing_change(Tox *UNUSED(tox), int fid, uint8_t is_typing, void *UNUSED(userdata))
{
    qDebug() << "was called";
    return;
//    postmessage(FRIEND_TYPING, fid, is_typing, NULL);
//
//    debug("Friend Typing (%u): %u\n", fid, is_typing);
}

void callback_read_receipt(Tox *UNUSED(tox), int fid, uint32_t receipt, void *UNUSED(userdata))
{
    qDebug() << "was called";
    return;
//    //postmessage(FRIEND_RECEIPT, fid, receipt);
//
//    debug("Friend Receipt (%u): %u\n", fid, receipt);
}

void callback_connection_status(Tox *tox, int fid, uint8_t status, void *UNUSED(userdata))
{
    qDebug() << "was called";
    if(status) {
        qDebug() << "Friend " << fid << "now" << (status ? "online" : "offline");
        //tox_request_avatar_info(tox, fid);
    }
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

/* */
bool Cyanide::send_friend_request(QString id_string, QString msg_string)
{
    msg_string += " ";
    int msg_length = tox_string_length(msg_string);
    void *msg = malloc(msg_length * sizeof(char));
    to_tox_string(msg_string, (uint8_t*)msg);

    int id_length = tox_string_length(id_string);
    void *id = malloc(id_length);
    to_tox_string(id_string, (uint8_t*)id);

    void *data = malloc(TOX_FRIEND_ADDRESS_SIZE + msg_length * sizeof(char));
    memcpy(data + TOX_FRIEND_ADDRESS_SIZE, msg, msg_length);

    if(!string_to_id((char*)data, (char*)id)) {
        qDebug() << "string_to_id() failed";
        return false;
    }

    int32_t fid = tox_add_friend(tox, (const uint8_t*)data, (const uint8_t*)data + TOX_FRIEND_ADDRESS_SIZE, msg_length);
    switch(fid) {
        case TOX_FAERR_TOOLONG:
            qDebug() << "TOX_FAERR_TOOLONG";
            break;
        case TOX_FAERR_NOMESSAGE:
            qDebug() << "TOX_FAERR_NOMESSAGE";
            break;
        case TOX_FAERR_OWNKEY:
            qDebug() << "TOX_FAERR_OWNKEY";
            break;
        case TOX_FAERR_ALREADYSENT:
            qDebug() << "TOX_FAERR_ALREADYSENT";
            break;
        case TOX_FAERR_UNKNOWN:
            qDebug() << "TOX_FAERR_UNKNOWN";
            break;
        case TOX_FAERR_BADCHECKSUM:
            qDebug() << "TOX_FAERR_BADCHECKSUM";
            break;
        case TOX_FAERR_SETNEWNOSPAM:
            qDebug() << "TOX_FAERR_SETNEWNOSPAM";
            break;
        case TOX_FAERR_NOMEM:
            qDebug() << "TOX_FAERR_NOMEM";
            break;
        default:
            qDebug() << "friend request was sent";
    }
    Friend f = *new Friend();

    memcpy(f.cid, data, TOX_CLIENT_ID_SIZE);
    f.name_length = TOX_FRIEND_ADDRESS_SIZE;
    memcpy(f.name, id, TOX_FRIEND_ADDRESS_SIZE);
    f.status_message_length = 0;
    add_friend(&f);

    free(msg);
    free(id);
    return true;
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

void Cyanide::remove_friend(int fid)
{
    tox_del_friend(tox, fid);
    save_needed = true;
    friends.erase(friends.begin() + fid);
}

/* setters and getters */

void Cyanide::set_friend_notification(int fid, bool status)
{
    friends[fid].notification = status;
    emit cyanide.signal_connection_status(fid);
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

    switch(status) {
    case TOX_USERSTATUS_NONE:
        url.append(f.connection_status ? "online" : "offline");
        break;
    case TOX_USERSTATUS_AWAY:
        url.append("away");
        break;
    case TOX_USERSTATUS_BUSY:
        url.append("busy");
        break;
    default: //case TOX_USERSTATUS_INVALID:
        qDebug() << "invalid user status_2x";
        url.append("offline");
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

int Cyanide::get_number_of_messages(int fid)
{
    return friends[fid].messages.size();
}

QString Cyanide::get_message_text(int fid, int mid)
{
    return friends[fid].messages[mid].text;
}

QDateTime Cyanide::get_message_timestamp(int fid, int mid)
{
    return friends[fid].messages[mid].timestamp;
}

bool Cyanide::get_message_author(int fid, int mid)
{
    return friends[fid].messages[mid].author;
}
