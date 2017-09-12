#include <stdlib.h>
#include <sys/eventfd.h>
#include <sys/stat.h>
#include <thread>
#include <time.h>
#include <unistd.h>

#include <QObject>
#include <QTranslator>
#include <QtDBus>
#include <QtQuick>

#ifdef MLITE
#include <mlite5/mnotification.h>
#include <mlite5/mremoteaction.h>
#else
#include <nemonotifications-qt5/notification.h>
#endif
#include "cyanide.h"
#include "dbusinterface.h"
#include "dns.cpp"
#include "history.h"
#include "settings.h"
#include "tox_bootstrap.h"
#include "tox_callbacks.h"
#include "util.h"
#include <sailfishapp.h>

#define TOX_ENC_SAVE_MAGIC_LENGTH 8

struct Tox_Options tox_options;

int main(int argc, char *argv[]) {
  QGuiApplication *app = SailfishApp::application(argc, argv);
  app->setOrganizationName("Tox");
  app->setOrganizationDomain("Tox");
  app->setApplicationName("Cyanide");

  Cyanide cyanide{};
  DBusInterface relay{};
  relay.cyanide = &cyanide;

  if (!(QDBusConnection::sessionBus().registerObject(
            "/", &relay, QDBusConnection::ExportScriptableContents) &&
        QDBusConnection::sessionBus().registerService("harbour.cyanide")))
    QGuiApplication::exit(0);

  cyanide.wifi_monitor();

  cyanide.read_default_profile(app->arguments());

  std::thread my_tox_thread{[&cyanide]() { cyanide.tox_thread(); }};

  qmlRegisterType<Message_Type>("harbour.cyanide", 1, 0, "Message_Type");
  qmlRegisterType<File_State>("harbour.cyanide", 1, 0, "File_State");
  qmlRegisterType<Call_Control>("harbour.cyanide", 1, 0, "Call_Control");
  qmlRegisterType<Call_State>("harbour.cyanide", 1, 0, "Call_State");
  cyanide.view->rootContext()->setContextProperty("cyanide", &cyanide);
  cyanide.view->rootContext()->setContextProperty("settings",
                                                  &cyanide.settings);
  cyanide.view->setSource(SailfishApp::pathTo("qml/cyanide.qml"));
  cyanide.view->showFullScreen();

  QObject::connect(cyanide.view, SIGNAL(visibilityChanged(QWindow::Visibility)),
                   &cyanide, SLOT(visibility_changed(QWindow::Visibility)));

  int result = app->exec();

  if (cyanide.in_call)
    cyanide.hang_up();

  uint64_t event = LOOP_FINISH;
  auto res = write(cyanide.events, &event, sizeof(event));
  (void)res;
  cyanide.loop = LOOP_FINISH;
  my_tox_thread.join();

  return result;
}

Cyanide::Cyanide(QObject *parent) : QObject(parent) {
  view = SailfishApp::createView();

  events = eventfd(0, 0);
  have_password = false;
  tox_save_data = nullptr;
  in_call = false;
  call_state = 0;
}

const QString Cyanide::tox_save_file() const {
  return tox_save_file(profile_name);
}

const QString Cyanide::tox_save_file(QString const &name) const {
  QString save_file{name};
  return save_file.replace('/', ' ').prepend(TOX_DATA_DIR).append(".tox");
}

/* sets profile_name */
void Cyanide::read_default_profile(QStringList const &args) {
  for (auto const &arg : args) {
    if (arg.startsWith("tox:")) {
      // TODO
    }
  }

  QFile file{DEFAULT_PROFILE_FILE};
  if (file.exists()) {
    file.open(QIODevice::ReadOnly | QIODevice::Text);
    profile_name = file.readLine();
    file.close();
    profile_name.chop(1);
    if (QFile::exists(tox_save_file()))
      return;
  }

  profile_name = DEFAULT_PROFILE_NAME;
  write_default_profile();
}

void Cyanide::write_default_profile() {
  QFile file{DEFAULT_PROFILE_FILE};
  file.open(QIODevice::WriteOnly | QIODevice::Text);
  file.write(profile_name.toUtf8() + '\n');
  file.close();
}

void Cyanide::reload() { loop = LOOP_RELOAD; }

void Cyanide::load_new_profile() {
  QString path{};
  int i = 0;
  do {
    i++;
    path = TOX_DATA_DIR + "id" + QString::number(i) + ".tox";
  } while (QFile::exists(path));
  QFile file{path};
  file.open(QIODevice::WriteOnly);
  file.close();
  load_tox_save_file(path, nullptr);
}

void Cyanide::delete_current_profile() {
  bool success;
  loop = LOOP_NOT_LOGGED_IN;
  usleep(1500 * MAX_ITERATION_TIME);
  success = QFile::remove(tox_save_file());
  qDebug() << "removed" << tox_save_file() << ":" << success;
  success = QFile::remove(CYANIDE_DATA_DIR + profile_name.replace('/', '_') +
                          ".sqlite");
  qDebug() << "removed db:" << success;
}

