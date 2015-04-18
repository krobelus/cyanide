#include <stdlib.h>
#include <sys/eventfd.h>
#include <sys/stat.h>
#include <thread>
#include <time.h>

#include <sailfishapp.h>
#include <mlite5/mnotificationgroup.h>
#include <mlite5/mnotification.h>
#include <mlite5/mremoteaction.h>
#include <QSound>
#include <QtQuick>
#include <QTranslator>

#include "cyanide.h"
#include "tox_bootstrap.h"
#include "tox_callbacks.h"
#include "util.h"
#include "dns.cpp"
#include "config.h"
#include "settings.h"

/* oh boy, here we go... */
#define UINT32_MAX (4294967295U)

Cyanide cyanide;
Settings settings;

struct Tox_Options tox_options;

int main(int argc, char *argv[])
{
    QGuiApplication *app = SailfishApp::application(argc, argv);
    app->setOrganizationName("Tox");
    app->setOrganizationDomain("Tox");
    app->setApplicationName("Cyanide");
    cyanide.view = SailfishApp::createView();

    bool success;
    success = QDBusConnection::sessionBus().registerObject("/", &cyanide, QDBusConnection::ExportScriptableContents)
            && QDBusConnection::sessionBus().registerService("harbour.cyanide");

    if(!success)
        QGuiApplication::exit(0);

    cyanide.read_default_profile(app->arguments());

    cyanide.eventfd = eventfd(0, 0);
    cyanide.check_wifi();
    cyanide.wifi_monitor();

    std::thread my_tox_thread(start_tox_thread, &cyanide);

    cyanide.view->rootContext()->setContextProperty("cyanide", &cyanide);
    cyanide.view->rootContext()->setContextProperty("settings", &settings);
    cyanide.view->setSource(SailfishApp::pathTo("qml/cyanide.qml"));
    cyanide.view->showFullScreen();

    QObject::connect(cyanide.view, SIGNAL(visibilityChanged(QWindow::Visibility)),
                    &cyanide, SLOT(visibility_changed(QWindow::Visibility)));

    int result = app->exec();

    cyanide.loop = LOOP_FINISH;
    settings.close_databases();
    my_tox_thread.join();

    return result;
}

QString Cyanide::tox_save_file()
{
    return tox_save_file(profile_name);
}

QString Cyanide::tox_save_file(QString name)
{
    return TOX_DATA_DIR + name.replace('/', '_') + ".tox";
}

/* sets profile_name */
void Cyanide::read_default_profile(QStringList args)
{
    for(int i = 0; i < args.size(); i++) {
        if(args[i].startsWith("tox:")) {
            //TODO
        }
    }

    QFile file(DEFAULT_PROFILE_FILE);
    if(file.exists()) {
        file.open(QIODevice::ReadOnly | QIODevice::Text);
        profile_name = file.readLine();
        file.close();
        profile_name.chop(1);
        if(QFile::exists(tox_save_file()))
            return;
    }

    profile_name = DEFAULT_PROFILE_NAME;
    write_default_profile();
}

void Cyanide::write_default_profile()
{
    QFile file(DEFAULT_PROFILE_FILE);
    file.open(QIODevice::WriteOnly | QIODevice::Text);
    file.write(profile_name.toUtf8() + '\n');
    file.close();
}

void Cyanide::reload()
{
    loop = LOOP_RELOAD;
}

void Cyanide::load_new_profile()
{
    QString path;
    int i = 0;
    do {
        i++;
        path = TOX_DATA_DIR + "id" + QString::number(i) + ".tox";
    } while(QFile::exists(path));
    QFile file(path);
    file.open(QIODevice::WriteOnly);
    file.close();
    load_tox_save_file(path);
}

void Cyanide::delete_current_profile()
{
    bool success;
    loop = LOOP_STOP;
    usleep(1500 * MAX_ITERATION_TIME);
    success = QFile::remove(tox_save_file());
    qDebug() << "removed" << tox_save_file() << ":" << success;
    success = QFile::remove(CYANIDE_DATA_DIR + profile_name.replace('/','_') + ".sqlite");
    qDebug() << "removed db:" << success;
}

void Cyanide::load_tox_save_file(QString path)
{
    QString basename = path.mid(path.lastIndexOf('/') + 1);
    if(!basename.endsWith(".tox"))
        basename += ".tox";

    /* ensure that the save file is in ~/.config/tox */
    QFile::copy(path, TOX_DATA_DIR + basename);

    /* remove the .tox extension */
    basename.chop(4);
    next_profile_name = basename;

    if(loop == LOOP_STOP) {
        // tox_loop not running, resume it
        profile_name = basename;
        resume_thread();
    } else {
        loop = LOOP_RELOAD_OTHER;
    }
}

void Cyanide::check_wifi()
{
    if(settings.get("wifi-only") == "true") {
        if(0 == system("test true = \"$(dbus-send --system --dest=net.connman --print-reply"
                                      " /net/connman/technology/wifi net.connman.Technology.GetProperties"
                                      "| grep -A1 Connected | sed -e 1d -e 's/^.*\\s//')\"")) {
            qDebug() << "connected via wifi, toxing";
            if(loop == LOOP_SUSPEND)
                resume_thread();
            else
                loop = LOOP_RUN;
        } else {
            qDebug() << "not connected via wifi, not toxing";
            suspend_thread();
        }
    } else {
        if(loop == LOOP_SUSPEND)
            resume_thread();
        else
            loop = LOOP_RUN;
    }
}

void Cyanide::wifi_monitor()
{
    QDBusConnection dbus = QDBusConnection::systemBus();

    if(!dbus.isConnected()) {
        qDebug() << "Failed to connect to the D-Bus session bus.";
    }
    dbus.connect("net.connman",
                 "/net/connman/technology/wifi",
                 "net.connman.Technology",
                 "PropertyChanged",
                 &cyanide,
                 SLOT(wifi_changed(QString, QDBusVariant)));
}

void Cyanide::wifi_changed(QString name, QDBusVariant dbus_variant)
{
    bool value = dbus_variant.variant().toBool();
    qDebug() << "Received DBus signal";
    qDebug() << "Property" << name << "Value" << value;

    if(name == "Powered") {
        ;
    } else if(name == "Connected" && settings.get("wifi-only") == "true") {
        if(value && loop == LOOP_SUSPEND) {
            qDebug() << "connected, resuming thread";
            resume_thread();
        } else if(!value && loop == LOOP_RUN) {
            qDebug() << "not connected, suspending thread";
            suspend_thread();
        }
    }
}

void Cyanide::resume_thread()
{
    loop = LOOP_RUN;
    uint64_t event = 1;
    ssize_t tmp = write(eventfd, &event, sizeof(event));
    Q_ASSERT(tmp == sizeof(event));
}

