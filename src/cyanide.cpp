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

    if(argc != 1) {
        sprintf(cyanide.save_path, "%s", argv[1]);
    } else {
        sprintf(cyanide.save_path, "%s/tox_save.tox", CONFIG_PATH);
    }

    cyanide.load_tox_and_stuff_pretty_please();
    std::thread tox_thread(start_tox_thread, &cyanide);

    settings.init();
    cyanide.view->rootContext()->setContextProperty("settings", &settings);
    cyanide.view->rootContext()->setContextProperty("cyanide", &cyanide);
    cyanide.view->setSource(SailfishApp::pathTo("qml/cyanide.qml"));
    cyanide.view->showFullScreen();

    QObject::connect(cyanide.view, SIGNAL(visibilityChanged(QWindow::Visibility)),
                    &cyanide, SLOT(visibility_changed(QWindow::Visibility)));

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
    // TODO switch(error)

    size_t save_data_size;
    const uint8_t *save_data = get_save_data(&save_data_size);
    tox = tox_new(&tox_options, save_data, save_data_size, (TOX_ERR_NEW*)&error);
    // TODO switch(error)

    if(save_data_size == 0 || save_data == NULL)
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
            emit cyanide.signal_friend_connection_status(SELF_FRIEND_NUMBER);
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

        uint32_t interval = tox_iteration_interval(tox);
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

    emit cyanide.signal_friend_name(SELF_FRIEND_NUMBER, NULL);
    emit cyanide.signal_friend_status_message(SELF_FRIEND_NUMBER);
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
    mkdir(AVATAR_PATH, 0755);
    sprintf(tmp_path, "%s.tmp", save_path);

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
        *size = 0;

    return (const uint8_t*)data;
}

void Cyanide::load_tox_data()
{
    TOX_ERR_FRIEND_QUERY error;
    size_t length;
    size_t nfriends = tox_self_get_friend_list_size(tox);
    qDebug() << "Loading" << nfriends << "friends...";

    for(size_t i = 0; i < nfriends; i++) {
        Friend f = *new Friend();

        tox_friend_get_public_key(tox, i, f.public_key, (TOX_ERR_FRIEND_GET_PUBLIC_KEY*)&error);

        char hex_pkey[2 * TOX_PUBLIC_KEY_SIZE + 1];
        public_key_to_string(hex_pkey, (char*)f.public_key);
        hex_pkey[2 * TOX_PUBLIC_KEY_SIZE] = '\0';
        char p[sizeof(AVATAR_PATH) + 2 * TOX_PUBLIC_KEY_SIZE + 5];
        sprintf(p, "%s/%s.png", AVATAR_PATH, hex_pkey);
        uint32_t avatar_size;
        uint8_t *avatar_data = (uint8_t*)file_raw(p, &avatar_size);
        if(avatar_data != NULL) {
            tox_hash(f.avatar_hash, avatar_data, avatar_size);
            free(avatar_data);
        }

        length = tox_friend_get_name_size(tox, i, &error);
        uint8_t name[length];
        if(!tox_friend_get_name(tox, i, name, &error)) {
            if(error == TOX_ERR_FRIEND_QUERY_FRIEND_NOT_FOUND)
                    qDebug() << "Friend not found" << i;
        }
        f.name = utf8_to_qstr(name, length);

        length = tox_friend_get_status_message_size(tox, i, &error);
        uint8_t status_message[length];
        if(!tox_friend_get_status_message(tox, i, status_message, &error)) {
            if(error == TOX_ERR_FRIEND_QUERY_FRIEND_NOT_FOUND)
                    qDebug() << "Friend not found" << i;
        }
        f.status_message = utf8_to_qstr(status_message, length);

        add_friend(&f);
    }

    length = tox_self_get_name_size(tox);
    uint8_t name[length];
    tox_self_get_name(tox, name);
    self.name = utf8_to_qstr(name, length);

    length = tox_self_get_status_message_size(tox);
    uint8_t status_message[length];
    tox_self_get_status_message(tox, status_message);
    self.status_message = utf8_to_qstr(status_message, length);

    emit cyanide.signal_friend_name(SELF_FRIEND_NUMBER, NULL);
    emit cyanide.signal_friend_status_message(SELF_FRIEND_NUMBER);
    save_needed = true;
}