bool Cyanide::load_tox_save_file(QString const path,
                                 QString const passphrase) {
  int error;
  bool success;

  QString basename{path.mid(path.lastIndexOf('/') + 1)};
  if (!basename.endsWith(".tox"))
    basename += ".tox";

  /* ensure that the save file is in ~/.config/tox */
  QFile::copy(path, TOX_DATA_DIR + basename);

  /* remove the .tox extension */
  basename.chop(4);
  next_profile_name = basename;
  next_have_password = passphrase != nullptr;

  if (next_have_password) {
    size_t size;
    uint8_t *data = (uint8_t *)file_raw(
        tox_save_file(next_profile_name).toUtf8().data(), &size);
    uint8_t salt[TOX_PASS_SALT_LENGTH];
    memcpy(salt, (const void *)(data + TOX_ENC_SAVE_MAGIC_LENGTH),
           TOX_PASS_SALT_LENGTH);
    tox_pass_key = tox_pass_key_new();
    tox_pass_key_derive_with_salt(tox_pass_key, (const uint8_t *)passphrase.toUtf8().constData(),
                             passphrase.toUtf8().size(), salt,
                             (TOX_ERR_KEY_DERIVATION *)&error);
    qDebug() << "key derivation error:" << error;
    tox_save_data = (uint8_t *)malloc(size - TOX_PASS_ENCRYPTION_EXTRA_LENGTH);
    success = tox_pass_key_decrypt(tox_pass_key, data, size, tox_save_data,
                                   (TOX_ERR_DECRYPTION *)&error);
    qDebug() << "decryption error:" << error;
    tox_save_data_size = size - TOX_PASS_ENCRYPTION_EXTRA_LENGTH;
    if (!success) {
      free(tox_save_data);
      tox_save_data = nullptr;
      return false;
    }
  }

  if (loop == LOOP_STOP || loop == LOOP_NOT_LOGGED_IN) {
    // tox_loop not running, resume it
    profile_name = next_profile_name;
    have_password = next_have_password;
    resume_thread();
  } else {
    loop = LOOP_RELOAD_OTHER;
  }
  return true;
}

bool Cyanide::file_is_encrypted(QString const path) const {
  uint8_t data[TOX_ENC_SAVE_MAGIC_LENGTH] = {0};
  QFile file{path};
  file.open(QIODevice::ReadOnly);
  file.read((char *)data, TOX_ENC_SAVE_MAGIC_LENGTH);
  file.close();

  return tox_is_data_encrypted(data);
}

void Cyanide::check_wifi() {
  if (!is_logged_in())
    return;

  if (settings.get("wifi-only") == "true") {
    if (0 ==
        system(
            "test true = \"$(dbus-send --system --dest=net.connman "
            "--print-reply"
            " /net/connman/technology/wifi net.connman.Technology.GetProperties"
            "| grep -A1 Connected | sed -e 1d -e 's/^.*\\s//')\"")) {
      qDebug() << "connected via Wi-Fi, toxing";
      if (loop == LOOP_SUSPEND)
        resume_thread();
      else
        loop = LOOP_RUN;
    } else {
      qDebug() << "not connected via Wi-Fi, not toxing";
      suspend_thread();
    }
  } else {
    if (loop == LOOP_SUSPEND)
      resume_thread();
    else
      loop = LOOP_RUN;
  }
}

void Cyanide::wifi_monitor() {
  QDBusConnection dbus = QDBusConnection::systemBus();

  if (!dbus.isConnected()) {
    qDebug() << "Failed to connect to the D-Bus session bus.";
  }
  dbus.connect("net.connman", "/net/connman/technology/wifi",
               "net.connman.Technology", "PropertyChanged", this,
               SLOT(wifi_changed(QString, QDBusVariant)));
}

void Cyanide::wifi_changed(QString &name, QDBusVariant &dbus_variant) {
  if (!is_logged_in())
    return;

  bool value = dbus_variant.variant().toBool();
  qDebug() << "Received DBus signal";
  qDebug() << "Property" << name << "Value" << value;

  if (name == "Powered") {
    ;
  } else if (name == "Connected" && settings.get("wifi-only") == "true") {
    if (value && loop == LOOP_SUSPEND) {
      qDebug() << "connected, resuming thread";
      resume_thread();
    } else if (!value && loop == LOOP_RUN) {
      qDebug() << "not connected, suspending thread";
      suspend_thread();
    }
  }
}

void Cyanide::resume_thread() {
  loop = LOOP_RUN;
  uint64_t event = LOOP_RUN;
  ssize_t tmp = write(events, &event, sizeof(event));
  Q_ASSERT(tmp == sizeof(event));
  (void)tmp;
}

void Cyanide::suspend_thread() {
  loop = LOOP_SUSPEND;
  self.connection_status = TOX_CONNECTION_NONE;
  emit signal_friend_connection_status(SELF_FRIEND_NUMBER, false);
  for (auto &entry : friends) {
    entry.second.connection_status = TOX_CONNECTION_NONE;
    emit signal_friend_connection_status(entry.first, false);
  }
  usleep(1500 * MAX_ITERATION_TIME);
}

void Cyanide::visibility_changed(QWindow::Visibility visibility) {
  /* remove all notifications for now until I find a proper solution
   * (because the error messages are shown too)
   */
  for (auto &entry : friends) {
    Friend &f = entry.second;
    if (f.notification != nullptr) {
#ifdef MLITE
      f.notification->remove();
#else
      f.notification->close();
#endif
      f.notification = nullptr;
    }
  }
#ifdef MLITE
  for (MNotification *n : MNotification::notifications()) {
    qDebug() << "removing stray notification" << n->body();
    n->remove();
  }
#endif
}

void Cyanide::on_message_notification_activated(int fid) {
  qDebug() << "notification activated";
  emit signal_focus_friend(fid);
  raise();
}

void Cyanide::notify_error(QString const summary, QString const body) {
#ifdef MLITE
  MNotification *n = new MNotification("", summary, body);
#else
  Notification *n = new Notification();
  n->setCategory("harbour.cyanide.message");
  n->setSummary(summary);
  n->setBody(body);
#endif
  n->publish();
}

void Cyanide::notify_message(int fid, QString const summary,
                             QString const body) {
  Q_ASSERT(fid != SELF_FRIEND_NUMBER);
  Friend *f = &friends.at(fid);

  if (f->notification != nullptr) {
    f->notification->setSummary(summary);
    f->notification->setBody(body);
#if MLITE
    f->notification->setCount(1);
#endif
    f->notification->publish();
  } else {
#ifdef MLITE
    new MNotification("harbour.cyanide.message", summary, body);
    MRemoteAction action{"harbour.cyanide", "/", "harbour.cyanide",
                         "message_notification_activated",
                         QVariantList() << fid};
    // TODO
    f->notification->setAction(action);
    f->notification->setCount(1);
    f->notification->publish();
#else
    f->notification = new Notification();
    f->notification->setCategory("harbour.cyanide.message");
    f->notification->setSummary(summary);
    f->notification->setBody(body);
    f->notification->publish();
#endif
  }
}