void Cyanide::suspend_thread()
{
    loop = LOOP_SUSPEND;
    self.connection_status = TOX_CONNECTION_NONE;
    emit signal_friend_connection_status(SELF_FRIEND_NUMBER, false);
    for(auto it = friends.begin(); it != friends.end(); it++) {
        it->second.connection_status = TOX_CONNECTION_NONE;
        emit signal_friend_connection_status(it->first, false);
    }
    usleep(1500 * MAX_ITERATION_TIME);
}

void Cyanide::visibility_changed(QWindow::Visibility visibility)
{
    /* remove all notifications for now until I find a proper solution
     * (because the error messages are show too)
     */
    for(std::pair<uint32_t, Friend>pair : friends) {
        Friend f = pair.second;
        if(f.notification != NULL) {
            f.notification = NULL;
        }
    }
    for(MNotification *n : MNotification::notifications()) {
        n->remove();
    }
}

void Cyanide::message_notification_activated(int fid)
{
    qDebug() << QString(); /* quality code */
    emit signal_focus_friend(fid);
    raise();
}

void Cyanide::notify_error(QString summary, QString body)
{
    MNotification *n = new MNotification("", summary, body);
    n->publish();
}

void Cyanide::notify_message(int fid, QString summary, QString body)
{
    Q_ASSERT(fid != SELF_FRIEND_NUMBER);
    Friend *f = &friends[fid];

    if(f->notification != NULL) {
        f->notification->remove();
    }
    f->notification = new MNotification("harbour.cyanide.message", summary, body);
    MRemoteAction action("harbour.cyanide", "/", "harbour.cyanide", "message_notification_activated",
            QVariantList() << fid);
    f->notification->setAction(action);
    f->notification->publish();
}

void Cyanide::notify_call(int fid, QString summary, QString body)
{
    Q_ASSERT(fid != SELF_FRIEND_NUMBER);
    Friend *f = &friends[fid];

    if(f->notification != NULL) {
        f->notification->remove();
    }
    f->notification = new MNotification("harbour.cyanide.call", summary, body);
    MRemoteAction action("harbour.cyanide", "/", "harbour.cyanide", "message_notification_activated",
            QVariantList() << fid);
    f->notification->setAction(action);
    f->notification->publish();
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

    settings.open_database(profile_name);

    self.user_status = TOX_USER_STATUS_NONE;
    self.connection_status = TOX_CONNECTION_NONE;
    memset(&self.avatar_transfer, 0, sizeof(File_Transfer));

    //TODO use this destructor thingy
    for(std::pair<uint32_t, Friend>pair : friends) {
        Friend f = pair.second;
        free(f.avatar_transfer.filename);
        f.files.clear();
        for(Message m : f.messages) {
            if(m.ft != NULL) {
                free(m.ft->filename);
                free(m.ft);
            }
        }
        f.messages.clear();
    }
    friends.clear();

    save_needed = false;

    // TODO free
    tox_options = *tox_options_new((TOX_ERR_OPTIONS_NEW*)&error);
    // TODO switch(error)

    size_t save_data_size;
    const uint8_t *save_data = get_save_data(&save_data_size);
    if(settings.get("udp-enabled") != "true")
        tox_options.udp_enabled = 0;
    tox = tox_new(&tox_options, save_data, save_data_size, (TOX_ERR_NEW*)&error);
    // TODO switch(error)

    tox_self_get_address(tox, self_address);
    tox_self_get_public_key(tox, self.public_key);

    QString public_key = get_friend_public_key(SELF_FRIEND_NUMBER);
    settings.add_friend_if_not_exists(public_key);

    QByteArray saved_hash = settings.get_friend_avatar_hash(public_key);
    memcpy(self.avatar_hash, saved_hash.constData(), TOX_HASH_LENGTH);

    if(save_data_size == 0 || save_data == NULL)
        load_defaults();
    else
        load_tox_data();

    emit signal_friend_added(SELF_FRIEND_NUMBER);
    emit signal_avatar_change(SELF_FRIEND_NUMBER);

    qDebug() << "Name:" << self.name;
    qDebug() << "Status" << self.status_message;
    qDebug() << "Tox ID" << get_self_address();
}

void start_tox_thread(Cyanide *cyanide)
{
    cyanide->tox_thread();
}

void start_toxav_thread(Cyanide *cyanide)
{
    cyanide->toxav_thread();
}

void Cyanide::tox_thread()
{
    qDebug() << "profile name" << profile_name;
    qDebug() << "tox_save_file" << tox_save_file();

    load_tox_and_stuff_pretty_please();

    set_callbacks();

    // Connect to bootstraped nodes in "tox_bootstrap.h"

    do_bootstrap();

    // Start the tox av session.
    toxav = toxav_new(tox, MAX_CALLS);

    // Give toxcore the av functions to call
    set_av_callbacks();

    check_wifi();

    tox_loop();
}