uint32_t Cyanide::add_friend(Friend *f)
{
    uint32_t fid;
    for(fid = 0; fid < UINT32_MAX; fid++) {
        if(friends.count(fid) == 0)
            break;
    }
    friends[fid] = *f;
    emit cyanide.signal_friend_added(fid);
    return fid;
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

    tox_callback_file_recv(tox, callback_file_recv, NULL);
    tox_callback_file_recv_chunk(tox, callback_file_recv_chunk, NULL);
    tox_callback_file_recv_control(tox, callback_file_recv_control, NULL);
    tox_callback_file_chunk_request(tox, callback_file_chunk_request, NULL);
}

void callback_friend_request(Tox *UNUSED(tox), const uint8_t *id, const uint8_t *msg, size_t length, void *UNUSED(userdata))
{
    qDebug() << "was called";
    int name_length = 2 * TOX_ADDRESS_SIZE * sizeof(char);
    void *name = malloc(name_length);
    address_to_string((char*)name, (char*)id);
    Friend *f = new Friend(id, utf8_to_qstr(name, name_length), utf8_to_qstr(msg, length));
    f->accepted = false;
    uint32_t friend_number = cyanide.add_friend(f);
    emit cyanide.signal_friend_request(friend_number);
}

void callback_friend_message(Tox *UNUSED(tox), uint32_t fid, TOX_MESSAGE_TYPE type, const uint8_t *message, size_t length, void *UNUSED(userdata))
{
    qDebug() << "was called";
    //TODO honor type
    std::vector<Message> *tmp = &cyanide.friends[fid].messages;
    tmp->push_back(Message(message, length, false));
    cyanide.friends[fid].notification = true;
    emit cyanide.signal_friend_message(fid, tmp->size() - 1);
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

void callback_file_recv(Tox *tox, uint32_t fid, uint32_t file_number, uint32_t kind,
                        uint64_t file_size, const uint8_t *filename, size_t filename_length, void *UNUSED(user_data))
{
    if(file_size == 0) {
        qDebug() << "ignoring transfer request of size 0";
        return;
    }

    File_Transfer ft;
    ft.friend_number = fid;
    ft.file_number = file_number;
    ft.kind = kind;
    ft.incoming = true;

    ft.file_size = file_size;
    ft.position = 0;

    if(ft.kind == TOX_FILE_KIND_AVATAR) {
        qDebug() << "Received avatar transfer request: friend" << fid << "file" << file_number
                 << "size" << file_size;
        /* check if the hash is the same as the one of the cached avatar */
        cyanide.get_file_id(&ft);
        if(0 == memcmp(cyanide.friends[ft.friend_number].avatar_hash, ft.file_id,
                TOX_HASH_LENGTH)) {
            qDebug() << "avatar already present";
            cyanide.cancel_transfer(&ft);
            return;
        }
        qDebug() << "accepting avatar";
        char p[sizeof(AVATAR_PATH) + 2 * TOX_PUBLIC_KEY_SIZE + 5];
        char hex_pubkey[2 * TOX_PUBLIC_KEY_SIZE + 1];
        public_key_to_string(hex_pubkey, (char*)cyanide.friends[ft.friend_number].public_key);
        hex_pubkey[2 * TOX_PUBLIC_KEY_SIZE] = '\0';
        sprintf(p, "%s/%s.png", AVATAR_PATH, hex_pubkey);
        if((ft.file = fopen(p, "wb")) == NULL) {
            qDebug() << "Failed to open file " << p;
            cyanide.cancel_transfer(&ft);
        } else if((ft.data = (uint8_t*)malloc(ft.file_size)) == NULL) { //TODO sanitize file_size
            qDebug() << "Failed to allocate memory for the avatar";
            fclose(ft.file);
            cyanide.cancel_transfer(&ft);
        } else {
            cyanide.add_file_transfer(&ft);
            cyanide.resume_transfer(&ft);
        }
    } else { // normal transfer
        qDebug() << "Received file transfer request: friend" << fid << "file" << file_number
                 << "size" << file_size;
        ft.filename_length = filename_length;
        ft.filename = (uint8_t*)malloc(filename_length);
        memcpy(ft.filename, filename, filename_length);

        char p[sizeof(DOWNLOAD_PATH) + filename_length];
        sprintf(p, "%s/%s", DOWNLOAD_PATH, filename);
        if((ft.file = fopen(p, "wb")) == NULL)
            qDebug() << "Failed to open file " << p;
    }
}

void callback_file_recv_chunk(Tox *tox, uint32_t fid, uint32_t file_number, uint64_t position,
                              const uint8_t *data, size_t length, void *UNUSED(user_data))
{
    qDebug() << "was called";

    bool success;
    size_t n;
    File_Transfer *ft = cyanide.get_file_transfer(fid, file_number);
    FILE *file = ft->file;

    if(ft->kind == TOX_FILE_KIND_AVATAR) {
        if(length == 0) {
            qDebug() << "avatar transfer finished: friend" << ft->friend_number
                     << "file" << ft->file_number;
            Friend *f = &cyanide.friends[ft->friend_number];
            success = tox_hash(f->avatar_hash, ft->data, ft->file_size);
            Q_ASSERT(success);
            n = fwrite(ft->data, 1, ft->file_size, file);
            Q_ASSERT(n == ft->file_size);
            fclose(ft->file);
            free(ft->data);
            cyanide.remove_file_transfer(ft);
            emit cyanide.signal_avatar_change(ft->friend_number);
        } else {
            qDebug() << "copying avatar chunk to memory, offset" << position;
            memcpy(ft->data + position, data, length);
        }
    } else {
        if(length == 0) {
            qDebug() << "file transfer finished";
            free(ft->filename);
            fclose(ft->file);
            cyanide.remove_file_transfer(ft);
        } else if(fwrite(data, 1, length, file) != length) {
            qDebug() << "fwrite failed";
        }
    }
}

void callback_file_recv_control(Tox *UNUSED(tox), uint32_t friend_number, uint32_t file_number,
                                TOX_FILE_CONTROL action, void *UNUSED(userdata))
{
    qDebug() << "was called";
    File_Transfer *ft = cyanide.get_file_transfer(friend_number, file_number);

    switch(action){
        case TOX_FILE_CONTROL_RESUME:
            qDebug() << "Resuming transfer";
            break;
        case TOX_FILE_CONTROL_PAUSE:
            qDebug() << "Pausing transfer";
            break;
        case TOX_FILE_CONTROL_CANCEL:
            qDebug() << "Aborting transfer";
            break;
    }
}

void callback_file_chunk_request(Tox *tox, uint32_t friend_number, uint32_t file_number, uint64_t position, size_t length, void *UNUSED(user_data))
{
    qDebug() << "was called";

    bool success;
    TOX_ERR_FILE_SEND_CHUNK error;
    uint8_t *chunk = NULL;

    File_Transfer *ft = cyanide.get_file_transfer(friend_number, file_number);
    FILE *file = ft->file;

    if(length != 0) {
        chunk = (uint8_t*)malloc(length);
        if(fread(chunk, 1, length, file) != length) {
            qDebug() << "fread failed";
            return;
        }
    } else {
        cyanide.remove_file_transfer(ft);
    }

    success = tox_file_send_chunk(tox, friend_number, file_number, position,
            (const uint8_t*)chunk, length, &error);
    if(!success) {
        qDebug() << "Failed to send file chunk";
        switch(error) {
            case TOX_ERR_FILE_SEND_CHUNK_OK:
                break;
            case TOX_ERR_FILE_SEND_CHUNK_NULL:
                qDebug() << "chunk is null";
                break;
            case TOX_ERR_FILE_SEND_CHUNK_FRIEND_NOT_FOUND:
                qDebug() << "friend not found";
                break;
            case TOX_ERR_FILE_SEND_CHUNK_FRIEND_NOT_CONNECTED:
                qDebug() << "friend not connected";
                break;
            case TOX_ERR_FILE_SEND_CHUNK_NOT_FOUND:
                qDebug() << "file transfer not found";
                break;
            case TOX_ERR_FILE_SEND_CHUNK_NOT_TRANSFERRING:
                qDebug() << "file transfer inactive";
                break;
            case TOX_ERR_FILE_SEND_CHUNK_INVALID_LENGTH:
                qDebug() << "trying to send invalid length";
                break;
            case TOX_ERR_FILE_SEND_CHUNK_SENDQ:
                qDebug() << "packet queue is full";
                break;
            case TOX_ERR_FILE_SEND_CHUNK_WRONG_POSITION:
                qDebug() << "wrong position";
                break;
            default:
                Q_ASSERT(false);
        }
    }

    free(chunk);
}

bool Cyanide::resume_transfer(File_Transfer *ft)
{
    qDebug() << "resuming transfer, friend_number:" << ft->friend_number << "file_number" << ft->file_number;
    return send_file_control(ft, TOX_FILE_CONTROL_RESUME);
}

bool Cyanide::pause_transfer(File_Transfer *ft)
{
    qDebug() << "pausing transfer, friend_number:" << ft->friend_number << "file_number" << ft->file_number;
    return send_file_control(ft, TOX_FILE_CONTROL_PAUSE);
}

bool Cyanide::cancel_transfer(File_Transfer *ft)
{
    qDebug() << "aborting transfer, friend_number:" << ft->friend_number << "file_number" << ft->file_number;
    return send_file_control(ft, TOX_FILE_CONTROL_CANCEL);
}

bool Cyanide::send_file_control(File_Transfer *ft, TOX_FILE_CONTROL action)
{
    bool success = true;
    TOX_ERR_FILE_CONTROL error;

    success = tox_file_control(tox, ft->friend_number, ft->file_number,
                                    action, &error);
    if(!success) {
        qDebug() << "File control failed";
        switch(error) {
            case TOX_ERR_FILE_CONTROL_OK:
                break;
            case TOX_ERR_FILE_CONTROL_FRIEND_NOT_FOUND:
                qDebug() << "Error: friend not found";
                break;
            case TOX_ERR_FILE_CONTROL_FRIEND_NOT_CONNECTED:
                qDebug() << "Error: friend not connected";
                break;
            case TOX_ERR_FILE_CONTROL_NOT_FOUND:
                qDebug() << "Error: file transfer not found";
                break;
            case TOX_ERR_FILE_CONTROL_NOT_PAUSED:
                qDebug() << "Error: not paused";
                break;
            case TOX_ERR_FILE_CONTROL_DENIED:
                qDebug() << "Error: transfer is paused by peer";
                break;
            case TOX_ERR_FILE_CONTROL_ALREADY_PAUSED:
                qDebug() << "Error: already paused";
                break;
            case TOX_ERR_FILE_CONTROL_SENDQ:
                qDebug() << "Error: packet queue is full";
                break;
            default:
                Q_ASSERT(false);
        }
        return false;
    }

    return success;
}

bool Cyanide::get_file_id(File_Transfer *ft)
{
    TOX_ERR_FILE_GET error;

    if(!tox_file_get_file_id(tox, ft->friend_number, ft->file_number
                            , ft->file_id, &error)) {
        qDebug() << "Failed to get file id";
        switch(error) {
            case TOX_ERR_FILE_GET_OK:
                break;
            case TOX_ERR_FILE_GET_FRIEND_NOT_FOUND:
                qDebug() << "Error: file not found";
                break;
            case TOX_ERR_FILE_GET_NOT_FOUND:
                qDebug() << "Error: file transfer not found";
                break;
        }
        return false;
    }
    return true;
}

bool Cyanide::send_file(int fid, QString path)
{
    int error;
    TOX_FILE_KIND kind = TOX_FILE_KIND_DATA;

    File_Transfer ft;
    ft.friend_number = fid;
    ft.kind = kind;
    ft.incoming = false;

    ft.filename_length = qstrlen(path);
    ft.filename = (uint8_t*)malloc(ft.filename_length);
    qstr_to_utf8(ft.filename, path);

    char p[qstrlen(path)];
    qstr_to_utf8((uint8_t*)p, path);
    if((ft.file = fopen(p, "rb")) == NULL) {
        qDebug() << "Failed to open file" << path;
        return false;
    }
    fseek(ft.file, 0L, SEEK_END);
    ft.file_size = ftell(ft.file);
    rewind(ft.file);
    ft.position = 0;

    ft.file_number = tox_file_send(tox, ft.friend_number, ft.kind, ft.file_size,
                                    NULL, ft.filename, ft.filename_length,
                                    (TOX_ERR_FILE_SEND*)&error);
    qDebug() << "sending file, friend number" << ft.friend_number
             << "file number" << ft.file_number;

    switch(error) {
        case TOX_ERR_FILE_SEND_OK:
            break;
        case TOX_ERR_FILE_SEND_FRIEND_NOT_FOUND:
            qDebug() << "Error: friend not found.";
            return false;
        case TOX_ERR_FILE_SEND_FRIEND_NOT_CONNECTED:
            qDebug() << "Error: friend not connected.";
            return false;
        case TOX_ERR_FILE_SEND_NAME_TOO_LONG:
            qDebug() << "Error: filename too long.";
            return false;
        case TOX_ERR_FILE_SEND_TOO_MANY:
            qDebug() << "Error: Too many ongoing transfers.";
            return false;
        default:
            Q_ASSERT(false);
    }

    //success = get_file_id(&ft);
    //Q_ASSERT(success);
    add_file_transfer(&ft);

    return true;
}

void Cyanide::add_file_transfer(File_Transfer *ft)
{
    uint64_t id = (uint64_t)ft->friend_number << 32 | ft->file_number;
    file_transfers[id] = *ft;
}

void Cyanide::remove_file_transfer(File_Transfer *ft)
{
    uint64_t id = (uint64_t)ft->friend_number << 32 | ft->file_number;
    file_transfers.erase(id);
}

File_Transfer* Cyanide::get_file_transfer(uint32_t friend_number, uint32_t file_number)
{
    uint64_t id = (uint64_t)friend_number << 32 | file_number;
    // TODO bounds checking :)
    return &file_transfers[id];
}

void Cyanide::send_typing_notification(int fid, bool typing)
{
    TOX_ERR_SET_TYPING error;
    if(settings.get("send-typing-notifications") == "true")
        tox_self_set_typing(tox, fid, typing, &error);
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
        Friend *f = new Friend((const uint8_t*)address, id_str, "");
        uint32_t fid = add_friend(f);
        char hex_address[2 * TOX_ADDRESS_SIZE + 1];
        address_to_string(hex_address, (char*)address);
        hex_address[2 * TOX_ADDRESS_SIZE] = '\0';
        settings.add_friend_address(get_friend_public_key(fid), hex_address);
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
    emit cyanide.signal_friend_message(SELF_FRIEND_NUMBER, f->messages.size() - 1);

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
        settings.remove_friend(get_friend_public_key(fid));
        friends.erase(fid);
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

QList<int> Cyanide::get_friend_numbers()
{
    QList<int> friend_numbers;
    for(auto it = friends.begin(); it != friends.end(); it++) {
        friend_numbers.append(it->first);
    }
    return friend_numbers;
}

void Cyanide::set_friend_notification(int fid, bool status)
{
    Q_ASSERT(fid != SELF_FRIEND_NUMBER);
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
    emit signal_friend_name(SELF_FRIEND_NUMBER, NULL);
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
    emit signal_friend_status_message(SELF_FRIEND_NUMBER);
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
    emit cyanide.signal_friend_status(SELF_FRIEND_NUMBER);
}

int Cyanide::get_number_of_friends()
{
    return friends.size();
}

QString Cyanide::get_friend_name(int fid)
{
    Friend f = fid == SELF_FRIEND_NUMBER ? self : friends[fid];
    return f.name;
}

QString Cyanide::get_friend_avatar(int fid)
{
    QString avatar = AVATAR_PATH + QString("/") + get_friend_public_key(fid) + QString(".png");
    return QFile::exists(avatar) ? avatar : "qrc:/images/blankavatar";
}

QString Cyanide::get_friend_status_message(int fid)
{
    Friend f = fid == SELF_FRIEND_NUMBER ? self : friends[fid];
    return f.status_message;
}

QString Cyanide::get_friend_status_icon(int fid)
{
    QString url = "qrc:/images/";
    Friend f = fid == SELF_FRIEND_NUMBER ? self : friends[fid];

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
    Friend f = fid == SELF_FRIEND_NUMBER ? self : friends[fid];
    return f.connection_status != TOX_CONNECTION_NONE;
}

bool Cyanide::get_friend_accepted(int fid)
{
    Q_ASSERT(fid != SELF_FRIEND_NUMBER);
    return friends[fid].accepted;
}

QString Cyanide::get_friend_public_key(int fid)
{
    Friend f = fid == SELF_FRIEND_NUMBER ? self : friends[fid];
    uint8_t hex_id[2 * TOX_PUBLIC_KEY_SIZE];
    public_key_to_string((char*)hex_id, (char*)f.public_key);
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
    Q_ASSERT(fid != SELF_FRIEND_NUMBER);
    return friends[fid].messages.size();
}

QString Cyanide::get_message_text(int fid, int mid)
{
    Q_ASSERT(fid != SELF_FRIEND_NUMBER);
    return friends[fid].messages[mid].text;
}

QString Cyanide::get_message_rich_text(int fid, int mid)
{
    Q_ASSERT(fid != SELF_FRIEND_NUMBER);
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
    Q_ASSERT(fid != SELF_FRIEND_NUMBER);
    return friends[fid].messages[mid].timestamp;
}

bool Cyanide::get_message_author(int fid, int mid)
{
    Q_ASSERT(fid != SELF_FRIEND_NUMBER);
    return friends[fid].messages[mid].author;
}