void Cyanide::notify_call(int fid, QString const summary,
                          QString const body) {
  Q_ASSERT(fid != SELF_FRIEND_NUMBER);
  Friend *f = &friends[fid];

  if (f->notification != nullptr) {
#ifdef MLITE
    f->notification->remove();
#else
    f->notification->close();
#endif
  }
#ifdef MLITE
  f->notification = new MNotification(
      // "harbour.cyanide.call",
      "x-nemo.email.conf", summary, body);
  MRemoteAction action{"harbour.cyanide", "/", "harbour.cyanide",
                       "message_notification_activated", QVariantList() << fid};
  f->notification->setAction(action);
  f->notification->setCount(1);
  f->notification->publish();
#else
  f->notification = new Notification();
  f->notification->setCategory("harbour.cyanide.call");
  f->notification->setSummary(summary);
  f->notification->setBody(body);
  f->notification->publish();
#endif
}

bool Cyanide::is_logged_in() const { return loop != LOOP_NOT_LOGGED_IN; }

void Cyanide::raise() {
  if (!is_visible()) {
    view->raise();
    view->requestActivate();
  }
}

bool Cyanide::is_visible() const {
  return view->visibility() == QWindow::FullScreen;
}

void Cyanide::free_friend_messages(Friend &f) {
  f.files.clear();
  for (auto &m : f.messages) {
    if (m.ft != nullptr) {
      free(m.ft->filename);
      free(m.ft);
    }
  }
  f.messages.clear();
}

void Cyanide::load_tox_and_stuff_pretty_please() {
  int error;

  settings.open_database(profile_name);
  history.open_database(profile_name);

  self.user_status = TOX_USER_STATUS_NONE;
  self.connection_status = TOX_CONNECTION_NONE;
  memset(&self.avatar_transfer, 0, sizeof(File_Transfer));

  /* for(std::pair<uint32_t, Friend>pair : friends) { */
  for (auto &entry : friends) {
    Friend &f = entry.second;
    free(f.avatar_transfer.filename);
    free_friend_messages(f);
  }
  friends.clear();

  save_needed = false;

  tox_options_default(&tox_options);

  if (settings.get("udp-enabled") != "true")
    tox_options.udp_enabled = 0;

  if (tox_save_data == nullptr)
    tox_save_data = (uint8_t *)file_raw(tox_save_file().toUtf8().data(),
                                        &tox_save_data_size);
  if (tox_save_data == nullptr)
    tox_save_data_size = 0;

  if (tox_save_data_size >= TOX_ENC_SAVE_MAGIC_LENGTH &&
      tox_is_data_encrypted(tox_save_data)) {
    qDebug() << "save is encrypted";
    loop = LOOP_NOT_LOGGED_IN;
    tox_options.savedata_type = TOX_SAVEDATA_TYPE_NONE;
  } else {
    loop = LOOP_RUN;
    tox_options.savedata_data = tox_save_data;
    tox_options.savedata_length = tox_save_data_size;
    tox_options.savedata_type = tox_save_data_size ? TOX_SAVEDATA_TYPE_TOX_SAVE
                                                   : TOX_SAVEDATA_TYPE_NONE;
  }

  tox = tox_new(&tox_options, (TOX_ERR_NEW *)&error);
  switch (error) {
  case TOX_ERR_NEW_OK:
    break;
  case TOX_ERR_NEW_NULL:
    Q_ASSERT(false);
    break;
  case TOX_ERR_NEW_MALLOC:
    Q_ASSERT(false);
    break;
  case TOX_ERR_NEW_PORT_ALLOC:
    Q_ASSERT(false);
    break;
  case TOX_ERR_NEW_PROXY_BAD_TYPE:
    Q_ASSERT(false);
    break;
  case TOX_ERR_NEW_PROXY_BAD_HOST:
    Q_ASSERT(false);
    break;
  case TOX_ERR_NEW_PROXY_BAD_PORT:
    Q_ASSERT(false);
    break;
  case TOX_ERR_NEW_PROXY_NOT_FOUND:
    Q_ASSERT(false);
    break;
  case TOX_ERR_NEW_LOAD_ENCRYPTED:
    qDebug() << "encrypted";
    Q_ASSERT(false);
    break;
  case TOX_ERR_NEW_LOAD_BAD_FORMAT:
    qDebug() << "bad format";
    Q_ASSERT(false);
    break;
  }

  free(tox_save_data);
  tox_save_data = nullptr;

  tox_self_get_address(tox, self_address);
  tox_self_get_public_key(tox, self.public_key);

  QString public_key{get_friend_public_key(SELF_FRIEND_NUMBER)};
  settings.add_friend_if_not_exists(public_key);

  QByteArray saved_hash{settings.get_friend_avatar_hash(public_key)};
  memcpy(self.avatar_hash, saved_hash.constData(), TOX_HASH_LENGTH);

  /*TODO refactor */
  if (tox_save_data_size)
    load_tox_data();
  else
    load_defaults();

  emit signal_friend_added(SELF_FRIEND_NUMBER);
  emit signal_avatar_change(SELF_FRIEND_NUMBER);

  qDebug() << "Name:" << self.name;
  qDebug() << "Status" << self.status_message;
  qDebug() << "Tox ID" << get_self_address();
}

void Cyanide::tox_thread() {
  qDebug() << "profile name" << profile_name;
  qDebug() << "tox_save_file" << tox_save_file();

  load_tox_and_stuff_pretty_please();

  set_callbacks();

  // Connect to bootstraped nodes in "tox_bootstrap.h
  do_bootstrap();

  check_wifi();

  // Start the tox av session.
  TOXAV_ERR_NEW error;
  toxav = toxav_new(tox, &error);
  switch (error) {
  case TOXAV_ERR_NEW_OK:
    break;
  case TOXAV_ERR_NEW_NULL:
    Q_ASSERT(false);
    break;
  case TOXAV_ERR_NEW_MALLOC:
    qWarning() << "Failed to allocate memory for toxav";
    break;
  case TOXAV_ERR_NEW_MULTIPLE:
    qWarning() << "Attempted to create second toxav session";
    break;
  }

  // Give toxcore the av functions to call
  set_av_callbacks();

  tox_loop();
}