void Cyanide::tox_loop()
{
    std::thread av_thread(start_toxav_thread, &cyanide);

    uint64_t last_save = get_time(), time;
    TOX_CONNECTION c, connection = c = TOX_CONNECTION_NONE;

    while(loop == LOOP_RUN) {
        // Put toxcore to work
        tox_iterate(tox);

        // Check current connection
        if((c = tox_self_get_connection_status(tox)) != connection) {
            self.connection_status = connection = c;
            emit signal_friend_connection_status(SELF_FRIEND_NUMBER, c != TOX_CONNECTION_NONE);
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
        usleep(1000 * MIN(interval, MAX_ITERATION_TIME));
    }

    if(loop != LOOP_SUSPEND)
        av_thread.join();

    uint64_t event;
    ssize_t tmp;
    switch(loop) {
        case LOOP_RUN:
            Q_ASSERT(false);
            break;
        case LOOP_FINISH:
            qDebug() << "exiting...";

            killall_tox();

            break;
        case LOOP_RELOAD:
            killall_tox();
            tox_thread();
            break;
        case LOOP_RELOAD_OTHER:
            qDebug() << "loading profile" << next_profile_name;

            killall_tox();

            profile_name = next_profile_name;

            write_default_profile();

            tox_thread();
            break;
        case LOOP_SUSPEND:
            tmp = read(eventfd, &event, sizeof(event));
            Q_ASSERT(tmp == sizeof(event));
            qDebug() << "read" << event << ", resuming thread";
            tox_loop();
            break;
        case LOOP_STOP:
            killall_tox();
            tmp = read(eventfd, &event, sizeof(event));
            Q_ASSERT(tmp = sizeof(event));
            qDebug() << "read" << event << ", starting thread with profile" << profile_name
                        << "save file" << tox_save_file();
            write_default_profile();
            tox_thread();
            break;
    }
}

void Cyanide::toxav_thread()
{
    while(loop == LOOP_RUN || loop == LOOP_SUSPEND) {
        toxav_do(toxav);
        usleep(toxav_do_interval(toxav));
    }
    switch(loop) {
        case LOOP_RUN:
            Q_ASSERT(false);
            break;
        case LOOP_FINISH:
        case LOOP_RELOAD:
        case LOOP_RELOAD_OTHER:
            break;
        case LOOP_SUSPEND:
            Q_ASSERT(false);
            break;
        case LOOP_STOP:
            break;
    }
}

void Cyanide::killall_tox()
{
    toxav_kill(toxav);
    kill_tox();
}

void Cyanide::kill_tox()
{
    TOX_ERR_FRIEND_ADD error;

    /* re-add all blocked friends */
    for(auto it = friends.begin(); it != friends.end(); it++) {
        Friend *f = &it->second;

        if(f->blocked) {
            tox_friend_add_norequest(tox, f->public_key, &error);

            if(error == TOX_ERR_FRIEND_ADD_MALLOC) {
                qDebug() << "memory allocation failure";
            } else {
                f->blocked = false;
            }
        }
    }

    write_save();

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

    emit signal_friend_name(SELF_FRIEND_NUMBER, NULL);
    emit signal_friend_status_message(SELF_FRIEND_NUMBER);
    save_needed = true;
}

void Cyanide::write_save()
{
    void *data;
    uint32_t size;

    size = tox_get_savedata_size(tox);
    data = malloc(size);
    tox_get_savedata(tox, (uint8_t*)data);

    QDir().mkpath(TOX_DATA_DIR);
    QDir().mkpath(TOX_AVATAR_DIR);

    QSaveFile file(tox_save_file());
    if(!file.open(QIODevice::WriteOnly)) {
        qDebug() << "failed to open save file";
    }

    file.write((const char*)data, size);

    file.commit();

    save_needed = false;
    free(data);
}

const uint8_t* Cyanide::get_save_data(size_t *size)
{
    void *data;

    data = file_raw(tox_save_file().toUtf8().data(), size);
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

        uint8_t hex_id[2 * TOX_PUBLIC_KEY_SIZE];
        public_key_to_string((char*)hex_id, (char*)f.public_key);
        QString public_key = utf8_to_qstr(hex_id, 2 * TOX_PUBLIC_KEY_SIZE);
        settings.add_friend_if_not_exists(public_key);
        QByteArray saved_hash = settings.get_friend_avatar_hash(public_key);
        memcpy(f.avatar_hash, saved_hash.constData(), TOX_HASH_LENGTH);

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

    emit signal_friend_name(SELF_FRIEND_NUMBER, NULL);
    emit signal_friend_status_message(SELF_FRIEND_NUMBER);
    save_needed = true;
}

uint32_t Cyanide::add_friend(Friend *f)
{
    uint32_t fid = next_friend_number();
    friends[fid] = *f;
    emit signal_friend_added(fid);
    return fid;
}

/* find out the friend number that toxcore will assign when using
 * tox_friend_add() and tox_friend_add_norequest() */
uint32_t Cyanide::next_friend_number()
{
    uint32_t fid;
    for(fid = 0; fid < UINT32_MAX; fid++) {
        if(friends.count(fid) == 0 || friends[fid].blocked)
            break;
    }
    return fid;
}

uint32_t Cyanide::next_but_one_unoccupied_friend_number()
{
    int count = 0;
    uint32_t fid;
    for(fid = 0; fid < UINT32_MAX; fid++) {
        if(friends.count(fid) == 0 && count++)
            break;
    }
    return fid;
}

void Cyanide::relocate_blocked_friend()
{
    uint32_t from = next_friend_number();

    if(friends.count(from) == 1 && friends[from].blocked) {
        uint32_t to = next_but_one_unoccupied_friend_number();
        friends[to] = friends[from];
        friends.erase(from);
        emit signal_friend_added(to);
    }
}

void Cyanide::add_message(uint32_t fid, Message message)
{
    friends[fid].messages.append(message);
    uint32_t mid = friends[fid].messages.size() - 1;
    qDebug() << "added message number" << mid;

    if(message.ft != NULL)
        friends[fid].files[message.ft->file_number] = mid;

    if(!message.author)
        set_friend_activity(fid, true);

    emit signal_friend_message(fid, mid, message.type);
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

void Cyanide::set_av_callbacks()
{
    toxav_register_callstate_callback(toxav, (ToxAVCallback)callback_av_invite, av_OnInvite, NULL);
    toxav_register_callstate_callback(toxav, (ToxAVCallback)callback_av_start, av_OnStart, NULL);
    toxav_register_callstate_callback(toxav, (ToxAVCallback)callback_av_cancel, av_OnCancel, NULL);
    toxav_register_callstate_callback(toxav, (ToxAVCallback)callback_av_reject, av_OnReject, NULL);
    toxav_register_callstate_callback(toxav, (ToxAVCallback)callback_av_end, av_OnEnd, NULL);

    toxav_register_callstate_callback(toxav, (ToxAVCallback)callback_av_ringing, av_OnRinging, NULL);

    toxav_register_callstate_callback(toxav, (ToxAVCallback)callback_av_requesttimeout, av_OnRequestTimeout, NULL);
    toxav_register_callstate_callback(toxav, (ToxAVCallback)callback_av_peertimeout, av_OnPeerTimeout, NULL);
    toxav_register_callstate_callback(toxav, (ToxAVCallback)callback_av_selfmediachange, av_OnSelfCSChange, NULL);
    toxav_register_callstate_callback(toxav, (ToxAVCallback)callback_av_peermediachange, av_OnPeerCSChange, NULL);

    toxav_register_audio_callback(toxav, (ToxAvAudioCallback)callback_av_audio, NULL);
    toxav_register_video_callback(toxav, (ToxAvVideoCallback)callback_av_video, NULL);
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

void callback_friend_message(Tox *UNUSED(tox), uint32_t fid, TOX_MESSAGE_TYPE type, const uint8_t *msg, size_t length, void *UNUSED(userdata))
{
    qDebug() << "was called";
    Message m;
    m.type = type == TOX_MESSAGE_TYPE_ACTION ? MSGTYPE_ACTION : MSGTYPE_NORMAL;
    m.author = false;
    m.text = utf8_to_qstr(msg, length);
    m.timestamp = QDateTime::currentDateTime();
    m.ft = NULL;
    cyanide.add_message(fid, m);
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
    // TODO
    qDebug() << "was called";
}

void callback_friend_connection_status(Tox *tox, uint32_t fid, TOX_CONNECTION status, void *UNUSED(userdata))
{
    Friend *f = &cyanide.friends[fid];
    f->connection_status = status;
    if(status != TOX_CONNECTION_NONE) {
        qDebug() << "Sending avatar to friend" << fid;
        QString errmsg = cyanide.send_avatar(fid);
        if(errmsg != "")
            qDebug() << "Failed to send avatar. " << errmsg;
    }
    emit cyanide.signal_friend_connection_status(fid, status != TOX_CONNECTION_NONE);
}

void callback_file_recv(Tox *tox, uint32_t fid, uint32_t file_number, uint32_t kind,
                        uint64_t file_size, const uint8_t *filename, size_t filename_length, void *UNUSED(user_data))
{
    if(kind == TOX_FILE_KIND_AVATAR)
        return cyanide.incoming_avatar(fid, file_number, file_size, filename, filename_length);

    qDebug() << "Received file transfer request: friend" << fid << "file" << file_number
             << "size" << file_size;

    Message m;
    m.type = MSGTYPE_FILE;

    m.timestamp = QDateTime::currentDateTime();
    m.author = false;

    File_Transfer *ft = m.ft = (File_Transfer*)malloc(sizeof(File_Transfer));
    if(ft == NULL) {
        qDebug() << "Failed to allocate memory for the file transfer";
        return;
    }

    if(file_size == 0) {
        free(m.ft);
        m.ft = NULL;
        qDebug() << "ignoring transfer request of size 0";
        return;
    }

    ft->file_number = file_number;
    ft->file_size = file_size;
    ft->position = 0;
    ft->filename_length = filename_length;
    ft->status = -1;
    ft->filename = (uint8_t*)malloc(filename_length);
    memcpy(ft->filename, filename, filename_length);

    m.text = DOWNLOAD_DIR + utf8_to_qstr(filename, filename_length);

    if((ft->file = fopen(m.text.toUtf8().constData(), "wb")) == NULL)
        qDebug() << "Failed to open file " << m.text;

    cyanide.add_message(fid, m);
}

void Cyanide::incoming_avatar(uint32_t fid, uint32_t file_number, uint64_t file_size,
                              const uint8_t *filename, size_t filename_length)
{
    qDebug() << "Received avatar transfer request: friend" << fid
             << "file" << file_number << "size" << file_size;

    friends[fid].files[file_number] = -2;

    File_Transfer *ft = &friends[fid].avatar_transfer;
    ft->file_number = file_number;
    ft->file_size = file_size;
    ft->position = 0;
    ft->file = NULL;
    ft->data = NULL;
    ft->status = -2;

    /* check if the hash is the same as the one of the cached avatar */
    cyanide.get_file_id(fid, ft);
    if(0 == memcmp(cyanide.friends[fid].avatar_hash, ft->file_id,
            TOX_HASH_LENGTH)) {
        qDebug() << "avatar already present";
        goto cancel;
    }

    ft->filename_length = filename_length;
    ft->filename = (uint8_t*)malloc(filename_length);
    memcpy(ft->filename, filename, filename_length);
    char p[TOX_AVATAR_DIR.size() + 2 * TOX_PUBLIC_KEY_SIZE + 4];
    char hex_pubkey[2 * TOX_PUBLIC_KEY_SIZE + 1];
    public_key_to_string(hex_pubkey, (char*)cyanide.friends[fid].public_key);
    hex_pubkey[2 * TOX_PUBLIC_KEY_SIZE] = '\0';
    sprintf(p, "%s%s.png", TOX_AVATAR_DIR.toUtf8().constData(), hex_pubkey);

    if(file_size == 0) {
        unlink(p);
        memset(friends[fid].avatar_hash, 0, TOX_HASH_LENGTH);
        QByteArray hash((const char*)friends[fid].avatar_hash, TOX_HASH_LENGTH);
        settings.set_friend_avatar_hash(cyanide.get_friend_public_key(fid), hash);
        emit signal_avatar_change(fid);
        goto cancel;
    } else if(file_size > MAX_AVATAR_SIZE) {
        qDebug() << "avatar too large, rejecting";
        goto cancel;
    } else if((ft->file = fopen(p, "wb")) == NULL) {
        qDebug() << "Failed to open file " << p;
        goto cancel;
    } else if((ft->data = (uint8_t*)malloc(ft->file_size)) == NULL) {
        qDebug() << "Failed to allocate memory for the avatar";
        goto cancel;
    } else {
        QString errmsg = cyanide.send_file_control(fid, -2, TOX_FILE_CONTROL_RESUME);
        if(errmsg == "")
            return;
        else
            qDebug() << "failed to resume avatar transfer: " << errmsg;
    }
cancel:
    QString errmsg = cyanide.send_file_control(fid, -2, TOX_FILE_CONTROL_CANCEL);
    if(errmsg != "")
        qDebug() << "failed to cancel avatar transfer: " << errmsg;
    free(ft->data);
    if(ft->file != NULL)
        fclose(ft->file);
}

void Cyanide::incoming_avatar_chunk(uint32_t fid, uint64_t position,
                                    const uint8_t *data, size_t length)
{
    bool success;
    size_t n;

    Friend *f = &cyanide.friends[fid];
    File_Transfer ft = f->avatar_transfer;

    if(length == 0) {
        qDebug() << "avatar transfer finished: friend" << fid
                 << "file" << ft.file_number;
        success = tox_hash(f->avatar_hash, ft.data, ft.file_size);
        QByteArray hash((const char*)f->avatar_hash, TOX_HASH_LENGTH);
        settings.set_friend_avatar_hash(cyanide.get_friend_public_key(fid), hash);
        Q_ASSERT(success);
        n = fwrite(ft.data, 1, ft.file_size, ft.file);
        Q_ASSERT(n == ft.file_size);
        fclose(ft.file);
        free(ft.data);
        emit signal_avatar_change(fid);
    } else {
        memcpy(ft.data + position, data, length);
    }
}

void callback_file_recv_chunk(Tox *tox, uint32_t fid, uint32_t file_number, uint64_t position,
                              const uint8_t *data, size_t length, void *UNUSED(user_data))
{
    qDebug() << "was called";

    Friend *f = &cyanide.friends[fid];
    int mid = f->files[file_number];

    if(mid == -1) {
        Q_ASSERT(false);
    } else if(mid == -2) {
        return cyanide.incoming_avatar_chunk(fid, position, data, length);
    }

    File_Transfer *ft = f->messages[mid].ft;
    FILE *file = ft->file;

    if(length == 0) {
        qDebug() << "file transfer finished";
        fclose(ft->file);
        ft->status = 2;
        emit cyanide.signal_file_status(fid, mid, ft->status);
        emit cyanide.signal_file_progress(fid, mid, 100);
    } else if(fwrite(data, 1, length, file) != length) {
        qDebug() << "fwrite failed";
    } else {
        if(ft->file_size == 0) {
            qDebug() << "receiving file with size 0";
            qDebug() << "lol";
        } else {
            ft->position += length;
            emit cyanide.signal_file_progress(fid, mid, cyanide.get_file_progress(fid, mid));
        }
    }
}

void callback_file_recv_control(Tox *UNUSED(tox), uint32_t fid, uint32_t file_number,
                                TOX_FILE_CONTROL action, void *UNUSED(userdata))
{
    qDebug() << "was called";

    Friend *f = &cyanide.friends[fid];
    int mid  = f->files[file_number];
    qDebug() << "message id is" << mid << "file number is" << file_number;

    Message *m = NULL;
    File_Transfer *ft;

    if(mid == -1) {
        qDebug() << "outgoing avatar transfer";
        ft = &cyanide.self.avatar_transfer;
    } else if(mid == -2) {
        qDebug() << "incoming avatar transfer";
        ft = &f->avatar_transfer;
    } else {
        qDebug() << "normal transfer";
        m = &f->messages[mid];
        ft = m->ft;
    }

    switch(action){
        case TOX_FILE_CONTROL_RESUME:
            qDebug() << "transfer was resumed, status:" << ft->status;
            if(ft->status == -2) {
                ft->status = 1;
            } else if(ft->status == -3) {
                ft->status = -1;
            } else {
                qDebug() << "unexpected status ^";
            }
            break;
        case TOX_FILE_CONTROL_PAUSE:
            qDebug() << "transfer was paused, status:" << ft->status;
            if(ft->status == -1) {
                ft->status = -3;
            } else if(ft->status == 1) {
                ft->status = -2;
            } else {
                qDebug() << "unexpected status ^";
            }
            break;
        case TOX_FILE_CONTROL_CANCEL:
            qDebug() << "transfer was cancelled, status:" << ft->status;
            ft->status = 0;
            /* if it's an incoming file, delete the file */
            if(m != NULL && !m->author) {
                if(!QFile::remove(m->text))
                    qDebug() << "Failed to remove file" << m->text;
            }
            break;
        default:
            Q_ASSERT(false);
    }
    emit cyanide.signal_file_status(fid, mid, ft->status);
}

void callback_file_chunk_request(Tox *tox, uint32_t fid, uint32_t file_number,
                                 uint64_t position, size_t length, void *UNUSED(user_data))
{
    qDebug() << "was called";

    bool success = false;
    TOX_ERR_FILE_SEND_CHUNK error;
    uint8_t *chunk = NULL;

    Friend *f = &cyanide.friends[fid];
    int mid = f->files[file_number];

    qDebug() << "mid" << mid << "fid" << fid
             << "file_number" << file_number;

    File_Transfer *ft;

    if(mid == -1) {
        ft = &cyanide.self.avatar_transfer;
    } else if(mid == -2) {
        Q_ASSERT(false);
    } else {
        ft = f->messages[mid].ft;
    }

    FILE *file = ft->file;

    if(length == 0) {
        fclose(file);
        qDebug() << "file sending done";
        ft->status = 2;
        emit cyanide.signal_file_status(fid, mid, ft->status);
        success = true;
    } else {
        chunk = (uint8_t*)malloc(length);
        if(fread(chunk, 1, length, file) != length) {
            qDebug() << "fread failed";
            return;
        }
        success = tox_file_send_chunk(tox, fid, file_number, position,
                (const uint8_t*)chunk, length, &error);
    }

    if(success) {
        ft->position += length;
        if(mid >= 0)
            emit cyanide.signal_file_progress(fid, mid, cyanide.get_file_progress(fid, mid));
    } else {
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

QString Cyanide::resume_transfer(int fid, int mid)
{
    return send_file_control(fid, mid, TOX_FILE_CONTROL_RESUME);
}

QString Cyanide::pause_transfer(int fid, int mid)
{
    return send_file_control(fid, mid, TOX_FILE_CONTROL_PAUSE);
}

QString Cyanide::cancel_transfer(int fid, int mid)
{
    return send_file_control(fid, mid, TOX_FILE_CONTROL_CANCEL);
}

QString Cyanide::send_file_control(int fid, int mid, TOX_FILE_CONTROL action)
{
    bool success;
    TOX_ERR_FILE_CONTROL error;

    Message *m = NULL;
    File_Transfer *ft;

    if(mid == -1) {
        ft = &self.avatar_transfer;
    } else if(mid == -2) {
        ft = &friends[fid].avatar_transfer;
    } else {
        m = &friends[fid].messages[mid];
        ft = m->ft;
    }

    success = tox_file_control(tox, fid, ft->file_number, action, &error);

    if(success) {
        switch(action) {
        case TOX_FILE_CONTROL_RESUME:
            qDebug() << "resuming transfer, status:" << ft->status;
            if(ft->status == -1) {
                ft->status = 1;
            } else if(ft->status == -3) {
                ft->status = -2;
            } else {
                qDebug() << "unexpected status ^";
            }
            break;
        case TOX_FILE_CONTROL_PAUSE:
            qDebug() << "pausing transfer, status:" << ft->status;
            if(ft->status == 1) {
                ft->status = -1;
            } else if(ft->status == -2) {
                ft->status = -3;
            } else {
                qDebug() << "unexpected status ^";
            }
            break;
        case TOX_FILE_CONTROL_CANCEL:
            qDebug() << "cancelling transfer, status:" << ft->status;
            ft->status = 0;
            /* if it's an incoming file, delete the file */
            if(m != NULL && !m->author) {
                if(!QFile::remove(m->text))
                    qDebug() << "Failed to remove file" << m->text;
            }
            break;
        }
        qDebug() << "sent file control, new status:" << ft->status;
        if(mid >= 0)
            emit signal_file_status(fid, mid, ft->status);
        return "";
    } else {
        qDebug() << "File control failed";
        switch(error) {
            case TOX_ERR_FILE_CONTROL_OK:
                return "";
            case TOX_ERR_FILE_CONTROL_FRIEND_NOT_FOUND:
                return tr("Error: Friend not found");
            case TOX_ERR_FILE_CONTROL_FRIEND_NOT_CONNECTED:
                return tr("Error: Friend not connected");
            case TOX_ERR_FILE_CONTROL_NOT_FOUND:
                return tr("Error: File transfer not found");
            case TOX_ERR_FILE_CONTROL_NOT_PAUSED:
                return "Bug (please report): Not paused";
            case TOX_ERR_FILE_CONTROL_DENIED:
                return "Bug (please report): Transfer is paused by peer";
            case TOX_ERR_FILE_CONTROL_ALREADY_PAUSED:
                return tr("Error: Already paused");
            case TOX_ERR_FILE_CONTROL_SENDQ:
                return tr("Error: Packet queue is full");
            default:
                Q_ASSERT(false);
                return "Bug (please report): Unknown";
        }
    }
}

bool Cyanide::get_file_id(uint32_t fid, File_Transfer *ft)
{
    TOX_ERR_FILE_GET error;

    if(!tox_file_get_file_id(tox, fid, ft->file_number
                            , ft->file_id, &error)) {
        qDebug() << "Failed to get file id";
        switch(error) {
            case TOX_ERR_FILE_GET_OK:
                break;
            case TOX_ERR_FILE_GET_FRIEND_NOT_FOUND:
                qDebug() << "Error: File not found";
                break;
            case TOX_ERR_FILE_GET_NOT_FOUND:
                qDebug() << "Error: File transfer not found";
                break;
        }
        return false;
    }
    return true;
}

QString Cyanide::send_file(int fid, QString path)
{
    return send_file(TOX_FILE_KIND_DATA, fid, path, NULL);
}

QString Cyanide::send_avatar(int fid)
{
    QString avatar = TOX_AVATAR_DIR + get_friend_public_key(SELF_FRIEND_NUMBER) + QString(".png");

    return send_file(TOX_FILE_KIND_AVATAR, fid, avatar, self.avatar_hash);
}

QString Cyanide::send_file(TOX_FILE_KIND kind, int fid, QString path, uint8_t *file_id)
{
    int error;

    Message m;
    File_Transfer *ft;

    if(kind == TOX_FILE_KIND_AVATAR) {
        ft = &self.avatar_transfer;
    } else {
        ft = (File_Transfer*)malloc(sizeof(File_Transfer));
        if(ft == NULL) {
            qDebug() << "Failed to allocate memory for the file transfer";
            return "no memory";
        }
        m.type = MSGTYPE_FILE; //TODO inline images?
        m.timestamp = QDateTime::currentDateTime();
        m.author = true;
        m.text = path;
        m.ft = ft;
    }

    ft->position = 0;
    ft->status = -2;

    QString basename = path.right(path.size() - 1 - path.lastIndexOf("/"));
    ft->filename_length = qstrlen(basename);
    ft->filename = (uint8_t*)malloc(ft->filename_length + 1);
    qstr_to_utf8(ft->filename, basename);

    ft->file = fopen(path.toUtf8().constData(), "rb");
    if(ft->file == NULL) {
        if(kind == TOX_FILE_KIND_AVATAR) {
            ft->file_size = 0;
        } else {
            return tr("Error: Failed to open file: ") + path;
        }
    } else {
        fseek(ft->file, 0L, SEEK_END);
        ft->file_size = ftell(ft->file);
        rewind(ft->file);
    }

    ft->file_number = tox_file_send(tox, fid, kind, ft->file_size,
                                    file_id, ft->filename, ft->filename_length,
                                    (TOX_ERR_FILE_SEND*)&error);
    friends[fid].files[ft->file_number] = -1;
    qDebug() << "sending file, friend number" << fid
             << "file number" << ft->file_number;

    switch(error) {
        case TOX_ERR_FILE_SEND_OK:
            break;
        case TOX_ERR_FILE_SEND_FRIEND_NOT_FOUND:
            return tr("Error: Friend not found");
        case TOX_ERR_FILE_SEND_FRIEND_NOT_CONNECTED:
            return tr("Error: Friend not connected");
        case TOX_ERR_FILE_SEND_NAME_TOO_LONG:
            return tr("Error: Filename too long");
        case TOX_ERR_FILE_SEND_TOO_MANY:
            return tr("Error: Too many ongoing transfers");
        default:
            Q_ASSERT(false);
    }

    if(kind != TOX_FILE_KIND_AVATAR)
        add_message(fid, m);

    return "";
}

void callback_av_invite(ToxAv *av, int32_t call_index, void *UNUSED(userdata))
{
    qDebug() << "was called";

    int fid = toxav_get_peer_id(av, call_index, 0);
    Friend *f = &cyanide.friends[fid];

    ToxAvCSettings peer_settings ;
    toxav_get_peer_csettings(av, call_index, 0, &peer_settings);
    bool video = peer_settings.call_type == av_TypeVideo;

    f->call_index = call_index;
    f->callstate = -2;
    emit cyanide.signal_friend_callstate(fid, f->callstate);
    emit cyanide.signal_av_invite(fid);
}

void callback_av_start(ToxAv *av, int32_t call_index, void *UNUSED(userdata))
{
    qDebug() << "was called";
    ToxAvCSettings peer_settings;
    int fid = toxav_get_peer_id(av, call_index, 0);
    toxav_get_peer_csettings(av, call_index, 0, &peer_settings);
    bool video = peer_settings.call_type == av_TypeVideo;
    if(toxav_prepare_transmission(av, call_index, 1) == 0) {
        notify_error("asdf call started", "");
    } else {
        qDebug() << "toxav_prepare_transmission() failed";
        return;
    }
}

void callback_av_cancel(ToxAv *av, int32_t call_index, void *UNUSED(userdata))
{
    qDebug() << "was called";
}

void callback_av_reject(ToxAv *av, int32_t call_index, void *UNUSED(userdata))
{
    qDebug() << "was called";
}

void callback_av_end(ToxAv *av, int32_t call_index, void *UNUSED(userdata))
{
    qDebug() << "was called";
}

void callback_av_ringing(ToxAv *av, int32_t call_index, void *UNUSED(userdata))
{
    qDebug() << "was called";
}

void callback_av_requesttimeout(ToxAv *av, int32_t call_index, void *UNUSED(userdata))
{
    qDebug() << "was called";
}

void callback_av_peertimeout(ToxAv *av, int32_t call_index, void *UNUSED(userdata))
{
    qDebug() << "was called";
}

void callback_av_selfmediachange(ToxAv *av, int32_t call_index, void *UNUSED(userdata))
{
    qDebug() << "was called";
}

void callback_av_peermediachange(ToxAv *av, int32_t call_index, void *UNUSED(userdata))
{
    qDebug() << "was called";
}

void callback_av_audio(ToxAv *av, int32_t call_index, const int16_t *data, uint16_t samples, void *UNUSED(userdata))
{
    qDebug() << "was called";
    ToxAvCSettings dest;
    if(toxav_get_peer_csettings(av, call_index, 0, &dest) == 0) {
        audio_play(call_index, data, length, dest.audio_channels);
    }
}

static void audio_play(int i, const int16_t *data, int samples, uint8_t channels, unsigned int sample_rate)
{
    if(!channels || channels > 2) {
        return;
    }
    // TODO
}

void callback_av_video(ToxAv *av, int32_t call_index, const vpx_image_t *img, void *UNUSED(userdata))
{
    qDebug() << "was called";
}

void Cyanide::av_invite_accept(int fid)
{
    Friend *f = &friends[fid];
    ToxAvCSettings csettings = av_DefaultSettings;
    toxav_answer(toxav, f->call_index, &csettings);
    emit signal_friend_callstate(fid, (f->callstate = 2));
}

void Cyanide::av_invite_reject(int fid)
{
    Friend *f = &friends[fid];
    toxav_reject(toxav, f->call_index, ""); //TODO add reason
    emit signal_friend_callstate(fid, (f->callstate = 0));
}

void Cyanide::av_call(int fid)
{
    Friend *f = &friends[fid];
    if(f->callstate != 0)
        notify_error("already in a call", "");

    ToxAvCSettings csettings = av_DefaultSettings;

    toxav_call(toxav, &f->call_index, fid, &csettings, 15);
    emit signal_friend_callstate(fid, (f->callstate = -1));
}

void Cyanide::av_call_cancel(int fid)
{
    Friend *f = &friends[fid];
    Q_ASSERT(f->callstate == -1);
    toxav_cancel(toxav, f->call_index, fid, "Call cancelled by friend");
    emit signal_friend_callstate(fid, (f->callstate = 0));
}

void Cyanide::av_hangup(int fid)
{
    Friend *f = &friends[fid];
    toxav_hangup(toxav, f->call_index);
    emit signal_friend_callstate(fid, (f->callstate = 0));
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
        if(!dns_request(address, id_str))
            return tr("Error: Invalid Tox ID");
    }

    QString errmsg = send_friend_request_id(address, msg, msg_len);
    if(errmsg == "") {
        Friend *f = new Friend((const uint8_t*)address, id_str, "");
        add_friend(f);
        char hex_address[2 * TOX_ADDRESS_SIZE + 1];
        address_to_string(hex_address, (char*)address);
        hex_address[2 * TOX_ADDRESS_SIZE] = '\0';
        settings.add_friend(hex_address);
    }

    return errmsg;
}

/* */
QString Cyanide::send_friend_request_id(const uint8_t *id, const uint8_t *msg, size_t msg_length)
{
    TOX_ERR_FRIEND_ADD error;
    relocate_blocked_friend();
    uint32_t fid = tox_friend_add(tox, id, msg, msg_length, &error);
    switch(error) {
        case TOX_ERR_FRIEND_ADD_OK:
            return "";
        case TOX_ERR_FRIEND_ADD_NULL:
            return "Error: Null";
        case TOX_ERR_FRIEND_ADD_TOO_LONG:
            return tr("Error: Message is too long");
        case TOX_ERR_FRIEND_ADD_NO_MESSAGE:
            return "Bug (please report): Empty message";
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

std::vector<QString> split_message(QString rest)
{
    std::vector<QString> messages;
    QString chunk;
    int last_space;

    while(rest != "") {
        chunk = rest.left(TOX_MAX_MESSAGE_LENGTH);
        last_space = chunk.lastIndexOf(" ");

        if(chunk.size() == TOX_MAX_MESSAGE_LENGTH) {
            if(last_space > 0) {
                chunk = rest.left(last_space);
            }
        }
        rest = rest.right(rest.size() - chunk.size());

        messages.push_back(chunk);
    }
    return messages;
}

QString Cyanide::send_friend_message(int fid, QString message)
{
    TOX_ERR_FRIEND_SEND_MESSAGE error;
    QString errmsg = "";

    if(friends[fid].blocked)
        return "Friend is blocked, unblock to connect";

    TOX_MESSAGE_TYPE type = TOX_MESSAGE_TYPE_NORMAL;

    std::vector<QString> messages = split_message(message);

    for(size_t i = 0; i < messages.size(); i++) {
        QString msg_str = messages[i];

        size_t msg_len = qstrlen(msg_str);
        uint8_t msg[msg_len];
        qstr_to_utf8(msg, msg_str);

        uint32_t message_id = tox_friend_send_message(tox, fid, type, msg, msg_len, &error);
        qDebug() << "message id:" << message_id;

        switch(error) {
            case TOX_ERR_FRIEND_SEND_MESSAGE_OK:
                break;
            case TOX_ERR_FRIEND_SEND_MESSAGE_NULL:
                Q_ASSERT(false);
                break;
            case TOX_ERR_FRIEND_SEND_MESSAGE_FRIEND_NOT_FOUND:
                Q_ASSERT(false);
                break;
            case TOX_ERR_FRIEND_SEND_MESSAGE_FRIEND_NOT_CONNECTED:
                errmsg = tr("Error: Friend not connected");
                break;
            case TOX_ERR_FRIEND_SEND_MESSAGE_SENDQ:
                Q_ASSERT(false);
                break;
            case TOX_ERR_FRIEND_SEND_MESSAGE_TOO_LONG:
                qDebug() << "message too long";
                break;
            case TOX_ERR_FRIEND_SEND_MESSAGE_EMPTY:
                Q_ASSERT(false);
                break;
        }

        if(errmsg != "")
            return errmsg;

        Message m;
        m.type = MSGTYPE_NORMAL; //TODO implement /me
        m.author = true;
        m.text = msg_str;
        m.timestamp = QDateTime::currentDateTime();
        m.ft = NULL;

        add_message(fid, m);
    }

    return errmsg;
}

bool Cyanide::accept_friend_request(int fid)
{
    TOX_ERR_FRIEND_ADD error;
    relocate_blocked_friend();
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
    if(friends[fid].accepted && !tox_friend_delete(tox, fid, &error)) {
        qDebug() << "Failed to remove friend";
        return;
    }
    save_needed = true;
    settings.remove_friend(get_friend_public_key(fid));
    friends.erase(fid);
}

/* setters and getters */

QString Cyanide::get_profile_name()
{
    return profile_name;
}

QString Cyanide::set_profile_name(QString name)
{
    QString old_name = tox_save_file();
    QString new_name = tox_save_file(name);

    QFile old_file(old_name);
    QFile new_file(new_name);

    if(new_file.exists()) {
        return tr("Error: Tox save file already exists");
    } else {
        save_needed = false;
        /* possible race condition? */
        old_file.rename(new_name);
        QFile::rename(CYANIDE_DATA_DIR + profile_name.replace('/','_') + ".sqlite",
                    CYANIDE_DATA_DIR + name.replace('/','_') + ".sqlite");
        profile_name = name;
        save_needed = true;
        return "";
    }
}

QList<int> Cyanide::get_friend_numbers()
{
    QList<int> friend_numbers;
    for(auto it = friends.begin(); it != friends.end(); it++) {
        friend_numbers.append(it->first);
    }
    return friend_numbers;
}

QList<int> Cyanide::get_message_numbers(int fid)
{
    QList<int> message_numbers;
    auto messages = friends[fid].messages;
    int i = 0;
    for(auto it = messages.begin(); it != messages.end(); it++) {
        message_numbers.append(i++);
    }
    return message_numbers;
}

void Cyanide::set_friend_activity(int fid, bool status)
{
    Q_ASSERT(fid != SELF_FRIEND_NUMBER);
    friends[fid].activity = status;
    emit signal_friend_activity(fid);
}

bool Cyanide::get_friend_blocked(int fid)
{
    Friend f = fid == SELF_FRIEND_NUMBER ? self : friends[fid];
    return f.blocked;
}

void Cyanide::set_friend_blocked(int fid, bool block)
{
    Q_ASSERT(fid != SELF_FRIEND_NUMBER);
    int error;

    Friend *f = &friends[fid];

    if(block) {
        if(tox_friend_delete(tox, fid, (TOX_ERR_FRIEND_DELETE*)&error)) {
            f->connection_status = TOX_CONNECTION_NONE;
        } else {
            qDebug() << "Failed to remove friend";
        }
    } else {
        int friend_number = tox_friend_add_norequest(tox, f->public_key, (TOX_ERR_FRIEND_ADD*)&error);
        Q_ASSERT(friend_number == fid);
    }
    friends[fid].blocked = block;
    emit signal_friend_blocked(fid, block);
    emit signal_friend_connection_status(fid, f->connection_status != TOX_CONNECTION_NONE);
}

int Cyanide::get_friend_callstate(int fid)
{
    Friend f = fid == SELF_FRIEND_NUMBER ? self : friends[fid];
    return f.callstate;
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
    emit signal_friend_status(SELF_FRIEND_NUMBER);
}

QString Cyanide::set_self_avatar(QString new_avatar)
{
    bool success;
    QString public_key = get_friend_public_key(SELF_FRIEND_NUMBER);
    QString old_avatar = TOX_AVATAR_DIR + public_key + QString(".png");

    if(new_avatar == "") {
        /* remove the avatar */
        success = QFile::remove(old_avatar);
        if(!success) {
            qDebug() << "Failed to remove avatar file";
        } else {
            memset(self.avatar_hash, 0, TOX_HASH_LENGTH);
            emit signal_avatar_change(SELF_FRIEND_NUMBER);
            send_new_avatar();
        }
        return "";
    }

    uint32_t size;
    uint8_t *data = (uint8_t*)file_raw(new_avatar.toUtf8().data(), &size);
    if(data == NULL) {
        return tr("File not found: ") + new_avatar;
    }

    if(size > MAX_AVATAR_SIZE) {
        free(data);
        return tr("Avatar too large. Maximum size: 64KiB");
    }
    uint8_t previous_hash[TOX_HASH_LENGTH];
    memcpy(previous_hash, self.avatar_hash, TOX_HASH_LENGTH);
    success = tox_hash(self.avatar_hash, data, size);
    Q_ASSERT(success);
    FILE* out = fopen(old_avatar.toUtf8().data(), "wb");
    fwrite(data, size, 1, out);
    fclose(out);
    free(data);
    if(0 != memcmp(previous_hash, self.avatar_hash, TOX_HASH_LENGTH)) {
        QByteArray hash((const char*)self.avatar_hash, TOX_HASH_LENGTH);
        settings.set_friend_avatar_hash(public_key, hash);
    } else {
        /* friend avatar data is the same as before, so we don't
         * need to save it.
         * send it anyway, other clients will probably reject it
         */
    }
    emit signal_avatar_change(SELF_FRIEND_NUMBER);
    send_new_avatar();

    return "";
}

void Cyanide::send_new_avatar()
{
    /* send the avatar to each connected friend */
    for(auto i=friends.begin(); i!=friends.end(); i++) {
        Friend *f = &i->second;
        if(f->connection_status != TOX_CONNECTION_NONE) {
            QString errmsg = cyanide.send_avatar(i->first);
            if(errmsg != "")
                qDebug() << "Failed to send avatar. " << errmsg;
        }
    }
}

QString Cyanide::get_friend_name(int fid)
{
    Friend f = fid == SELF_FRIEND_NUMBER ? self : friends[fid];
    return f.name;
}

QString Cyanide::get_friend_avatar(int fid)
{
    QString avatar = TOX_AVATAR_DIR + get_friend_public_key(fid) + QString(".png");
    return QFile::exists(avatar) ? avatar : "qrc:/images/blankavatar";
}

QString Cyanide::get_friend_status_message(int fid)
{
    Friend f = fid == SELF_FRIEND_NUMBER ? self : friends[fid];
    return f.status_message;
}

QString Cyanide::get_friend_status_icon(int fid)
{
    Friend f = fid == SELF_FRIEND_NUMBER ? self : friends[fid];
    return get_friend_status_icon(fid, f.connection_status != TOX_CONNECTION_NONE);
}

QString Cyanide::get_friend_status_icon(int fid, bool online)
{
    QString url = "qrc:/images/";
    Friend f = fid == SELF_FRIEND_NUMBER ? self : friends[fid];

    if(!online) {
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
    
    if(f.activity)
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

int Cyanide::get_message_type(int fid, int mid)
{
    Q_ASSERT(fid != SELF_FRIEND_NUMBER);
    return friends[fid].messages[mid].type;
}

QString Cyanide::get_message_text(int fid, int mid)
{
    Q_ASSERT(fid != SELF_FRIEND_NUMBER);
    return friends[fid].messages[mid].text;
}

QString Cyanide::get_message_html_escaped_text(int fid, int mid)
{
    return get_message_text(fid, mid).toHtmlEscaped();
}

QString Cyanide::get_message_rich_text(int fid, int mid)
{
    QString text = get_message_html_escaped_text(fid, mid);

    const QString email_chars = QRegExp::escape("A-Za-z0-9!#$%&'*+-/=?^_`{|}~.");
    const QString url_chars =   QRegExp::escape("A-Za-z0-9!#$%&'*+-/=?^_`{|}~");
    const QString email_token = "[" + email_chars + "]+";
    const QString url_token = "[" + url_chars + "]+";

    /* match either protocol:address or email@domain or example.org */
    QRegExp rx("(\\b[A-Za-z][A-Za-z0-9]*:[^\\s]+\\b"
               "|\\b" + email_token + "@" + email_token + "\\b"
               "|\\b" + url_token  + "\\." + email_token + "\\b)");
    QString repl;
    int protocol;
    int pos = 0;

     while((pos = rx.indexIn(text, pos)) != -1) {
         /* check whether the captured text has a protocol identifier */
         protocol = rx.cap(1).indexOf(QRegExp("^[A-Za-z0-9]+:"), 0);
         repl = "<a href=\"" +
                 QString(  (protocol != -1) ? ""
                         : (rx.cap(1).indexOf("@") != -1) ? "mailto:"
                         : "https:")
                 + rx.cap(1) + "\">" + rx.cap(1) + "</a>";
         text.replace(pos, rx.matchedLength(), repl);
         pos += repl.length();
     }

     QRegExp ry("(^"+QRegExp::escape("&gt;")+"[^\n]*\n"
                "|^"+QRegExp::escape("&gt;")+"[^\n]*$)"
                );
     pos = 0;
     while((pos = ry.indexIn(text, pos)) != -1) {
         repl = "<font color='lightgreen'>"+ry.cap(1)+"</font>";
         text.replace(pos, ry.matchedLength(), repl);
         pos += repl.length();
     }
     text.replace("\n", "<br>");

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

int Cyanide::get_file_status(int fid, int mid)
{
    Q_ASSERT(fid != SELF_FRIEND_NUMBER);
    return friends[fid].messages[mid].ft->status;
}

QString Cyanide::get_file_link(int fid, int mid)
{
    Q_ASSERT(fid != SELF_FRIEND_NUMBER);
    QString filename = friends[fid].messages[mid].text;
    QString fullpath = "file://" + filename;
    return "<a href=\"" + fullpath.toHtmlEscaped() + "\">"
            + filename.right(filename.size() - 1 - filename.lastIndexOf("/")).toHtmlEscaped()
            + "</a>";
}

int Cyanide::get_file_progress(int fid, int mid)
{
    Q_ASSERT(fid != SELF_FRIEND_NUMBER);
    File_Transfer *ft = friends[fid].messages[mid].ft;

    return ft->file_size == 0 ? 100
                              : 100 * ft->position / ft->file_size;
}