void Cyanide::tox_loop() {
  std::thread toxav_thread([this]() { this->toxav_thread(); });

  uint64_t last_save = get_time(), time;
  TOX_CONNECTION c, connection = c = TOX_CONNECTION_NONE;

  while (loop == LOOP_RUN) {
    // Put toxcore to work
    tox_iterate(tox, this);

    // Check current connection
    if ((c = tox_self_get_connection_status(tox)) != connection) {
      self.connection_status = connection = c;
      emit signal_friend_connection_status(SELF_FRIEND_NUMBER,
                                           c != TOX_CONNECTION_NONE);
      qDebug() << (c != TOX_CONNECTION_NONE ? "Connected to DHT"
                                            : "Disconnected from DHT");
    }

    time = get_time();

    // Wait 1 million ticks then reconnect if needed and write save
    if (time - last_save >= (uint64_t)10 * 1000 * 1000 * 1000) {
      last_save = time;

      if (connection == TOX_CONNECTION_NONE) {
        do_bootstrap();
      }

      if (save_needed || (time - last_save >= (uint)100 * 1000 * 1000 * 1000)) {
        write_save();
      }
    }

    uint32_t interval = tox_iteration_interval(tox);
    usleep(1000 * MIN(interval, MAX_ITERATION_TIME));
  }

  if (loop != LOOP_SUSPEND) {
    toxav_thread.join();
  }

  uint64_t event;
  ssize_t unused;
  switch (loop) {
  case LOOP_RUN:
    Q_ASSERT(false);
    break;
  case LOOP_FINISH:
    qDebug() << "exiting…";

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
    have_password = next_have_password;
    write_default_profile();
    tox_thread();
    break;
  case LOOP_SUSPEND:
    unused = read(events, &event, sizeof(event));
    if (event == LOOP_FINISH)
      return;
    qDebug() << "read" << event << ", resuming thread";
    tox_loop();
    break;
  case LOOP_STOP:
  case LOOP_NOT_LOGGED_IN:
    killall_tox();
    unused = read(events, &event, sizeof(event));
    if (event == LOOP_FINISH)
      return;
    qDebug() << "read" << event << ", starting thread with profile"
             << profile_name << "save file" << tox_save_file();
    write_default_profile();
    tox_thread();
    break;
  }
  (void)unused;
}

void Cyanide::toxav_thread() {
  return;
  while (loop == LOOP_RUN || loop == LOOP_SUSPEND) {
    toxav_iterate(toxav);
    usleep(toxav_iteration_interval(toxav));
  }
  switch (loop) {
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
  case LOOP_NOT_LOGGED_IN:
    Q_ASSERT(false);
    break;
  }
}

void Cyanide::killall_tox() {
  qDebug() << "updating saved file transfers";
  /* for(auto fit = friends.begin(); fit != friends.end(); fit++) { */
  for (auto &entry : friends) {
    Friend &f = entry.second;
    for (auto const &entry : f.files) {
      // entry.first is the toxcore file number
      // entry.second is the corresponding message id
      if (entry.second >= 0) {
        history.update_file(*f.messages.at(entry.second).ft);
      }
    }
  }

  toxav_kill(toxav);
  kill_tox();
}

void Cyanide::kill_tox() {
  TOX_ERR_FRIEND_ADD error;

  /* re-add all blocked friends */
  for (auto &entry : friends) {
    Friend &f = entry.second;

    if (f.blocked) {
      tox_friend_add_norequest(tox, f.public_key, &error);

      if (error == TOX_ERR_FRIEND_ADD_MALLOC) {
        qDebug() << "memory allocation failure";
      } else {
        f.blocked = false;
      }
    }
  }

  write_save();

  tox_kill(tox);
}

/* bootstrap to dht with bootstrap_nodes */
void Cyanide::do_bootstrap() {
  TOX_ERR_BOOTSTRAP error;
  static unsigned int j = 0;

  if (j == 0)
    j = rand();

  int i = 0;
  while (i < 4) {
    struct bootstrap_node *d = &bootstrap_nodes[j % countof(bootstrap_nodes)];
    tox_bootstrap(tox, d->address, d->port, d->key, &error);
    i++;
    j++;
  }
}

void Cyanide::load_defaults() {
  TOX_ERR_SET_INFO error;

  uint8_t *name = (uint8_t *)DEFAULT_NAME.toUtf8().data(),
          *status = (uint8_t *)DEFAULT_STATUS.toUtf8().data();
  uint16_t name_len = DEFAULT_NAME.toUtf8().size(),
           status_len = DEFAULT_STATUS.toUtf8().size();

  self.name = DEFAULT_NAME;
  self.status_message = DEFAULT_STATUS;

  tox_self_set_name(tox, name, name_len, &error);
  tox_self_set_status_message(tox, status, status_len, &error);

  emit signal_friend_name(SELF_FRIEND_NUMBER, nullptr);
  emit signal_friend_status_message(SELF_FRIEND_NUMBER);
  save_needed = true;
}

void Cyanide::write_save() {
  if (!is_logged_in()) {
    qDebug() << "attempting to write save while not logged in";
    return;
  }

  void *data;
  uint32_t size;

  size = tox_get_savedata_size(tox);
  data = malloc(size);
  tox_get_savedata(tox, (uint8_t *)data);

  if (have_password) {
    qDebug() << "writing encrypted save";
    uint8_t *encrypted =
        (uint8_t *)malloc(size + TOX_PASS_ENCRYPTION_EXTRA_LENGTH);
    if (!tox_pass_key_encrypt(tox_pass_key, (const uint8_t *)data, size,
                              encrypted, nullptr)) {
      qDebug() << "encryption failed, not writing save";
      free(data);
      free(encrypted);
      return;
    }
    free(data);
    data = encrypted;
    size = size + TOX_PASS_ENCRYPTION_EXTRA_LENGTH;
  } else {
    qDebug() << "writing save";
  }

  QDir().mkpath(TOX_DATA_DIR);
  QDir().mkpath(TOX_AVATAR_DIR);

  QSaveFile file{tox_save_file()};
  if (!file.open(QIODevice::WriteOnly)) {
    qDebug() << "failed to open save file";
  }

  file.write((const char *)data, size);

  file.commit();

  save_needed = false;
  free(data);
}

void Cyanide::load_tox_data() {
  TOX_ERR_FRIEND_QUERY error;
  size_t length;
  size_t nfriends = tox_self_get_friend_list_size(tox);
  qDebug() << "Loading" << nfriends << "friends…";

  for (size_t i = 0; i < nfriends; i++) {
    Friend f = *new Friend();

    tox_friend_get_public_key(tox, i, f.public_key,
                              (TOX_ERR_FRIEND_GET_PUBLIC_KEY *)&error);

    uint8_t hex_id[2 * TOX_PUBLIC_KEY_SIZE];
    public_key_to_string((char *)hex_id, (char *)f.public_key);
    QString public_key{utf8_to_qstr(hex_id, 2 * TOX_PUBLIC_KEY_SIZE)};
    settings.add_friend_if_not_exists(public_key);
    QByteArray saved_hash{settings.get_friend_avatar_hash(public_key)};
    memcpy(f.avatar_hash, saved_hash.constData(), TOX_HASH_LENGTH);

    length = tox_friend_get_name_size(tox, i, &error);
    uint8_t name[length];
    if (!tox_friend_get_name(tox, i, name, &error)) {
      if (error == TOX_ERR_FRIEND_QUERY_FRIEND_NOT_FOUND)
        qDebug() << "Friend not found" << i;
    }
    f.name = utf8_to_qstr(name, length);

    length = tox_friend_get_status_message_size(tox, i, &error);
    uint8_t status_message[length];
    if (!tox_friend_get_status_message(tox, i, status_message, &error)) {
      if (error == TOX_ERR_FRIEND_QUERY_FRIEND_NOT_FOUND)
        qDebug() << "Friend not found" << i;
    }
    f.status_message = utf8_to_qstr(status_message, length);

    uint32_t fid = add_friend(&f);
    /* TODO queue all paused transfers for resuming */
    history.load_messages(get_friend_public_key(fid), friends[fid].messages);
  }
  emit signal_load_messages();

  length = tox_self_get_name_size(tox);
  uint8_t name[length];
  tox_self_get_name(tox, name);
  self.name = utf8_to_qstr(name, length);

  length = tox_self_get_status_message_size(tox);
  uint8_t status_message[length];
  tox_self_get_status_message(tox, status_message);
  self.status_message = utf8_to_qstr(status_message, length);

  emit signal_friend_name(SELF_FRIEND_NUMBER, nullptr);
  emit signal_friend_status_message(SELF_FRIEND_NUMBER);
  save_needed = true;
}

uint32_t Cyanide::add_friend(Friend *f) {
  uint32_t fid = next_friend_number();
  friends[fid] = *f;
  return fid;
}

/* find out the friend number that toxcore will assign when using
 * tox_friend_add() and tox_friend_add_norequest() */
uint32_t Cyanide::next_friend_number() const {
  uint32_t fid;
  for (fid = 0; fid < UINT32_MAX; fid++) {
    if (friends.count(fid) == 0 || friends.at(fid).blocked)
      break;
  }
  return fid;
}

uint32_t Cyanide::next_but_one_unoccupied_friend_number() const {
  int count = 0;
  uint32_t fid;
  for (fid = 0; fid < UINT32_MAX; fid++) {
    if (friends.count(fid) == 0 && count++)
      break;
  }
  return fid;
}

void Cyanide::relocate_blocked_friend() {
  uint32_t from = next_friend_number();

  if (friends.count(from) == 1 && friends[from].blocked) {
    uint32_t to = next_but_one_unoccupied_friend_number();
    friends[to] = friends[from];
    friends.erase(from);
    emit signal_friend_added(to);
  }
}

void Cyanide::add_message(uint32_t fid, Message &message) {
  friends.at(fid).messages.append(message);
  uint32_t mid = friends[fid].messages.size() - 1;

  if (message.ft != nullptr)
    friends[fid].files[message.ft->file_number] = mid;

  if (settings.get("keep-history") == "true") {
    history.add_message(get_friend_public_key(fid), message);
  }

  emit signal_friend_message(fid, mid, message.type);
}

void Cyanide::set_callbacks() {
  tox_callback_friend_request(tox, callback_friend_request);
  tox_callback_friend_message(tox, callback_friend_message);
  tox_callback_friend_name(tox, callback_friend_name);
  tox_callback_friend_status_message(tox, callback_friend_status_message);
  tox_callback_friend_status(tox, callback_friend_status);
  tox_callback_friend_typing(tox, callback_friend_typing);
  tox_callback_friend_read_receipt(tox, callback_friend_read_receipt);
  tox_callback_friend_connection_status(tox, callback_friend_connection_status);

  tox_callback_file_recv(tox, callback_file_recv);
  tox_callback_file_recv_chunk(tox, callback_file_recv_chunk);
  tox_callback_file_recv_control(tox, callback_file_recv_control);
  tox_callback_file_chunk_request(tox, callback_file_chunk_request);
}

void Cyanide::set_av_callbacks() {
  toxav_callback_call(toxav, callback_call, this);
  toxav_callback_call_state(toxav, callback_call_state, this);
  // toxav_callback_audio_bit_rate_status(toxav, callback_audio_bit_rate_status,
  // this);
  // toxav_callback_video_bit_rate_status(toxav, callback_video_bit_rate_status,
  // this);
  toxav_callback_audio_receive_frame(toxav, callback_audio_receive_frame, this);
  toxav_callback_video_receive_frame(toxav, callback_video_receive_frame, this);
}

void Cyanide::send_typing_notification(int fid, bool typing) {
  TOX_ERR_SET_TYPING error;
  if (settings.get("send-typing-notifications") == "true")
    tox_self_set_typing(tox, fid, typing, &error);
}

QString Cyanide::send_friend_request(QString id_str, QString msg_str) {
  /* honor the Tox URI scheme */
  if (id_str.startsWith("tox:"))
    id_str = id_str.remove(0, 4);

  size_t id_len = qstrlen(id_str);
  char id[id_len];
  qstr_to_utf8((uint8_t *)id, id_str);

  if (msg_str.isEmpty())
    msg_str = DEFAULT_FRIEND_REQUEST_MESSAGE;

  size_t msg_len = qstrlen(msg_str);
  uint8_t msg[msg_len];
  qstr_to_utf8(msg, msg_str);

  uint8_t address[TOX_ADDRESS_SIZE];
  if (!string_to_address(address, (uint8_t *)id)) {
    /* not a regular id, try DNS discovery */
    if (!dns_lookup(address, id_str))
      return tr("Error: Invalid Tox ID");
  }

  QString errmsg{send_friend_request_id(address, msg, msg_len)};
  if (errmsg == "") {
    Friend *f = new Friend((const uint8_t *)address, id_str, "");
    emit signal_friend_added(add_friend(f));
    char hex_address[2 * TOX_ADDRESS_SIZE + 1];
    address_to_string(hex_address, (char *)address);
    hex_address[2 * TOX_ADDRESS_SIZE] = '\0';
    settings.add_friend(hex_address);
  }

  return errmsg;
}

/* */
QString Cyanide::send_friend_request_id(const uint8_t *id, const uint8_t *msg,
                                        size_t msg_length) {
  TOX_ERR_FRIEND_ADD error;
  relocate_blocked_friend();
  uint32_t fid = tox_friend_add(tox, id, msg, msg_length, &error);
  switch (error) {
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
    return tr("Error: Invalid Tox ID (bad NoSpam value)");
  case TOX_ERR_FRIEND_ADD_MALLOC:
    return tr("Error: No memory");
  default:
    Q_ASSERT(fid != UINT32_MAX);
    Q_ASSERT(fid == friends.size() - 1);
    return "";
  }
  (void)fid;
}

std::vector<QString> split_message(QString &rest) {
  std::vector<QString> messages;
  QString chunk;
  int last_space;

  while (rest != "") {
    chunk = rest.left(TOX_MAX_MESSAGE_LENGTH);
    last_space = chunk.lastIndexOf(" ");

    if (chunk.size() == TOX_MAX_MESSAGE_LENGTH) {
      if (last_space > 0) {
        chunk = rest.left(last_space);
      }
    }
    rest = rest.right(rest.size() - chunk.size());

    messages.push_back(chunk);
  }
  return messages;
}

QString Cyanide::send_friend_message(int fid, QString message) {
  TOX_ERR_FRIEND_SEND_MESSAGE error;
  QString errmsg{""};

  if (friends[fid].blocked)
    /* TODO show this somehow in the UI */
    return "Friend is blocked, unblock to connect";

  TOX_MESSAGE_TYPE type = TOX_MESSAGE_TYPE_NORMAL;

  std::vector<QString> messages = split_message(message);

  for (QString const &msg_str : messages) {

    size_t msg_len = qstrlen(msg_str);
    uint8_t msg[msg_len];
    qstr_to_utf8(msg, msg_str);

    uint32_t message_id =
        tox_friend_send_message(tox, fid, type, msg, msg_len, &error);
    qDebug() << "message ID:" << message_id;

    switch (error) {
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

    if (errmsg != "")
      return errmsg;

    Message m;
    m.type = Message_Type::Normal;
    m.author = true;
    m.text = msg_str;
    m.timestamp = QDateTime::currentDateTime();
    m.ft = nullptr;

    add_message(fid, m);
  }

  return errmsg;
}

bool Cyanide::accept_friend_request(int fid) {
  TOX_ERR_FRIEND_ADD error;
  relocate_blocked_friend();
  if (tox_friend_add_norequest(tox, friends[fid].public_key, &error) ==
      UINT32_MAX) {
    qDebug() << "could not add friend";
    return false;
  }

  friends[fid].accepted = true;
  save_needed = true;
  return true;
}

void Cyanide::remove_friend(int fid) {
  TOX_ERR_FRIEND_DELETE error;
  if (friends[fid].accepted && !tox_friend_delete(tox, fid, &error)) {
    qDebug() << "Failed to remove friend";
    return;
  }
  save_needed = true;
  settings.remove_friend(get_friend_public_key(fid));
  friends.erase(fid);
}

void Cyanide::load_history(int fid, QDateTime const from,
                           QDateTime const to) {
  free_friend_messages(friends[fid]);
  history.load_messages(get_friend_public_key(fid), friends[fid].messages, from,
                        to);
  emit signal_load_messages();
}

void Cyanide::clear_history(int fid) {
  history.clear_messages(get_friend_public_key(fid));
  free_friend_messages(friends[fid]);
  emit signal_load_messages();
}

QDateTime Cyanide::null_date() const { return QDateTime(); }

/* setters and getters */

QString Cyanide::get_profile_name() { return profile_name; }

QString Cyanide::set_profile_name(QString name) {
  QString old_name{tox_save_file()};
  QString new_name{tox_save_file(name)};

  QFile old_file{old_name};
  QFile new_file{new_name};

  if (new_file.exists()) {
    return tr("Error: File exists");
  } else {
    save_needed = false;
    /* possible race condition? */
    old_file.rename(new_name);
    QFile::rename(CYANIDE_DATA_DIR + profile_name.replace('/', '_') + ".sqlite",
                  CYANIDE_DATA_DIR + name.replace('/', '_') + ".sqlite");
    profile_name = name;
    save_needed = true;
    return "";
  }
}

QList<int> Cyanide::get_friend_numbers() const {
  QList<int> list{};
  int away = 0, busy = 0;

  for (auto const &entry : friends) {
    int fid = entry.first;
    Friend const &f = entry.second;
    if (fid < 0) {
      qWarning() << "friend ID < 0";
      continue;
    }
    if (f.connection_status == TOX_CONNECTION_NONE) {
      list.append(fid);
    } else if (f.user_status == TOX_USER_STATUS_BUSY) {
      list.insert(busy++, fid);
    } else if (f.user_status == TOX_USER_STATUS_AWAY) {
      list.insert(away++, fid);
      busy++;
    } else {
      list.insert(0, fid);
      away++;
      busy++;
    }
  }

  return list;
}

QList<int> Cyanide::get_message_numbers(int fid) const {
  QList<int> message_numbers{};
  Friend const &f = friends.count(fid) ? friends.at(fid) : Friend{};
  auto messages = f.messages;
  for (int i = 0; i < messages.size(); i++)
    message_numbers.append(i);
  return message_numbers;
}

void Cyanide::set_friend_activity(int fid, bool status) {
  Q_ASSERT(fid != SELF_FRIEND_NUMBER);
  friends[fid].activity = status;
  emit signal_friend_activity(fid);
}

bool Cyanide::get_friend_blocked(int fid) const {
  Friend const &f = fid == SELF_FRIEND_NUMBER
                        ? self
                        : friends.count(fid) ? friends.at(fid) : Friend();
  return f.blocked;
}

void Cyanide::set_friend_blocked(int fid, bool block) {
  Q_ASSERT(fid != SELF_FRIEND_NUMBER);
  int error;

  Friend *f = &friends[fid];

  if (block) {
    if (tox_friend_delete(tox, fid, (TOX_ERR_FRIEND_DELETE *)&error)) {
      f->connection_status = TOX_CONNECTION_NONE;
    } else {
      qDebug() << "Failed to remove friend";
    }
  } else {
    int friend_number = tox_friend_add_norequest(tox, f->public_key,
                                                 (TOX_ERR_FRIEND_ADD *)&error);
    Q_ASSERT(friend_number == fid);
    (void)friend_number;
  }
  friends[fid].blocked = block;
  emit signal_friend_blocked(fid, block);
  emit signal_friend_connection_status(fid, f->connection_status !=
                                                TOX_CONNECTION_NONE);
}

void Cyanide::set_self_name(QString const name) {
  bool success;
  TOX_ERR_SET_INFO error;
  self.name = name;

  size_t length = qstrlen(name);
  uint8_t tmp[length];
  qstr_to_utf8(tmp, name);
  success = tox_self_set_name(tox, tmp, length, &error);
  Q_ASSERT(success);

  save_needed = true;
  emit signal_friend_name(SELF_FRIEND_NUMBER, nullptr);
  (void)success;
}

void Cyanide::set_self_status_message(QString const status_message) {
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
  (void)success;
}

int Cyanide::get_self_user_status() const {
  switch (self.user_status) {
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

void Cyanide::set_self_user_status(int status) {
  switch (status) {
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

QString Cyanide::set_self_avatar(QString const new_avatar) {
  bool success;
  QString public_key{get_friend_public_key(SELF_FRIEND_NUMBER)};
  QString old_avatar{TOX_AVATAR_DIR + public_key + QString(".png")};

  if (new_avatar == "") {
    /* remove the avatar */
    success = QFile::remove(old_avatar);
    if (!success) {
      qDebug() << "Failed to remove avatar file";
    } else {
      memset(self.avatar_hash, 0, TOX_HASH_LENGTH);
      emit signal_avatar_change(SELF_FRIEND_NUMBER);
      send_new_avatar();
    }
    return "";
  }

  uint32_t size;
  uint8_t *data = (uint8_t *)file_raw(new_avatar.toUtf8().data(), &size);
  if (data == nullptr) {
    return tr("File not found: ") + new_avatar;
  }

  if (size > MAX_AVATAR_SIZE) {
    free(data);
    return tr("Avatar too large. Maximum size: 64KiB");
  }
  uint8_t previous_hash[TOX_HASH_LENGTH];
  memcpy(previous_hash, self.avatar_hash, TOX_HASH_LENGTH);
  success = tox_hash(self.avatar_hash, data, size);
  Q_ASSERT(success);
  FILE *out = fopen(old_avatar.toUtf8().data(), "wb");
  fwrite(data, size, 1, out);
  fclose(out);
  free(data);
  if (0 != memcmp(previous_hash, self.avatar_hash, TOX_HASH_LENGTH)) {
    QByteArray hash{(const char *)self.avatar_hash, TOX_HASH_LENGTH};
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

void Cyanide::send_new_avatar() {
  /* send the avatar to each connected friend */
  for (auto &entry : friends) {
    Friend &f = entry.second;
    if (f.connection_status != TOX_CONNECTION_NONE) {
      QString errmsg{send_avatar(entry.first)};
      if (errmsg != "")
        qDebug() << "Failed to send avatar. " << errmsg;
      else
        f.needs_avatar = false;
    } else {
      f.needs_avatar = true;
    }
  }
}

QString Cyanide::get_friend_name(int fid) const {
  Friend const &f = fid == SELF_FRIEND_NUMBER
                        ? self
                        : friends.count(fid) ? friends.at(fid) : Friend();
  return f.name;
}

QString Cyanide::get_friend_avatar(int fid) const {
  QString avatar{TOX_AVATAR_DIR + get_friend_public_key(fid) + QString(".png")};
  return QFile::exists(avatar) ? avatar : "qrc:/images/blankavatar";
}

QString Cyanide::get_friend_status_message(int fid) const {
  Friend const &f = fid == SELF_FRIEND_NUMBER
                        ? self
                        : friends.count(fid) ? friends.at(fid) : Friend();
  return f.status_message;
}

QString Cyanide::get_friend_status_icon(int fid) const {
  Friend const &f = fid == SELF_FRIEND_NUMBER
                        ? self
                        : friends.count(fid) ? friends.at(fid) : Friend();
  return get_friend_status_icon(fid,
                                f.connection_status != TOX_CONNECTION_NONE);
}

QString Cyanide::get_friend_status_icon(int fid, bool online) const {
  QString url{"qrc:/images/"};
  Friend const &f = fid == SELF_FRIEND_NUMBER
                        ? self
                        : friends.count(fid) ? friends.at(fid) : Friend();

  if (!online) {
    url.append("offline");
  } else {
    switch (f.user_status) {
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

  if (f.activity)
    url.append("_notification");

  url.append("_2x");

  return url;
}

bool Cyanide::get_friend_connection_status(int fid) const {
  Friend const &f = fid == SELF_FRIEND_NUMBER
                        ? self
                        : friends.count(fid) ? friends.at(fid) : Friend();
  return f.connection_status != TOX_CONNECTION_NONE;
}

bool Cyanide::get_friend_accepted(int fid) const {
  Q_ASSERT(fid != SELF_FRIEND_NUMBER);
  Friend const &f = fid == SELF_FRIEND_NUMBER
                        ? self
                        : friends.count(fid) ? friends.at(fid) : Friend();
  return f.accepted;
}

QString Cyanide::get_friend_public_key(int fid) const {
  Friend const &f = fid == SELF_FRIEND_NUMBER
                        ? self
                        : friends.count(fid) ? friends.at(fid) : Friend();
  uint8_t hex_id[2 * TOX_PUBLIC_KEY_SIZE];
  public_key_to_string((char *)hex_id, (char *)f.public_key);
  return utf8_to_qstr(hex_id, 2 * TOX_PUBLIC_KEY_SIZE);
}

void Cyanide::set_random_nospam() {
  tox_self_set_nospam(tox, rand());
  tox_self_get_address(tox, self_address);
}

QString Cyanide::get_self_address() const {
  char hex_address[2 * TOX_ADDRESS_SIZE];
  address_to_string((char *)hex_address, (char *)self_address);
  return utf8_to_qstr(hex_address, 2 * TOX_ADDRESS_SIZE);
}

int Cyanide::get_message_type(int fid, int mid) const {
  Q_ASSERT(fid != SELF_FRIEND_NUMBER);
  Friend const &f = friends.count(fid) ? friends.at(fid) : Friend();
  return f.messages[mid].type;
}

QString Cyanide::get_message_text(int fid, int mid) const {
  Q_ASSERT(fid != SELF_FRIEND_NUMBER);
  Friend const &f = friends.count(fid) ? friends.at(fid) : Friend();
  return f.messages[mid].text;
}

QString Cyanide::get_message_html_escaped_text(int fid, int mid) const {
  return get_message_text(fid, mid).toHtmlEscaped();
}

QString Cyanide::get_message_rich_text(int fid, int mid) const {
  QString text = get_message_html_escaped_text(fid, mid);

  const QString email_chars{QRegExp::escape("A-Za-z0-9!#$%&'*+-/=?^_`{|}~.")};
  const QString url_chars{QRegExp::escape("A-Za-z0-9!#$%&'*+-/=?^_`{|}~")};
  const QString email_token{"[" + email_chars + "]+"};
  const QString url_token{"[" + url_chars + "]+"};

  /* match either protocol:address or email@domain or example.org */
  QRegExp rx{"(\\b[A-Za-z][A-Za-z0-9]*:[^\\s]+\\b"
             "|\\b" +
             email_token + "@" + email_token + "\\b"
                                               "|\\b" +
             url_token + "\\." + email_token + "\\b)"};
  QString repl{};
  int protocol;
  int pos = 0;

  while ((pos = rx.indexIn(text, pos)) != -1) {
    /* check whether the captured text has a protocol identifier */
    protocol = rx.cap(1).indexOf(QRegExp{"^[A-Za-z0-9]+:"}, 0);
    repl =
        "<a href=\"" +
        QString{(protocol != -1)
                    ? ""
                    : (rx.cap(1).indexOf("@") != -1) ? "mailto:" : "HTTPS:"} +
        rx.cap(1) + "\">" + rx.cap(1) + "</a>";
    text.replace(pos, rx.matchedLength(), repl);
    pos += repl.length();
  }

  QRegExp ry{"(^" + QRegExp::escape("&gt;") + "[^\n]*\n"
                                              "|^" +
             QRegExp::escape("&gt;") + "[^\n]*$)"};
  pos = 0;
  while ((pos = ry.indexIn(text, pos)) != -1) {
    repl = "<font color='lightgreen'>" + ry.cap(1) + "</font>";
    text.replace(pos, ry.matchedLength(), repl);
    pos += repl.length();
  }
  text.replace("\n", "<br>");

  return text;
}

QDateTime Cyanide::get_message_timestamp(int fid, int mid) const {
  Q_ASSERT(fid != SELF_FRIEND_NUMBER);
  Friend const &f = friends.count(fid) ? friends.at(fid) : Friend();
  return f.messages[mid].timestamp;
}

bool Cyanide::get_message_author(int fid, int mid) const {
  Q_ASSERT(fid != SELF_FRIEND_NUMBER);
  Friend const &f = friends.count(fid) ? friends.at(fid) : Friend();
  return f.messages[mid].author;
}

int Cyanide::get_file_status(int fid, int mid) const {
  Q_ASSERT(fid != SELF_FRIEND_NUMBER);
  Friend const &f = friends.count(fid) ? friends.at(fid) : Friend();
  return f.messages[mid].ft->status;
}

QString Cyanide::get_file_link(int fid, int mid) const {
  Q_ASSERT(fid != SELF_FRIEND_NUMBER);
  Friend const &f = friends.count(fid) ? friends.at(fid) : Friend();
  QString filename{f.messages[mid].text};
  QString fullpath{"file://" + filename};
  return "<a href=\"" + fullpath.toHtmlEscaped() + "\">" +
         filename.right(filename.size() - 1 - filename.lastIndexOf("/"))
             .toHtmlEscaped() +
         "</a>";
}

int Cyanide::get_file_progress(int fid, int mid) const {
  Q_ASSERT(fid != SELF_FRIEND_NUMBER);
  Friend const &f = friends.count(fid) ? friends.at(fid) : Friend();
  File_Transfer *ft = f.messages[mid].ft;
  return ft->file_size == 0 ? 100 : 100 * ft->position / ft->file_size;
}
